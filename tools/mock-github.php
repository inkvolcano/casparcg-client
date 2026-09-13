<?php
// Just enough of the GitHub API to run the pull route against, for tools/test-github.
//
// It answers the three calls a client makes, in the shapes the real API uses. Those
// shapes were checked against github.com itself while the client was written: HEAD
// works as a tree ref, a recursive tree lists full slash-separated paths with a
// type and a Git blob sha each, and a blob asked for with the raw media type comes
// back as bytes.
//
// The content is whatever sits under mock_repo/ beside this file, so a test changes
// what the repository holds by writing a file.

const TOKEN = 'a-github-token-for-testing';

// A second token that can write. The real thing is a fine-grained token with
// Contents: read and write; the pull token has read only, and a push made with
// it must be refused the way GitHub refuses it.
const WRITE_TOKEN = 'a-github-write-token-for-testing';
const STATE = __DIR__ . '/mock_state';
const REPO = __DIR__ . '/mock_repo';

ini_set('display_errors', '0');

// A check-in, which is not a GitHub call at all - the client posts it to whatever
// address it was given for reporting, and this stands in for that so the test can
// see whether one was ever sent. Above the Authorization check on purpose: a
// check-in carries X-Relay-Token instead, and would otherwise be refused as though
// the repository did not exist.
//
// It exists because a client on the GitHub route sent no check-in at all for
// several builds. The pull worked, the estate view stayed empty, and nothing here
// looked at the one thing that would have shown it.
if (isset($_GET['action']) && $_GET['action'] === 'checkin') {
    file_put_contents(__DIR__ . '/mock_checkin.json', file_get_contents('php://input'));

    header('Content-Type: application/json');
    echo json_encode(array('ok' => true));
    exit;
}

$given = isset($_SERVER['HTTP_AUTHORIZATION']) ? $_SERVER['HTTP_AUTHORIZATION'] : '';
if ($given !== 'Bearer ' . TOKEN && $given !== 'Bearer ' . WRITE_TOKEN) {
    // What GitHub does for a private repository a token cannot see.
    http_response_code(404);
    header('Content-Type: application/json');
    echo json_encode(array('message' => 'Not Found'));
    exit;
}

// Writing needs the token that can write. GitHub's wording, so the client's
// message to the operator is the one they would see against the real thing.
if ($_SERVER['REQUEST_METHOD'] !== 'GET' && $given !== 'Bearer ' . WRITE_TOKEN) {
    http_response_code(403);
    header('Content-Type: application/json');
    echo json_encode(array('message' => 'Resource not accessible by personal access token'));
    exit;
}

// GitHub refuses a request with no user agent, and the client sends one, so this
// holds it to the same rule rather than being more forgiving than the real thing.
if (empty($_SERVER['HTTP_USER_AGENT'])) {
    http_response_code(403);
    header('Content-Type: application/json');
    echo json_encode(array('message' => 'Request forbidden by administrative rules'));
    exit;
}

/** sha1("blob <length>\0" . $content), which is how Git names a file's contents. */
function blobSha($content) {
    return sha1('blob ' . strlen($content) . "\0" . $content);
}

/** Every file under mock_repo, as [path => content]. */
function everything() {
    $files = array();
    if (!is_dir(REPO)) {
        return $files;
    }

    $walker = new RecursiveIteratorIterator(
        new RecursiveDirectoryIterator(REPO, FilesystemIterator::SKIP_DOTS)
    );

    foreach ($walker as $file) {
        if (!$file->isFile()) {
            continue;
        }
        $relative = str_replace('\\', '/', substr($file->getPathname(), strlen(REPO) + 1));
        $files[$relative] = file_get_contents($file->getPathname());
    }

    ksort($files);
    return $files;
}

/** The head commit: named by everything the repository holds, so it moves when a push lands. */
function headSha() {
    return sha1('commit ' . json_encode(everything()));
}

function stateDir($kind) {
    $dir = STATE . '/' . $kind;
    if (!is_dir($dir)) {
        mkdir($dir, 0777, true);
    }
    return $dir;
}

function jsonBody() {
    $decoded = json_decode(file_get_contents('php://input'), true);
    return is_array($decoded) ? $decoded : array();
}

$path = parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH);
$method = $_SERVER['REQUEST_METHOD'];

// GET /repos/{owner}/{repo}
if (preg_match('#^/repos/[^/]+/[^/]+$#', $path)) {
    header('Content-Type: application/json');
    echo json_encode(array(
        'full_name'      => 'mock/templates',
        'default_branch' => 'main',
        'private'        => true,
    ));
    exit;
}

// GET /repos/{owner}/{repo}/git/ref/heads/{branch}
if ($method === 'GET' && preg_match('#^/repos/[^/]+/[^/]+/git/ref/heads/(.+)$#', $path, $found)) {
    header('Content-Type: application/json');
    echo json_encode(array(
        'ref'    => 'refs/heads/' . $found[1],
        'object' => array('type' => 'commit', 'sha' => headSha()),
    ));
    exit;
}

// The write half of the Git Data API, in the order a push uses it: blobs, a tree,
// a commit, then the branch moved to it. Nothing reaches mock_repo until the
// last step, and that step refuses a commit whose parent is not the current
// head - which is what the real thing does without force, and what stops a
// push from overwriting one that landed in between.

// POST /repos/{owner}/{repo}/git/blobs
if ($method === 'POST' && preg_match('#^/repos/[^/]+/[^/]+/git/blobs$#', $path)) {
    $body = jsonBody();
    $content = isset($body['encoding']) && $body['encoding'] === 'base64'
        ? base64_decode(isset($body['content']) ? $body['content'] : '')
        : (isset($body['content']) ? $body['content'] : '');

    $sha = blobSha($content);
    file_put_contents(stateDir('blobs') . '/' . $sha, $content);

    http_response_code(201);
    header('Content-Type: application/json');
    echo json_encode(array('sha' => $sha));
    exit;
}

// POST /repos/{owner}/{repo}/git/trees
if ($method === 'POST' && preg_match('#^/repos/[^/]+/[^/]+/git/trees$#', $path)) {
    $body = jsonBody();
    $entries = isset($body['tree']) && is_array($body['tree']) ? $body['tree'] : array();

    foreach ($entries as $entry) {
        if (!isset($entry['sha']) || !is_file(stateDir('blobs') . '/' . $entry['sha'])) {
            http_response_code(422);
            header('Content-Type: application/json');
            echo json_encode(array('message' => 'tree.sha does not name a blob that was created'));
            exit;
        }
    }

    $sha = sha1('tree ' . json_encode($entries));
    file_put_contents(stateDir('trees') . '/' . $sha . '.json', json_encode($entries));

    http_response_code(201);
    header('Content-Type: application/json');
    echo json_encode(array('sha' => $sha));
    exit;
}

// POST /repos/{owner}/{repo}/git/commits
if ($method === 'POST' && preg_match('#^/repos/[^/]+/[^/]+/git/commits$#', $path)) {
    $body = jsonBody();
    $sha = sha1('commit-object ' . json_encode($body));
    file_put_contents(stateDir('commits') . '/' . $sha . '.json', json_encode($body));

    http_response_code(201);
    header('Content-Type: application/json');
    echo json_encode(array('sha' => $sha));
    exit;
}

// PATCH /repos/{owner}/{repo}/git/refs/heads/{branch}
if ($method === 'PATCH' && preg_match('#^/repos/[^/]+/[^/]+/git/refs/heads/(.+)$#', $path, $found)) {
    $body = jsonBody();
    $commitFile = stateDir('commits') . '/' . (isset($body['sha']) ? $body['sha'] : '') . '.json';
    if (!is_file($commitFile)) {
        http_response_code(422);
        header('Content-Type: application/json');
        echo json_encode(array('message' => 'Object does not exist'));
        exit;
    }

    $commit = json_decode(file_get_contents($commitFile), true);
    $parents = isset($commit['parents']) && is_array($commit['parents']) ? $commit['parents'] : array();
    if (count($parents) !== 1 || $parents[0] !== headSha()) {
        http_response_code(422);
        header('Content-Type: application/json');
        echo json_encode(array('message' => 'Update is not a fast forward'));
        exit;
    }

    $treeFile = stateDir('trees') . '/' . (isset($commit['tree']) ? $commit['tree'] : '') . '.json';
    if (!is_file($treeFile)) {
        http_response_code(422);
        header('Content-Type: application/json');
        echo json_encode(array('message' => 'The commit names no tree that was created'));
        exit;
    }

    foreach (json_decode(file_get_contents($treeFile), true) as $entry) {
        $destination = REPO . '/' . $entry['path'];
        $folder = dirname($destination);
        if (!is_dir($folder)) {
            mkdir($folder, 0777, true);
        }
        file_put_contents($destination, file_get_contents(stateDir('blobs') . '/' . $entry['sha']));
    }

    // For the test to read back what was said and by whom.
    file_put_contents(STATE . '/last_commit.json', json_encode($commit));

    header('Content-Type: application/json');
    echo json_encode(array('ref' => 'refs/heads/' . $found[1],
                           'object' => array('type' => 'commit', 'sha' => $body['sha'])));
    exit;
}

// GET /repos/{owner}/{repo}/git/trees/{ref}?recursive=1
if (preg_match('#^/repos/[^/]+/[^/]+/git/trees/#', $path)) {
    $tree = array();
    $directories = array();

    foreach (everything() as $relative => $content) {
        // Directories appear in a real tree as their own entries, and the client
        // has to skip them. Leaving them out would make this easier than reality.
        $parts = explode('/', $relative);
        array_pop($parts);
        $sofar = '';
        foreach ($parts as $part) {
            $sofar = $sofar === '' ? $part : $sofar . '/' . $part;
            $directories[$sofar] = true;
        }

        $tree[] = array(
            'path' => $relative,
            'mode' => '100644',
            'type' => 'blob',
            'sha'  => blobSha($content),
            'size' => strlen($content),
        );
    }

    foreach (array_keys($directories) as $directory) {
        $tree[] = array('path' => $directory, 'mode' => '040000',
                        'type' => 'tree', 'sha' => sha1($directory));
    }

    header('Content-Type: application/json');
    echo json_encode(array(
        'sha'       => 'mocktree',
        'tree'      => $tree,
        // A test can ask for the truncated case by putting a file called TRUNCATE
        // at the root, because that answer looks exactly like a missing repository
        // and the client has to refuse it rather than act on it.
        'truncated' => file_exists(REPO . '/TRUNCATE'),
    ));
    exit;
}

// GET /repos/{owner}/{repo}/git/blobs/{sha}
if (preg_match('#^/repos/[^/]+/[^/]+/git/blobs/([0-9a-f]+)$#', $path, $found)) {
    foreach (everything() as $relative => $content) {
        if (blobSha($content) !== $found[1]) {
            continue;
        }

        // The raw media type gives the bytes; anything else gets JSON. The client
        // asks for raw, so answering JSON here would be a way to catch it not doing so.
        $accept = isset($_SERVER['HTTP_ACCEPT']) ? $_SERVER['HTTP_ACCEPT'] : '';
        if (strpos($accept, 'raw') === false) {
            header('Content-Type: application/json');
            echo json_encode(array('content' => base64_encode($content), 'encoding' => 'base64'));
            exit;
        }

        header('Content-Type: application/octet-stream');
        header('Content-Length: ' . strlen($content));
        echo $content;
        exit;
    }

    http_response_code(404);
    header('Content-Type: application/json');
    echo json_encode(array('message' => 'Not Found'));
    exit;
}

http_response_code(404);
header('Content-Type: application/json');
echo json_encode(array('message' => 'Not Found', 'path' => $path));
