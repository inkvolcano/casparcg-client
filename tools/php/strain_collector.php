<?php
// Where every client reports what it is spending, so one place knows the total.
//
// A Google Sheets API key has a quota per minute, and it is spent per key, not per
// spreadsheet. Two clients and a connector all drawing on the same key can each be
// well inside the limit and still take the key past it together. None of them can
// see that on their own.
//
// So they all report here, and anything that needs the real number reads it back.
//
//   POST /strain_collector.php          a client or connector reporting itself
//   GET  /strain_collector.php          everyone's reports, and the totals per key
//   GET  ?format=flat                   one line per key, for something simple
//
// Point a client at this with Settings -> Sheets -> "Report to URL". It posts every
// ten seconds while sheets are being read, and nothing at all when they are not.

const STORE = __DIR__ . '/strain_data';

// A report older than this is nobody's current usage. Kept out of the totals and
// swept away when it is well past caring about.
const STALE_AFTER_SECONDS = 120;
const FORGET_AFTER_SECONDS = 86400;

// A bound on what a misbehaving reporter can leave on the disk.
const MAX_BODY_BYTES = 256 * 1024;
const MAX_REPORTERS = 200;

header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
header('Access-Control-Allow-Headers: X-Requested-With, Content-Type, Accept, Origin');
header('Cache-Control: no-store');

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(204);
    exit;
}

function reply($status, array $body) {
    http_response_code($status);
    echo json_encode($body, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
    exit;
}

/** A reporter's name reduced to something safe to use as a filename. */
function reporterId($source, $host) {
    $raw = trim($source . '-' . $host, '-');
    $id = preg_replace('/[^A-Za-z0-9._-]/', '_', $raw);
    $id = trim($id, '._-');
    return $id === '' ? '' : substr($id, 0, 80);
}

function allReports() {
    if (!is_dir(STORE)) {
        return array();
    }

    $reports = array();
    foreach (scandir(STORE) as $name) {
        if (substr($name, -5) !== '.json') {
            continue;
        }

        $path = STORE . '/' . $name;

        // Old enough that nobody is coming back for it.
        if (time() - filemtime($path) > FORGET_AFTER_SECONDS) {
            @unlink($path);
            continue;
        }

        $report = json_decode(file_get_contents($path), true);
        if (is_array($report)) {
            $report['ageSeconds'] = time() - filemtime($path);
            $report['current'] = $report['ageSeconds'] <= STALE_AFTER_SECONDS;
            $reports[] = $report;
        }
    }

    usort($reports, function ($a, $b) {
        return strcmp(isset($a['host']) ? $a['host'] : '', isset($b['host']) ? $b['host'] : '');
    });
    return $reports;
}

// ---- a client reporting itself -------------------------------------------

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $body = file_get_contents('php://input');

    // PHP discards a body over post_max_size rather than truncating it, so a short
    // read means the limit, not a short report.
    $declared = isset($_SERVER['CONTENT_LENGTH']) ? (int) $_SERVER['CONTENT_LENGTH'] : -1;
    if ($declared > 0 && strlen($body) !== $declared) {
        reply(413, array('error' => 'The body did not arrive whole; check post_max_size'));
    }

    if ($body === false || $body === '' || strlen($body) > MAX_BODY_BYTES) {
        reply(400, array('error' => 'Missing or oversized body'));
    }

    $report = json_decode($body, true);
    if (!is_array($report)) {
        reply(400, array('error' => 'Expected a JSON object'));
    }

    $id = reporterId(
        isset($report['source']) ? (string) $report['source'] : 'unknown',
        isset($report['host']) ? (string) $report['host'] : ''
    );

    if ($id === '') {
        reply(400, array('error' => 'A report needs a source or a host'));
    }

    if (!is_dir(STORE) && !mkdir(STORE, 0755, true)) {
        reply(500, array('error' => 'Cannot create the store'));
    }

    $file = STORE . '/' . $id . '.json';

    // A name already known may always update itself, so a busy estate is never
    // stopped from reporting by the cap.
    if (!file_exists($file) && count(allReports()) >= MAX_REPORTERS) {
        reply(429, array('error' => 'Already tracking as many reporters as this will'));
    }

    $temporary = $file . '.part';
    if (file_put_contents($temporary, $body) === false || !rename($temporary, $file)) {
        @unlink($temporary);
        reply(500, array('error' => 'Cannot store that report'));
    }

    reply(200, array('ok' => true, 'reporter' => $id));
}

// ---- what everything adds up to ------------------------------------------

$reports = allReports();

// Per key, because that is what the quota is against. keyId is a digest of the API
// key rather than the key, so two things sharing a budget can be added together
// without this ever being told what the key is.
$byKey = array();
$bySheet = array();

foreach ($reports as $report) {
    if (empty($report['current']) || empty($report['sheets']) || !is_array($report['sheets'])) {
        continue;
    }

    foreach ($report['sheets'] as $sheet) {
        $key = isset($sheet['keyId']) && $sheet['keyId'] !== '' ? $sheet['keyId'] : 'unknown';
        $id = isset($sheet['spreadsheetId']) ? $sheet['spreadsheetId'] : 'unknown';
        $total = isset($sheet['total']) ? (int) $sheet['total'] : 0;

        if (!isset($byKey[$key])) {
            $byKey[$key] = array('keyId' => $key, 'readsLastMinute' => 0,
                                 'sheets' => array(), 'reporters' => array());
        }

        $byKey[$key]['readsLastMinute'] += $total;
        if (!in_array($id, $byKey[$key]['sheets'], true)) {
            $byKey[$key]['sheets'][] = $id;
        }

        $who = isset($report['host']) ? $report['host'] : 'unknown';
        if (!in_array($who, $byKey[$key]['reporters'], true)) {
            $byKey[$key]['reporters'][] = $who;
        }

        if (!isset($bySheet[$id])) {
            $bySheet[$id] = array('spreadsheetId' => $id, 'keyId' => $key,
                                  'project' => isset($sheet['project']) ? $sheet['project'] : '',
                                  'readsLastMinute' => 0);
        }
        $bySheet[$id]['readsLastMinute'] += $total;
    }
}

// A flat line per key, for a connector that would rather not parse a tree.
if (isset($_GET['format']) && $_GET['format'] === 'flat') {
    header('Content-Type: text/plain');
    foreach ($byKey as $key) {
        echo $key['keyId'] . ' ' . $key['readsLastMinute'] . ' '
           . count($key['sheets']) . ' ' . count($key['reporters']) . "\n";
    }
    exit;
}

reply(200, array(
    'keys'      => array_values($byKey),
    'sheets'    => array_values($bySheet),
    'reporters' => $reports,
    'at'        => gmdate('c'),
));
