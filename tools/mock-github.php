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
const REPO = __DIR__ . '/mock_repo';

ini_set('display_errors', '0');

$given = isset($_SERVER['HTTP_AUTHORIZATION']) ? $_SERVER['HTTP_AUTHORIZATION'] : '';
if ($given !== 'Bearer ' . TOKEN) {
    // What GitHub does for a private repository a token cannot see.
    http_response_code(404);
    header('Content-Type: application/json');
    echo json_encode(array('message' => 'Not Found'));
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

$path = parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH);

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
