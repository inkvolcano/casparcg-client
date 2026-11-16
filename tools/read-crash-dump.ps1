# Read a crash dump of the client: where it crashed, by function and line.
#
#   powershell -ExecutionPolicy Bypass -File tools\read-crash-dump.ps1 -Dump <file.dmp> -Build 293
#
# The dump comes from %LOCALAPPDATA%\CrashDumps on the machine that crashed, or
# from the "dumps" folder of a bug report (Help > Bug Report > Collect a Bug
# Report). The build number is the one in that machine's title bar.
#
# A dump only names functions when it is read with the exact .pdb of the build
# that crashed, and a venue machine has none. Since build 294 every release
# carries them as casparcg-client-<tag>-symbols.zip; this fetches that and the
# client package of the same release into %TEMP%\casparcg-dumps\<tag> (once,
# then reused) and asks the Windows debugger (cdb, in the Windows SDK's
# Debugging Tools) for the crashing thread's stack.
#
# Build 289's startup crash was found this way by hand. Read-only: it changes
# nothing on this machine but that one cache folder.

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $Dump,
    [Parameter(Mandatory = $true)] [int] $Build,

    # owner/repo the release is in. Defaults to this repository's origin.
    [string] $Repo = "",

    # The client's version, for the tag (v2.3.1-<build>).
    [string] $Version = "2.3.1"
)

$ErrorActionPreference = "Stop"

function Fail($message) {
    Write-Host "  ! $message" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $Dump)) { Fail "No dump at $Dump." }

$cdb = @(
    "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64\cdb.exe",
    "$env:ProgramFiles\Windows Kits\10\Debuggers\x64\cdb.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $cdb) { Fail "cdb.exe not found. Install 'Debugging Tools for Windows' from the Windows SDK." }

if ($Repo -eq "") {
    $origin = (& git -C $PSScriptRoot remote get-url origin) 2>$null
    if ($origin -match 'github\.com[:/](.+?)(\.git)?$') { $Repo = $Matches[1] }
}
if ($Repo -eq "") { Fail "No -Repo given and no GitHub origin to take it from." }

$tag = "v$Version-$Build"
$cache = Join-Path $env:TEMP "casparcg-dumps\$tag"
$symbolsZip = "casparcg-client-$tag-symbols.zip"
$clientZip = "casparcg-client-$tag-windows.zip"

if (-not (Test-Path (Join-Path $cache "casparcg-client.pdb"))) {
    New-Item -ItemType Directory -Path $cache -Force | Out-Null
    Write-Host "  fetching $tag from $Repo..."

    & gh release download $tag --repo $Repo --dir $cache --pattern $symbolsZip --pattern $clientZip --clobber
    if ($LASTEXITCODE -ne 0) {
        Fail "Could not download $tag. Releases before build 294 carry no symbols: use the .pdb kept in that build's release folder, with cdb -y."
    }

    Expand-Archive -Path (Join-Path $cache $symbolsZip) -DestinationPath $cache -Force
    Expand-Archive -Path (Join-Path $cache $clientZip) -DestinationPath $cache -Force

    # The package holds one folder named for the build; the debugger wants the
    # executable and its .pdb side by side.
    $inner = Get-ChildItem $cache -Directory | Where-Object { Test-Path (Join-Path $_.FullName "casparcg-client.exe") } | Select-Object -First 1
    if ($inner) { Copy-Item (Join-Path $inner.FullName "*") $cache -Recurse -Force }
}

if (-not (Test-Path (Join-Path $cache "casparcg-client.pdb"))) { Fail "The symbols of $tag did not unpack into $cache." }

Write-Host "  reading $Dump with the symbols of $tag..."
Write-Host ""

# .lines -e: file and line beside each frame. .ecxr: the thread and registers at
# the moment of the crash, not the thread that wrote the dump. kn 30: its 30
# nearest frames, numbered. Checked against a dump of a deliberate null read
# (2026-11-16): "readValue+0xa [crash.cpp @ 3]", called from "main [@ 4]".
& $cdb -z $Dump -y "$cache;srv*" -i $cache -c ".lines -e; .ecxr; kn 30; q" 2>&1 |
    Where-Object { $_ -notmatch '^(NatVis|\*\*\*|Symbol search|Executable search|Microsoft \(R\)|Copyright)' }
