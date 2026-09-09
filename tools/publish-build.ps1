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

# The standalone updater goes out with every release, because the clients that
# most need it are the ones too old to have Install and Restart - they can find a
# build and download it, and then have no way to put it in place. Downloading one
# small file beats doing the swap by hand and getting the nested folder wrong.
# The server-side PHP, versioned with the build it belongs to.
#
# It is not part of the client and does not go on a venue machine - it is the
# relay and the sheet cache, which live on a web host. It ships here because the
# builds repository is the only thing some people have, and because the relay
# protocol moves with the client: build 206 taught the relay two timestamps
# instead of one, and 211 taught it which build each venue runs. Pairing them in
# one release is how anybody can tell which relay goes with which client.
#
# -Force on Get-ChildItem because .htaccess and .user.ini decide whether the relay
# works at all, and leaving them out is the classic way to deploy a broken one.
$phpSource = Join-Path $PSScriptRoot "php"
$phpPath = Join-Path $staging "server-php.zip"

if (Test-Path $phpSource) {
    $phpStage = Join-Path $staging "php"
    New-Item -ItemType Directory -Path $phpStage -Force | Out-Null
    Copy-Item -Path (Join-Path $phpSource "*") -Destination $phpStage -Recurse -Force

    $dotfiles = Get-ChildItem $phpSource -Recurse -Force -Filter ".*" -File
    foreach ($dotfile in $dotfiles) {
        $relative = $dotfile.FullName.Substring($phpSource.Length).TrimStart('\')
        $target = Join-Path $phpStage $relative
        New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
        Copy-Item $dotfile.FullName $target -Force
    }

    Compress-Archive -Path (Join-Path $phpStage "*") -DestinationPath $phpPath -Force
    Remove-Item $phpStage -Recurse -Force

    $phpCount = (Get-ChildItem $phpSource -Recurse -Force -File).Count
    Write-Host "  server-php.zip  $phpCount files"
} else {
    Fail "tools\php is missing, so a release would carry no relay for anyone to deploy."
}

$bootstrapSource = Join-Path $PSScriptRoot "install-update.cmd"
$bootstrapPath = Join-Path $staging "install-update.cmd"
if (Test-Path $bootstrapSource) {
    Copy-Item $bootstrapSource $bootstrapPath
} else {
    Fail "tools\install-update.cmd is missing, so old clients would have no way to install this."
}

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
    & gh release upload $tag $zipPath $sumsPath $bootstrapPath $phpPath --repo $Repo --clobber
    if ($LASTEXITCODE -ne 0) { Fail "gh release upload failed" }
} else {
    Write-Host "  creating $tag..."
    $notes = "Build $build of the client, $major.$minor.$bug." +
             "`n`n**On build 210 or newer:** Help -> Check for Updates -> Download -> Install and Restart." +
             "`n`n**On anything older**, which has no Install and Restart: Download, then Show Download, " +
             "put ``install-update.cmd`` from this release into the folder that opens, close the client and " +
             "run it. It works out the rest for itself. Only needed once." +
             "`n`nVerified against SHA256SUMS.txt either way." +
             "`n`n``server-php.zip`` is the relay and the sheet cache, for a web host rather " +
             "than a venue machine. Only needed if you want the estate view; templates and " +
             "updates work without it. Deploy every file in it, the dotfiles included."
    & gh release create $tag $zipPath $sumsPath $bootstrapPath $phpPath --repo $Repo --title "Build $build" --notes $notes
    if ($LASTEXITCODE -ne 0) { Fail "gh release create failed" }
}

# ---- and check it is actually all there -------------------------------------
#
# gh reporting success is not the same as four assets being on the release. An
# upload can fail per file, and a release missing the PHP or the standalone
# updater looks complete until the day somebody needs the missing one - which,
# for the updater, is the day they are on an old build and cannot install
# anything. Cheaper to find out now than at a venue.

Write-Host ""
Write-Host "  checking what actually landed..."

$published = & gh release view $tag --repo $Repo --json assets --jq '.assets[].name'
if ($LASTEXITCODE -ne 0) { Fail "Published, but could not read the release back to check it." }

$landed = @($published -split "`n" | Where-Object { $_ -ne "" })

$expected = @(
    @{ Name = $assetName;          What = "the client build" },
    @{ Name = "SHA256SUMS.txt";    What = "the checksums, without which a client refuses to install" },
    @{ Name = "install-update.cmd"; What = "the standalone updater for clients older than build 210" },
    @{ Name = "server-php.zip";    What = "the relay and sheet cache" }
)

$missing = @()
foreach ($item in $expected) {
    if ($landed -contains $item.Name) {
        Write-Host ("    ok   " + $item.Name)
    } else {
        Write-Host ("    MISSING  " + $item.Name + "  - " + $item.What) -ForegroundColor Red
        $missing += $item.Name
    }
}

if ($missing.Count -gt 0) {
    Write-Host ""
    Write-Host "  The release exists but is incomplete. Re-run with -Force to replace its assets." -ForegroundColor Red
    exit 1
}

Remove-Item $staging -Recurse -Force

Write-Host ""
Write-Host "  published $tag to $Repo" -ForegroundColor Green
Write-Host "  clients pointed at $Repo will see it on their next check."
