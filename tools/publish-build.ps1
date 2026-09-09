# Publish a build you just made as a GitHub release, so clients can find it.
#
# Your build command ends with a deployed folder and a timestamped copy on the
# Desktop. That folder is already the thing a venue needs; this puts it somewhere
# a venue can reach, named so the client can tell whether it is newer than what it
# is running.
#
#   powershell -ExecutionPolicy Bypass -File tools\publish-build.ps1
#
# It publishes nothing you have not built: it reads the version and the build id
# out of the source it was built from, so the tag can never claim a build that
# does not match the binary.
#
# Nothing here is automatic and nothing overwrites a published build unless you
# ask. A venue may already be running what is under that tag.

[CmdletBinding()]
param(
    # The tree your build command builds in - the copy, not this repository, since
    # that is what was actually compiled.
    [string] $BuildTree = "C:\CasparCG\latest build",

    # owner/repo to publish to. Defaults to this repository's origin.
    [string] $Repo = "",

    # Replace the assets on a tag that already exists. Off, because a venue may
    # already have downloaded what is there.
    [switch] $Force,

    # Build the zip and the checksum, print what would happen, publish nothing.
    [switch] $WhatIfPublish
)

$ErrorActionPreference = "Stop"

function Fail($message) {
    Write-Host "  ! $message" -ForegroundColor Red
    exit 1
}

# Run a native command, discard its output, and hand back whether it succeeded.
#
# Windows PowerShell 5.1 turns a native program's stderr into an ErrorRecord when
# it is redirected, and with ErrorActionPreference = Stop that ends the script. The
# questions asked here - does this tag exist, is there an origin remote - answer
# "no" on stderr, so asking them the obvious way makes the normal case fatal.
function TryNative([scriptblock] $command) {
    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"

    try {
        & $command *> $null
        return ($LASTEXITCODE -eq 0)
    } finally {
        $ErrorActionPreference = $previous
    }
}

# ---- what was built ---------------------------------------------------------

$release = Join-Path $BuildTree "build\Release"
$exe = Join-Path $release "casparcg-client.exe"

if (-not (Test-Path $exe)) {
    Fail "No casparcg-client.exe under `"$release`". Build first, and check -BuildTree."
}

# windeployqt puts Qt6Core.dll beside the exe. Without it the zip is a binary that
# will not start on a machine that has no Qt, which is every venue.
if (-not (Test-Path (Join-Path $release "Qt6Core.dll"))) {
    Fail "No Qt6Core.dll beside the exe - windeployqt has not run on this build."
}

# ---- what it calls itself ---------------------------------------------------
#
# Read from the tree that was compiled, never from this checkout: if the two have
# drifted, the binary is the truth and the tag has to match it.

$cmakeLists = Join-Path $BuildTree "src\CMakeLists.txt"
$versionIn = Join-Path $BuildTree "src\Common\Version.h.in"

if (-not (Test-Path $cmakeLists)) { Fail "No src\CMakeLists.txt under `"$BuildTree`"." }
if (-not (Test-Path $versionIn)) { Fail "No src\Common\Version.h.in under `"$BuildTree`"." }

$cmakeText = Get-Content $cmakeLists -Raw
$versionText = Get-Content $versionIn -Raw

function ReadNumber($text, $pattern, $what) {
    $match = [regex]::Match($text, $pattern)
    if (-not $match.Success) { Fail "Could not read $what out of the source." }
    return $match.Groups[1].Value
}

$major = ReadNumber $cmakeText 'CONFIG_VERSION_MAJOR\s+(\d+)' "the major version"
$minor = ReadNumber $cmakeText 'CONFIG_VERSION_MINOR\s+(\d+)' "the minor version"
$bug = ReadNumber $cmakeText 'CONFIG_VERSION_BUG\s+(\d+)' "the revision"
$build = ReadNumber $versionText 'DEV_BUILD_ID\s+"(\d+)"' "the build id"

$tag = "v$major.$minor.$bug-$build"
$assetName = "casparcg-client-$tag-windows.zip"

Write-Host ""
Write-Host "  building   $major.$minor.$bug build $build"
Write-Host "  tag        $tag"
Write-Host "  asset      $assetName"

# ---- where it goes ----------------------------------------------------------

if ($Repo -eq "") {
    $root = Join-Path $PSScriptRoot ".."
    $originUrl = ""

    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $originUrl = (& git -C $root remote get-url origin)
    } catch {
        $originUrl = ""
    } finally {
        $ErrorActionPreference = $previous
    }

    if (-not $originUrl) { Fail "No origin remote, so nowhere to publish. Pass -Repo owner/repo." }

    $m = [regex]::Match($originUrl, 'github\.com[:/](?<owner>[^/]+)/(?<repo>[^/.]+)')
    if (-not $m.Success) { Fail "Could not read owner/repo out of `"$originUrl`". Pass -Repo." }

    $Repo = "$($m.Groups['owner'].Value)/$($m.Groups['repo'].Value)"
}

Write-Host "  repository $Repo"

# ---- the package ------------------------------------------------------------
#
# One folder inside the zip, named for the build, so unzipping never scatters a
# hundred DLLs into whatever folder somebody happened to be in.

$staging = Join-Path $env:TEMP "casparcg-publish"
$inner = Join-Path $staging "casparcg-client-$tag"

if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory -Path $inner -Force | Out-Null

Write-Host ""
Write-Host "  copying the deployed build..."
& robocopy $release $inner /E /NFL /NDL /NP /R:2 /W:1 | Out-Null
# robocopy exit codes below 8 are success; 8 and up are real failures.
if ($LASTEXITCODE -ge 8) { Fail "robocopy failed with $LASTEXITCODE" }

$zipPath = Join-Path $staging $assetName

Write-Host "  compressing..."
$sevenZip = "C:\Program Files\7-Zip\7z.exe"
if (Test-Path $sevenZip) {
    & $sevenZip a -tzip -mx=5 $zipPath $inner | Out-Null
    if ($LASTEXITCODE -ne 0) { Fail "7-Zip failed with $LASTEXITCODE" }
} else {
    Compress-Archive -Path $inner -DestinationPath $zipPath -CompressionLevel Optimal
}

$zipInfo = Get-Item $zipPath
$sizeMb = [math]::Round($zipInfo.Length / 1MB, 1)

# ---- the checksum -----------------------------------------------------------
#
# The releases API publishes a size and nothing else worth verifying against, and
# two builds of this client are the same size often enough that a size check would
# pass one for the other. The client refuses a download whose hash it cannot
# match, so this file is what makes the download verifiable at all.

$hash = (Get-FileHash $zipPath -Algorithm SHA256).Hash.ToLower()
$sumsPath = Join-Path $staging "SHA256SUMS.txt"
"$hash  $assetName" | Out-File -FilePath $sumsPath -Encoding ascii -NoNewline

Write-Host "  $assetName  $sizeMb MB"
Write-Host "  sha256 $hash"

# ---- publish ----------------------------------------------------------------

if ($WhatIfPublish) {
    Write-Host ""
    Write-Host "  -WhatIfPublish: built but not published. The files are in $staging"
    exit 0
}

$existing = TryNative { gh release view $tag --repo $Repo --json tagName }

if ($existing -and -not $Force) {
    Write-Host ""
    Write-Host "  ! $tag is already published." -ForegroundColor Yellow
    Write-Host "    A venue may already be running it. Bump DEV_BUILD_ID for a new build,"
    Write-Host "    or pass -Force to replace what is under this tag."
    exit 1
}

Write-Host ""
if ($existing) {
    Write-Host "  replacing the assets on $tag..."
    & gh release upload $tag $zipPath $sumsPath --repo $Repo --clobber
    if ($LASTEXITCODE -ne 0) { Fail "gh release upload failed" }
} else {
    Write-Host "  creating $tag..."
    $notes = "Build $build of the client, $major.$minor.$bug.`n`nVerify with SHA256SUMS.txt before installing."
    & gh release create $tag $zipPath $sumsPath --repo $Repo --title "Build $build" --notes $notes
    if ($LASTEXITCODE -ne 0) { Fail "gh release create failed" }
}

Remove-Item $staging -Recurse -Force

Write-Host ""
Write-Host "  published $tag to $Repo" -ForegroundColor Green
Write-Host "  clients pointed at $Repo will see it on their next check."
