<?php
// Template relay.
//
// The dev machine pushes packs UP here; the clients poll and pull them DOWN.
// That direction is the whole point. A playout machine opens no inbound port,
// the dev machine never has to reach the venue, and neither has to know where
// the other is. Both only have to reach this.
//
// Drop it on any PHP host, 7.4 or newer.
//
//   POST ?action=upload&pack=SEVILLE&path=calendar.html   body = the file bytes
//        send X-Content-Sha1 and the body is checked against it before it is stored
//   POST ?action=remove&pack=SEVILLE&path=calendar.html
//   GET  ?action=manifest[&pack=SEVILLE]                  what is here, with digests
//   GET  ?action=fetch&pack=SEVILLE&path=calendar.html    one file
//   GET  ?action=ping                                     is this a relay, and which
//   POST ?action=checkin                                  a client saying what it now has
//   GET  ?action=clients                                  who has checked in, and are they current
//
// TWO tokens, and they are deliberately not the same one. The dev machine holds
// the upload token; the clients hold the download token. A client that is stolen
// can then read what it was already going to install, and cannot put anything
// here for the other clients to fetch.

// ---- configuration -------------------------------------------------------

// Change both before this is reachable. A relay left on the shipped tokens is a
// relay anyone can write templates to, and a template is code that CasparCG runs.
define('UPLOAD_TOKEN',   'change-me-upload');
define('DOWNLOAD_TOKEN', 'change-me-download');

// Where packs are kept. Put it OUTSIDE the web root if the host allows it: then
// no URL reaches a template directly and the token is the only way in. If it has
// to sit beside this file, the .htaccess written below covers Apache. nginx and
// IIS need their own rule, so check that before trusting it.
define('STORAGE', __DIR__ . '/relay_data');

define('MAX_FILE_BYTES', 32 * 1024 * 1024);

// Named in ping, so an operator with a staging relay and a live one can tell
// which of them they just talked to.
define('RELAY_NAME', 'template relay');

// ---- older PHP -----------------------------------------------------------

// Shared hosting is where this will live, and shared hosting is often behind.
if (!function_exists('str_contains')) {
    function str_contains($haystack, $needle) {
        return $needle === '' || strpos($haystack, $needle) !== false;
    }
}
if (!function_exists('str_starts_with')) {
    function str_starts_with($haystack, $needle) {
        return strncmp($haystack, $needle, strlen($needle)) === 0;
    }
}
if (!function_exists('str_ends_with')) {
    function str_ends_with($haystack, $needle) {
        return $needle === '' || substr($haystack, -strlen($needle)) === $needle;
    }
}

// ---- plumbing ------------------------------------------------------------

header('Content-Type: application/json');

function reply($status, array $body) {
    http_response_code($status);
    echo json_encode($body, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
    exit;
}

/** Constant-time, so a wrong token cannot be found one character at a time. */
function tokenIs($expected) {
    $given = isset($_SERVER['HTTP_X_RELAY_TOKEN']) ? $_SERVER['HTTP_X_RELAY_TOKEN'] : '';
    return $given !== '' && hash_equals($expected, $given);
}

function requireToken($expected) {
    if (!tokenIs($expected)) {
        reply(401, array('error' => 'Missing or wrong X-Relay-Token'));
    }
}

/**
 * One path segment, by the same rule the client enforces on the far end. A relay
 * that accepts a path the client will refuse is a relay that stores files nobody
 * can install, so the two rules are kept identical on purpose.
 */
function safeSegment($segment) {
    if ($segment === '' || $segment === '.' || $segment === '..') {
        return false;
    }
    if (!preg_match('/^[A-Za-z0-9._\-() +]+$/', $segment)) {
        return false;
    }
    // Windows strips trailing dots and spaces, so "a. " and "a" would arrive at a
    // client as one file under two names.
    if (str_ends_with($segment, '.') || str_ends_with($segment, ' ')) {
        return false;
    }

    // Every client is Windows, where these are device names rather than files, with
    // or without an extension. Opening one writes to a console or a port instead of
    // to disk, so it is refused here rather than discovered there.
    $stem = strtoupper(explode('.', $segment)[0]);
    $devices = array('CON', 'PRN', 'AUX', 'NUL',
                     'COM1', 'COM2', 'COM3', 'COM4', 'COM5', 'COM6', 'COM7', 'COM8', 'COM9',
                     'LPT1', 'LPT2', 'LPT3', 'LPT4', 'LPT5', 'LPT6', 'LPT7', 'LPT8', 'LPT9');

    return !in_array($stem, $devices, true);
}

function safeRelativePath($path) {
    $path = str_replace('\\', '/', $path);
    if ($path === '' || str_starts_with($path, '/')) {
        return false;
    }

    $segments = explode('/', $path);
    if (count($segments) > 8) {
        return false;   // nothing in a pack is buried that deep
    }

    foreach ($segments as $segment) {
        if (!safeSegment($segment)) {
            return false;
        }
    }
    return true;
}

/**
 * Never relayed, in either direction. Each client owns its own API key, its local
 * flag and its Sheets panel buttons; a relay that carried these would hand one
 * machine's settings to every other machine on the next poll.
 */
function isProtected($relativePath) {
    if (str_contains($relativePath, '/')) {
        return false;   // a webcg/project.js is template code, not the connection file
    }
    $lower = strtolower($relativePath);
    return $lower === 'project.js' || $lower === 'extensions.json';
}

function packDir($pack) {
    return STORAGE . '/' . $pack;
}

/**
 * Resolve a path inside a pack, or false. realpath collapses traversal and follows
 * symlinks, so this rather than the pattern match is what actually decides that a
 * request stays inside its own pack.
 */
function resolveInPack($pack, $path) {
    $real = realpath(packDir($pack) . '/' . $path);
    $root = realpath(packDir($pack));
    if ($real === false || $root === false) {
        return false;
    }
    return str_starts_with($real, $root . DIRECTORY_SEPARATOR) ? $real : false;
}

/** Every file under a pack, with the digest a client compares against. */
function describePack($pack) {
    $root = packDir($pack);
    if (!is_dir($root)) {
        return array('name' => $pack, 'version' => '', 'bytes' => 0, 'files' => array());
    }

    $files = array();
    $version = 0;
    $total = 0;

    $walker = new RecursiveIteratorIterator(
        new RecursiveDirectoryIterator($root, FilesystemIterator::SKIP_DOTS)
    );

    foreach ($walker as $file) {
        if (!$file->isFile() || str_ends_with($file->getFilename(), '.part')) {
            continue;   // a .part is an upload still in flight
        }

        $relative = str_replace('\\', '/', substr($file->getPathname(), strlen($root) + 1));
        $files[] = array(
            'path'  => $relative,
            'bytes' => $file->getSize(),
            'sha1'  => sha1_file($file->getPathname()),
        );
        $total += $file->getSize();
        $version = max($version, $file->getMTime());
    }

    usort($files, function ($a, $b) { return strcmp($a['path'], $b['path']); });

    return array(
        'name' => $pack,
        // The newest file in the pack. A client that remembers this can tell from a
        // single small read whether there is anything here worth looking at.
        'version' => $version ? gmdate('c', $version) : '',
        'bytes'   => $total,
        'files'   => $files,
    );
}

function allPacks() {
    if (!is_dir(STORAGE)) {
        return array();
    }

    $packs = array();
    foreach (scandir(STORAGE) as $name) {
        // Anything dotted is this relay's own bookkeeping, not a pack. Without this
        // the check-in folder would be offered to every client as something to
        // install, which is exactly the wrong shape of mistake.
        if ($name === '' || $name[0] === '.' || !is_dir(STORAGE . '/' . $name)) {
            continue;
        }
        $packs[] = $name;
    }
    sort($packs);
    return $packs;
}

// ---- who has picked things up ---------------------------------------------

// Uploading to a relay is uploading into a void: the dev machine hears that the
// relay took the file and never learns whether the venue actually pulled it. So
// clients say so, and this is where that is kept.

define('CLIENT_DIR', STORAGE . '/.clients');

// Enough for any estate this is for, and a bound on what a misbehaving client can
// fill the disk with. Updating an existing record is always allowed.
define('MAX_CLIENTS', 500);

/** A client name reduced to something safe to use as a filename. */
function clientId($name) {
    $id = preg_replace('/[^A-Za-z0-9._-]/', '_', $name);
    $id = trim($id, '._-');
    return $id === '' ? '' : substr($id, 0, 64);
}

function allClients() {
    if (!is_dir(CLIENT_DIR)) {
        return array();
    }

    $clients = array();
    foreach (scandir(CLIENT_DIR) as $name) {
        if (!str_ends_with($name, '.json')) {
            continue;
        }
        $record = json_decode(file_get_contents(CLIENT_DIR . '/' . $name), true);
        if (is_array($record)) {
            $clients[] = $record;
        }
    }

    usort($clients, function ($a, $b) {
        return strcmp(isset($a['host']) ? $a['host'] : '', isset($b['host']) ? $b['host'] : '');
    });
    return $clients;
}

// ---- storage, and keeping it out of the web ------------------------------

if (!is_dir(STORAGE) && !mkdir(STORAGE, 0755, true)) {
    reply(500, array('error' => 'Cannot create the storage folder'));
}

// Only meaningful on Apache, and only when storage sits under the web root.
$guard = STORAGE . '/.htaccess';
if (!file_exists($guard)) {
    file_put_contents($guard, "Require all denied\n<IfModule !mod_authz_core.c>\n  Deny from all\n</IfModule>\n");
}

// ---- routes --------------------------------------------------------------

$action = isset($_GET['action']) ? $_GET['action'] : '';
$pack   = isset($_GET['pack'])   ? $_GET['pack']   : '';
$path   = isset($_GET['path'])   ? $_GET['path']   : '';

if ($pack !== '' && !safeSegment($pack)) {
    reply(400, array('error' => 'That is not a usable pack name'));
}

switch ($action) {

    case 'ping':
        // Either token answers: both ends need to be able to ask "is this on, and is
        // the token I hold the right one" without writing anything.
        if (!tokenIs(UPLOAD_TOKEN) && !tokenIs(DOWNLOAD_TOKEN)) {
            reply(401, array('error' => 'Missing or wrong X-Relay-Token'));
        }
        reply(200, array(
            'relay'     => RELAY_NAME,
            'packs'     => count(allPacks()),
            'canUpload' => tokenIs(UPLOAD_TOKEN),
            'maxBytes'  => MAX_FILE_BYTES,
            'at'        => gmdate('c'),
        ));

    case 'checkin':
        // A client's own token. It reports about itself and nothing else, and the
        // record it writes is the one named after it.
        requireToken(DOWNLOAD_TOKEN);

        $sent = json_decode(file_get_contents('php://input'), true);
        if (!is_array($sent)) {
            reply(400, array('error' => 'Expected a JSON body'));
        }

        $id = clientId(isset($sent['host']) ? $sent['host'] : '');
        if ($id === '') {
            reply(400, array('error' => 'Need a usable host name'));
        }

        // Only the fields this understands are kept. A client cannot store arbitrary
        // content here by adding keys to the body.
        $record = array(
            'host'   => substr((string) $sent['host'], 0, 128),
            'os'     => isset($sent['os']) ? substr((string) $sent['os'], 0, 128) : '',
            'packs'  => array(),
            'seenAt' => gmdate('c'),
        );

        if (isset($sent['packs']) && is_array($sent['packs'])) {
            foreach ($sent['packs'] as $name => $version) {
                if (!is_string($name) || !safeSegment($name) || count($record['packs']) >= 100) {
                    continue;
                }
                $record['packs'][$name] = substr((string) $version, 0, 64);
            }
        }

        $file = CLIENT_DIR . '/' . $id . '.json';
        if (!is_dir(CLIENT_DIR) && !mkdir(CLIENT_DIR, 0755, true)) {
            reply(500, array('error' => 'Cannot record check-ins'));
        }

        // A new name is capped; an existing one may always update itself, so a full
        // relay never stops an estate that is already known from reporting.
        if (!file_exists($file) && count(allClients()) >= MAX_CLIENTS) {
            reply(429, array('error' => 'This relay is already tracking as many clients as it will'));
        }

        $temporary = $file . '.part';
        if (file_put_contents($temporary, json_encode($record)) === false || !rename($temporary, $file)) {
            @unlink($temporary);
            reply(500, array('error' => 'Cannot record that check-in'));
        }

        reply(200, array('ok' => true, 'host' => $record['host'], 'at' => $record['seenAt']));

    case 'clients':
        // The dev machine's token: this is a view of the whole estate, which is not
        // something one client should be able to enumerate with its own token.
        requireToken(UPLOAD_TOKEN);

        $current = array();
        foreach (allPacks() as $name) {
            $described = describePack($name);
            $current[$name] = $described['version'];
        }

        $out = array();
        foreach (allClients() as $client) {
            $behind = array();
            foreach ($current as $name => $version) {
                // Only packs the client actually reported. A client that does not
                // follow a pack is not behind on it.
                if (isset($client['packs'][$name]) && $client['packs'][$name] !== $version) {
                    $behind[] = $name;
                }
            }

            $client['behind'] = $behind;
            $client['current'] = empty($behind);
            $out[] = $client;
        }

        reply(200, array('clients' => $out, 'packs' => $current, 'at' => gmdate('c')));

    case 'manifest':
        requireToken(DOWNLOAD_TOKEN);
        if ($pack !== '') {
            reply(200, describePack($pack));
        }

        $packs = array();
        foreach (allPacks() as $name) {
            $packs[] = describePack($name);
        }
        reply(200, array('packs' => $packs, 'at' => gmdate('c')));

    case 'fetch':
        requireToken(DOWNLOAD_TOKEN);
        if ($pack === '' || !safeRelativePath($path)) {
            reply(400, array('error' => 'Need a pack and a usable path'));
        }

        $real = resolveInPack($pack, $path);
        if ($real === false || !is_file($real)) {
            reply(404, array('error' => 'No such file'));
        }

        header('Content-Type: application/octet-stream');
        header('Content-Length: ' . filesize($real));
        header('X-Relay-Sha1: ' . sha1_file($real));
        readfile($real);
        exit;

    case 'upload':
        requireToken(UPLOAD_TOKEN);
        if ($pack === '' || !safeRelativePath($path)) {
            reply(400, array('error' => 'Need a pack and a usable path'));
        }
        if (isProtected($path)) {
            reply(409, array('error' => 'That file belongs to each client and is never relayed'));
        }

        $body = file_get_contents('php://input');
        if ($body === false) {
            reply(400, array('error' => 'No body'));
        }
        if (strlen($body) > MAX_FILE_BYTES) {
            reply(413, array('error' => 'Larger than this relay accepts'));
        }

        // The dev machine says what it sent. If that is not what arrived, the file
        // is not stored: a template that changed in transit is the last thing to
        // hand out to every client on their next poll. Optional, so an older pusher
        // still works, but the current one always sends it.
        $claimed = isset($_SERVER['HTTP_X_CONTENT_SHA1']) ? strtolower(trim($_SERVER['HTTP_X_CONTENT_SHA1'])) : '';
        if ($claimed !== '' && $claimed !== sha1($body)) {
            reply(422, array('error' => 'The body does not match the digest the sender claimed',
                             'claimed' => $claimed, 'actual' => sha1($body)));
        }

        $destination = packDir($pack) . '/' . $path;
        $folder = dirname($destination);
        if (!is_dir($folder) && !mkdir($folder, 0755, true)) {
            reply(500, array('error' => 'Cannot create that folder'));
        }

        // Written beside and moved into place, so a client polling mid-upload gets
        // either the old file or the new one and never half of either.
        $temporary = $destination . '.part';
        if (file_put_contents($temporary, $body) === false || !rename($temporary, $destination)) {
            @unlink($temporary);
            reply(500, array('error' => 'Cannot write that file'));
        }

        reply(200, array('ok' => true, 'pack' => $pack, 'path' => $path,
                         'bytes' => strlen($body), 'sha1' => sha1($body)));

    case 'remove':
        requireToken(UPLOAD_TOKEN);
        if ($pack === '' || !safeRelativePath($path)) {
            reply(400, array('error' => 'Need a pack and a usable path'));
        }

        $real = resolveInPack($pack, $path);
        if ($real === false || !is_file($real)) {
            reply(404, array('error' => 'No such file'));
        }

        // Only ever removes it from the relay. What a client already installed stays
        // installed: taking a template off a machine that may be on air is not a
        // decision this tool gets to make from the other side of the internet.
        if (!unlink($real)) {
            reply(500, array('error' => 'Cannot remove that file'));
        }

        reply(200, array('ok' => true, 'pack' => $pack, 'removed' => $path,
                         'note' => 'Removed here only. Clients keep what they already installed.'));

    default:
        reply(400, array(
            'error'   => 'Unknown action',
            'actions' => array('ping', 'manifest', 'fetch', 'upload', 'remove', 'checkin', 'clients'),
        ));
}
