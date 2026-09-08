<?php
// The sheet cache the templates read from.
//
// A template asks for a spreadsheet and a sheet number and either gets rows back or
// a 404 that sends it to Google instead. That is the whole contract, and it is the
// same one the client serves from its own port, so a template can be pointed at
// either without being changed.
//
//   GET  ?spreadsheetId=X&sheetNumber=N    the stored rows, or 404
//   POST ?spreadsheetId=X&sheetNumber=N    store rows (body is the JSON)
//
// Why it exists: a Google Sheets API key has a quota per minute. Twenty templates
// each reading the same sheet spend twenty reads on one answer. Pointed here they
// spend one, and the rest come off this disk.
//
// This is the published version of the local_server.php that has been in use, with
// one thing fixed. That one built its filename straight from the query, so a request
// naming a spreadsheet of "../../something" wrote outside the data folder. It was
// reachable by anyone who could reach the page, needed no token, and would create
// the file. Both the id and the sheet number are now checked before they are used.

// Where the cached sheets are kept. Created on first use.
const STORE = __DIR__ . '/sheets_data';

// Anything older than this is served but reported as stale, so a caller can decide
// for itself whether to trust it. Nothing is deleted here.
const STALE_AFTER_SECONDS = 3600;

header('Content-Type: application/json');

// Templates run from file:// as often as from http://, and a file:// page sends an
// Origin of null. A wildcard covers both; naming an origin would not.
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
    echo json_encode($body);
    exit;
}

/**
 * A Google spreadsheet id is letters, digits, hyphens and underscores. Nothing else
 * belongs in one, and nothing else belongs in a filename built from one.
 */
function usableId($value) {
    return is_string($value) && $value !== '' && strlen($value) <= 120
        && preg_match('/^[A-Za-z0-9_-]+$/', $value) === 1;
}

/** A sheet number is a number. */
function usableSheet($value) {
    return is_string($value) && $value !== '' && strlen($value) <= 6
        && preg_match('/^[0-9]+$/', $value) === 1;
}

$spreadsheetId = isset($_GET['spreadsheetId']) ? $_GET['spreadsheetId'] : '';
$sheetNumber = isset($_GET['sheetNumber']) ? $_GET['sheetNumber'] : '';

if (!usableId($spreadsheetId) || !usableSheet($sheetNumber)) {
    // Deliberately the same answer for a missing parameter and an unusable one:
    // there is nothing here worth telling apart.
    reply(400, array('error' => 'Missing or unusable parameters'));
}

if (!is_dir(STORE) && !mkdir(STORE, 0755, true)) {
    reply(500, array('error' => 'Cannot create the cache folder'));
}

// Safe by construction now: both halves have been checked against a pattern that
// cannot contain a separator or a dot.
$file = STORE . '/' . $spreadsheetId . '_' . $sheetNumber . '.json';

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $body = file_get_contents('php://input');
    if ($body === false || $body === '') {
        reply(400, array('error' => 'No body'));
    }

    // It has to be JSON, or a template would read back something it cannot parse and
    // have no way to tell that from a sheet with no rows in it.
    if (json_decode($body) === null && json_last_error() !== JSON_ERROR_NONE) {
        reply(400, array('error' => 'Body is not JSON'));
    }

    // Written beside and moved into place, so a template reading mid-write gets the
    // previous answer rather than half of the next one.
    $temporary = $file . '.part';
    if (file_put_contents($temporary, $body) === false || !rename($temporary, $file)) {
        @unlink($temporary);
        reply(500, array('error' => 'Cannot store the sheet'));
    }

    reply(200, array('success' => 'Sheet updated', 'bytes' => strlen($body)));
}

if (!file_exists($file)) {
    // The answer a template is built to handle: it goes to Google instead.
    reply(404, array('error' => 'Sheet not found'));
}

// Age is a header rather than part of the body, because the body is the sheet and
// a template parses it expecting exactly what it stored.
$age = time() - filemtime($file);
header('X-Sheet-Age: ' . $age);
header('X-Sheet-Stale: ' . ($age > STALE_AFTER_SECONDS ? 'true' : 'false'));

readfile($file);
