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
//   GET  ?action=selftest                                 is this relay set up safely
//   GET  ?action=assignments                              which client gets which packs
//   POST ?action=assignments                              set that (upload token)
//
// TWO tokens, and they are deliberately not the same one. The dev machine holds
// the upload token; the clients hold the download token. A client that is stolen
// can then read what it was already going to install, and cannot put anything
// here for the other clients to fetch.

// ---- keep PHP's own output out of the answers ----------------------------

// Every answer here is JSON that a client parses. A host with display_errors on
// will print a deprecation notice, or a warning about a request that exceeded
// post_max_size, straight into the response body before this file runs a single
// line. That turns valid JSON into garbage with HTML in front of it, and every
// client fails on it for a reason no one would guess from the symptom.
//
// Errors still go to the host's log, which is where they belong.
//
// This is not the whole fix. Some warnings are emitted at request startup, before
// this file runs a single statement, and nothing written here can catch one. The
// .user.ini and .htaccess shipped beside this file cover that case, and they have
// to be deployed with it. The self-test says so if they were not.
$GLOBALS['relayDisplayErrorsWasOn'] = ini_get('display_errors');

ini_set('display_errors', '0');
ini_set('log_errors', '1');
error_reporting(E_ALL);

// A warning emitted before this point is already in the buffer, so it is thrown
// away rather than sent. Nothing this file deliberately outputs is lost: every
// reply happens after here.
if (ob_get_level() === 0) {
    ob_start();
}

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
    // Anything already in the buffer is PHP's, not ours, and it would corrupt the
    // JSON. Dropped rather than sent, so a client always gets something it can parse.
    if (ob_get_level() > 0) {
        ob_clean();
    }

    http_response_code($status);
    header('Content-Type: application/json');
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

/** "8M" and "512K" and "1G" as a number of bytes. */
function iniBytes($value) {
    $value = trim((string) $value);
    if ($value === '') {
        return 0;
    }

    $number = (float) $value;
    switch (strtolower(substr($value, -1))) {
        case 'g': return (int) ($number * 1024 * 1024 * 1024);
        case 'm': return (int) ($number * 1024 * 1024);
        case 'k': return (int) ($number * 1024);
    }
    return (int) $number;
}

/**
 * The largest body this relay can actually receive, which is not always the one it
 * advertises: PHP's post_max_size wins, and on a default install it is smaller.
 * A body over that limit does not arrive truncated, it arrives empty.
 */
function effectiveMaxBytes() {
    $post = iniBytes(ini_get('post_max_size'));
    if ($post > 0 && $post < MAX_FILE_BYTES) {
        return $post;
    }
    return MAX_FILE_BYTES;
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

// Which client gets which packs, decided in one place instead of on every machine.
//
//   { "STUDIO-A": ["SEVILLE", "SHARED"], "*": ["SHARED"] }
//
// A name is a client's machine name, the same one it checks in under. "*" is what a
// machine gets when it is not named. A client that is named nowhere and has no "*"
// to fall back on keeps whatever it was set to locally, so adding this file cannot
// silently stop an existing machine from updating.
define('ASSIGNMENTS', STORAGE . '/.assignments.json');

// Enough for any estate this is for, and a bound on what a misbehaving client can
// fill the disk with. Updating an existing record is always allowed.
define('MAX_CLIENTS', 500);

/** A client name reduced to something safe to use as a filename. */
function clientId($name) {
    $id = preg_replace('/[^A-Za-z0-9._-]/', '_', $name);
    $id = trim($id, '._-');
    return $id === '' ? '' : substr($id, 0, 64);
}

function assignments() {
    if (!file_exists(ASSIGNMENTS)) {
        return array();
    }

    $map = json_decode(file_get_contents(ASSIGNMENTS), true);
    return is_array($map) ? $map : array();
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

// ---- is this thing set up safely -----------------------------------------

// The failures that actually happen are not clever. Somebody deploys with the
// shipped tokens. Somebody puts the storage under the web root on nginx, where the
// .htaccess written next to it does nothing at all, and every template becomes a
// public download. Somebody serves it over plain HTTP and the token goes across a
// venue's wifi in clear.
//
// None of those announce themselves. This asks the questions instead of waiting for
// someone to notice.
function selfTest() {
    $problems = array();
    $warnings = array();
    $notes = array();

    // The one that turns a relay into a public write endpoint.
    if (UPLOAD_TOKEN === 'change-me-upload' || DOWNLOAD_TOKEN === 'change-me-download') {
        $problems[] = 'A token is still the shipped default. Anyone who has read this file can write templates here.';
    }

    // Two tokens with one value is one token, and the point of the split is gone:
    // every client could then upload for every other client.
    if (UPLOAD_TOKEN === DOWNLOAD_TOKEN) {
        $problems[] = 'The upload and download tokens are the same, so every client can also upload.';
    }

    if (strlen(UPLOAD_TOKEN) < 20 || strlen(DOWNLOAD_TOKEN) < 20) {
        $warnings[] = 'A token is shorter than 20 characters. Use something long and random.';
    }

    // Plain HTTP means the token and every template cross the network readable.
    $https = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off')
          || (isset($_SERVER['HTTP_X_FORWARDED_PROTO']) && $_SERVER['HTTP_X_FORWARDED_PROTO'] === 'https')
          || (isset($_SERVER['SERVER_PORT']) && $_SERVER['SERVER_PORT'] == 443);

    $host = isset($_SERVER['HTTP_HOST']) ? $_SERVER['HTTP_HOST'] : '';
    $local = ($host === '' || strpos($host, 'localhost') === 0 || strpos($host, '127.0.0.1') === 0);

    if (!$https && !$local) {
        $problems[] = 'This request arrived over plain HTTP. Tokens and templates are readable in transit. Put it behind HTTPS.';
    } elseif (!$https) {
        $notes[] = 'Reached over plain HTTP, but on localhost, so this may just be a local test.';
    }

    // The quiet one. An .htaccess only binds Apache; on nginx or IIS the storage
    // folder under a web root is simply served, token or no token.
    $server = isset($_SERVER['SERVER_SOFTWARE']) ? $_SERVER['SERVER_SOFTWARE'] : 'unknown';
    $docRoot = isset($_SERVER['DOCUMENT_ROOT']) ? realpath($_SERVER['DOCUMENT_ROOT']) : false;
    $storage = realpath(STORAGE);
    $underDocRoot = ($docRoot !== false && $storage !== false
                     && str_starts_with($storage, $docRoot . DIRECTORY_SEPARATOR));

    if ($underDocRoot) {
        $apache = (stripos($server, 'apache') !== false);
        if ($apache) {
            $warnings[] = 'Storage sits under the web root. The .htaccess covers Apache, but moving STORAGE outside the web root is safer.';
        } else {
            $problems[] = 'Storage sits under the web root on ' . $server . ', where .htaccess does nothing. '
                        . 'Every template here may be downloadable without a token. Move STORAGE outside the web root, '
                        . 'or add the equivalent deny rule for this server.';
        }
    } else {
        $notes[] = 'Storage is outside the web root, which is where it belongs.';
    }

    if (!is_dir(STORAGE)) {
        $problems[] = 'The storage folder does not exist and could not be created.';
    } elseif (!is_writable(STORAGE)) {
        $problems[] = 'The storage folder is not writable, so no upload can ever succeed.';
    }

    // Read before ini_set ran, so this is the host's own setting. It matters more
    // than it looks: a request-startup warning is emitted before this file executes
    // and lands in front of the JSON, which every client then fails to parse.
    if (filter_var($GLOBALS['relayDisplayErrorsWasOn'], FILTER_VALIDATE_BOOLEAN)) {
        $problems[] = 'display_errors is on for this host, so a PHP warning can be printed in front of '
                    . 'a JSON answer and break every client. The .user.ini and .htaccess shipped beside '
                    . 'relay.php fix this; deploy them alongside it, or set display_errors = Off for this host.';
    }

    if (version_compare(PHP_VERSION, '7.4', '<')) {
        $problems[] = 'PHP ' . PHP_VERSION . ' is older than this needs. 7.4 or newer.';
    }

    // What an upload is actually allowed to be, which is not always what the relay
    // says. PHP wins, and on a default install its limit is much the smaller.
    $postMax = ini_get('post_max_size');
    if (iniBytes($postMax) > 0 && iniBytes($postMax) < MAX_FILE_BYTES) {
        $warnings[] = 'post_max_size is ' . $postMax . ', below the ' . round(MAX_FILE_BYTES / 1048576)
                    . ' MB this relay advertises. Anything larger is refused rather than stored, '
                    . 'which is correct but not what the limit says. Raise post_max_size in php.ini, '
                    . 'or lower MAX_FILE_BYTES to match.';
    }

    $notes[] = 'PHP ' . PHP_VERSION . ' on ' . $server . '; post_max_size ' . $postMax
             . '; the largest file that can actually be uploaded here is '
             . round(effectiveMaxBytes() / 1048576, 1) . ' MB.';

    $bytes = 0;
    $files = 0;
    foreach (allPacks() as $name) {
        $described = describePack($name);
        $bytes += $described['bytes'];
        $files += count($described['files']);
    }

    $notes[] = count(allPacks()) . ' pack(s), ' . $files . ' file(s), '
             . round($bytes / 1048576, 1) . ' MB stored; ' . count(allClients()) . ' client(s) checked in.';

    return array(
        'ok'       => empty($problems),
        'problems' => $problems,
        'warnings' => $warnings,
        'notes'    => $notes,
        'at'       => gmdate('c'),
    );
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
            'maxBytes'  => effectiveMaxBytes(),
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
            // What the client's last poll actually did. Capped like everything else
            // here: a client does not get to decide how much of this disk it uses.
            'result' => isset($sent['result']) ? substr((string) $sent['result'], 0, 300) : '',
            'failed' => isset($sent['failed']) ? (int) $sent['failed'] : 0,
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

    case 'selftest':
        // The upload token: this reports on how the relay is configured, which is
        // not something a client should be able to ask about.
        requireToken(UPLOAD_TOKEN);
        $result = selfTest();
        reply($result['ok'] ? 200 : 500, $result);

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

            // A client that reported failures is not current, whatever its pack list
            // says. One that could install nothing reports no packs at all, and
            // without this that reads as though it had nothing to do.
            $failed = isset($client['failed']) ? (int) $client['failed'] : 0;

            $client['behind'] = $behind;
            $client['current'] = empty($behind) && $failed === 0;
            $out[] = $client;
        }

        reply(200, array('clients' => $out, 'packs' => $current, 'at' => gmdate('c')));

    case 'assignments':
        if ($_SERVER['REQUEST_METHOD'] === 'POST') {
            // Deciding what every venue installs is the dev machine's job, so it
            // takes the upload token rather than the one every client holds.
            requireToken(UPLOAD_TOKEN);

            $sent = json_decode(file_get_contents('php://input'), true);
            if (!is_array($sent)) {
                reply(400, array('error' => 'Expected a JSON object of client to pack list'));
            }

            // Only the shape this understands is stored. A name has to be usable as
            // a client name and every pack has to be a real pack name, so a typo
            // cannot become a path.
            $clean = array();
            $dropped = array();

            foreach ($sent as $client => $packs) {
                if (!is_string($client) || $client === '' || strlen($client) > 128) {
                    continue;
                }

                // A machine name has to already be one, not merely survive being
                // cleaned into one. A key of "../evil" would never match any machine,
                // but storing it invites somebody to think it does something.
                if ($client !== '*' && clientId($client) !== $client) {
                    $dropped[] = $client;
                    continue;
                }

                if (!is_array($packs)) {
                    $dropped[] = $client;
                    continue;
                }

                $list = array();
                foreach ($packs as $pack) {
                    if (is_string($pack) && safeSegment($pack)) {
                        $list[] = $pack;
                    }
                }

                // An empty list is a real instruction: this machine takes nothing.
                // So a list that arrived with packs in it and ends up empty is not
                // that instruction, it is a typo, and storing it would quietly stop
                // a venue updating. Dropped instead, which leaves the machine on
                // whatever it had.
                if (empty($list) && !empty($packs)) {
                    $dropped[] = $client;
                    continue;
                }

                $clean[$client] = $list;
            }

            // A pack name can be perfectly valid and still not be a pack. Assigning
            // SEVILE instead of SEVILLE is accepted by every check above and then
            // quietly delivers nothing to that venue.
            //
            // Not refused, because assigning a pack before uploading it is a
            // reasonable order to work in. Reported, so the mistake is visible at the
            // moment it is made rather than at the venue an hour later.
            $known = allPacks();
            $unknown = array();
            foreach ($clean as $client => $list) {
                foreach ($list as $pack) {
                    if (!in_array($pack, $known, true) && !in_array($pack, $unknown, true)) {
                        $unknown[] = $pack;
                    }
                }
            }

            $temporary = ASSIGNMENTS . '.part';
            if (file_put_contents($temporary, json_encode($clean, JSON_PRETTY_PRINT)) === false
                || !rename($temporary, ASSIGNMENTS)) {
                @unlink($temporary);
                reply(500, array('error' => 'Cannot store the assignments'));
            }

            reply(200, array('ok' => true, 'clients' => count($clean),
                             'dropped' => $dropped, 'unknownPacks' => $unknown));
        }

        // Readable with either token: a client needs to know what it is assigned.
        if (!tokenIs(UPLOAD_TOKEN) && !tokenIs(DOWNLOAD_TOKEN)) {
            reply(401, array('error' => 'Missing or wrong X-Relay-Token'));
        }
        reply(200, array('assignments' => assignments(), 'at' => gmdate('c')));

    case 'manifest':
        requireToken(DOWNLOAD_TOKEN);
        if ($pack !== '') {
            reply(200, describePack($pack));
        }

        $packs = array();
        foreach (allPacks() as $name) {
            $packs[] = describePack($name);
        }

        // Sent with the manifest rather than as its own request: a client needs both
        // on every poll, and one round trip is one round trip.
        reply(200, array('packs' => $packs, 'assignments' => assignments(), 'at' => gmdate('c')));

    case 'fetch':
        requireToken(DOWNLOAD_TOKEN);
        if ($pack === '' || !safeRelativePath($path)) {
            reply(400, array('error' => 'Need a pack and a usable path'));
        }

        $real = resolveInPack($pack, $path);
        if ($real === false || !is_file($real)) {
            reply(404, array('error' => 'No such file'));
        }

        // Same reasoning as reply(): a warning in front of the bytes would corrupt
        // the file, and the digest check on the far end would refuse a template that
        // was actually fine.
        if (ob_get_level() > 0) {
            ob_clean();
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
            $body = '';
        }

        // PHP silently discards a body over post_max_size, so what arrives is empty
        // rather than short. Writing that would replace a working template with
        // nothing, and the sender would be told it succeeded.
        $declared = isset($_SERVER['CONTENT_LENGTH']) ? (int) $_SERVER['CONTENT_LENGTH'] : -1;
        if ($declared > 0 && strlen($body) !== $declared) {
            reply(413, array(
                'error'    => 'The body did not arrive whole. This is almost always post_max_size in php.ini.',
                'sent'     => $declared,
                'received' => strlen($body),
                'limit'    => ini_get('post_max_size'),
            ));
        }

        if ($body === '') {
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
            'actions' => array('ping', 'manifest', 'fetch', 'upload', 'remove',
                               'checkin', 'clients', 'selftest', 'assignments'),
        ));
}
