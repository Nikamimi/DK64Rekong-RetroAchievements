param(
    [ValidatePattern('^v[0-9]+\.[0-9]+\.[0-9]+(-beta\.[0-9]+)?$')]
    [string]$Tag = 'v0.2.2'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$build = Join-Path $root 'build'
$sourceCommit = git -C $root rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $sourceCommit -notmatch '^[a-f0-9]{40}$') {
    throw 'Cannot resolve the source commit.'
}
$dirty = git -C $root status --porcelain=v1
if ($LASTEXITCODE -ne 0 -or $dirty) { throw 'Commit source changes before packaging a release.' }
$manifest = Get-Content -LiteralPath (Join-Path $root 'mod.toml') -Raw
if ($manifest -notmatch '(?m)^version\s*=\s*"([^"]+)"') { throw 'Missing mod version.' }
$modVersion = $Matches[1]
if ($Tag -ne "v$modVersion" -and -not $Tag.StartsWith("v$modVersion-beta.")) {
    throw 'Tag and mod version differ.'
}

$nrm = Join-Path $build 'dk64_ra_probe.nrm'
$targets = @(
    @{ Name = 'windows-x86_64'; Binary = 'native-win/Release/dk64_ra_probe.dll';
       Native = 'dk64_ra_probe.dll'; Guide = 'private-tester-guide.md'; Cache = 'native-win/CMakeCache.txt' },
    @{ Name = 'linux-x86_64-glibc238'; Binary = 'native-linux/dk64_ra_probe.so';
       Native = 'dk64_ra_probe.so'; Guide = 'private-linux-tester-guide.md'; Cache = 'native-linux/CMakeCache.txt' }
)
$nativeInputs = @(Get-ChildItem -LiteralPath (Join-Path $root 'native'), (Join-Path $root 'include') -Recurse -File |
    Where-Object { $_.Extension -in '.cpp', '.h', '.txt' })
$guestInputs = @(Get-Item -LiteralPath (Join-Path $root 'mod.toml'),
    (Join-Path $root 'mod.ld'), (Join-Path $root 'Makefile'), (Join-Path $root 'thumb.png')) +
    @(Get-ChildItem -LiteralPath (Join-Path $root 'src'), (Join-Path $root 'include') -Recurse -File)
if (-not (Test-Path -LiteralPath $nrm -PathType Leaf)) { throw 'Build the .nrm first.' }
if ($guestInputs | Where-Object LastWriteTimeUtc -gt (Get-Item -LiteralPath $nrm).LastWriteTimeUtc) {
    throw 'The .nrm is older than its inputs; rebuild it first.'
}
foreach ($target in $targets) {
    $binary = Join-Path $build $target.Binary
    foreach ($inputPath in @($binary, (Join-Path $root "docs/$($target.Guide)"),
                            (Join-Path $root 'THIRD_PARTY_NOTICES.md'))) {
        if (-not (Test-Path -LiteralPath $inputPath -PathType Leaf)) { throw "Missing input: $inputPath" }
    }
    if ($nativeInputs | Where-Object LastWriteTimeUtc -gt (Get-Item -LiteralPath $binary).LastWriteTimeUtc) {
        throw "$($target.Name) binary is stale; rebuild it first."
    }
    $cache = Get-Content -LiteralPath (Join-Path $build $target.Cache) -Raw
    foreach ($setting in @('DK64_RA_MEMORY_DIAGNOSTICS', 'DK64_RA_RAP_DIAGNOSTICS')) {
        if ($cache -notmatch "(?m)^${setting}:BOOL=OFF\r?$") { throw "Disable $setting before packaging." }
    }
}

# A stable asset name lets the mod-list configuration keep working on future
# releases. The loader selects the companion for its OS; users may also choose
# either smaller platform-specific archive.
if ($Tag -eq "v$modVersion") {
    $combinedGuide = Join-Path $root "docs/releases/$Tag.md"
    if (-not (Test-Path -LiteralPath $combinedGuide -PathType Leaf)) {
        throw "Missing combined-package guide: $combinedGuide"
    }
    $targets += @{ Name = 'windows-linux-x86_64'; Combined = $true }
}

$output = Join-Path $build "releases/$Tag"
if (Test-Path -LiteralPath $output) { throw "Refusing to overwrite an existing release directory: $output" }
[void](New-Item -ItemType Directory -Path $output)
$utf8 = [System.Text.UTF8Encoding]::new($false)

function Add-TextEntry($archive, [string]$name, [string]$content) {
    $writer = [System.IO.StreamWriter]::new($archive.CreateEntry($name).Open(), $utf8)
    try { $writer.Write($content) } finally { $writer.Dispose() }
}

function Get-StreamHash($stream) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
}

$archiveHashes = foreach ($target in $targets) {
    $combined = $target.ContainsKey('Combined')
    $archiveName = if ($combined) { 'dk64-ra.zip' } else { "dk64-ra-$($target.Name)-$Tag.zip" }
    $archivePath = Join-Path $output $archiveName
    # Never enumerate a build directory for archive contents: logs, ROMs and
    # private state must not enter a distributable.
    $files = [ordered]@{
        'dk64_ra_probe.nrm' = $nrm
    }
    if ($combined) {
        $files['dk64_ra_probe.dll'] = Join-Path $build 'native-win/Release/dk64_ra_probe.dll'
        $files['dk64_ra_probe.so'] = Join-Path $build 'native-linux/dk64_ra_probe.so'
        $files['TESTER_README.md'] = $combinedGuide
        $files['WINDOWS_README.md'] = Join-Path $root 'docs/private-tester-guide.md'
        $files['LINUX_README.md'] = Join-Path $root 'docs/private-linux-tester-guide.md'
    } else {
        $files[$target.Native] = Join-Path $build $target.Binary
        $files['TESTER_README.md'] = Join-Path $root "docs/$($target.Guide)"
    }
    $files['THIRD_PARTY_NOTICES.md'] = Join-Path $root 'THIRD_PARTY_NOTICES.md'
    $info = "Tag: $Tag`nMod version: $modVersion`nPlatform: $($target.Name)`nSource commit: $sourceCommit`nSource: https://github.com/Nikamimi/DK64Rekong-RetroAchievements/tree/$sourceCommit`nSee TESTER_README.md for installation, validation and known limits.`n"
    $expectedHashes = [ordered]@{}
    foreach ($name in $files.Keys) {
        $expectedHashes[$name] = (Get-FileHash -LiteralPath $files[$name] -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    $infoStream = [System.IO.MemoryStream]::new($utf8.GetBytes($info))
    try { $expectedHashes['RELEASE_INFO.txt'] = Get-StreamHash $infoStream }
    finally { $infoStream.Dispose() }
    $sums = (@($expectedHashes.Keys | ForEach-Object { "$($expectedHashes[$_])  $_" }) -join "`n") + "`n"
    $archive = [System.IO.Compression.ZipFile]::Open($archivePath, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in $files.Keys) {
            [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                $archive, $files[$name], $name, [System.IO.Compression.CompressionLevel]::Optimal)
        }
        Add-TextEntry $archive 'RELEASE_INFO.txt' $info
        Add-TextEntry $archive 'SHA256SUMS.txt' $sums
    } finally { $archive.Dispose() }

    $check = [System.IO.Compression.ZipFile]::OpenRead($archivePath)
    try {
        $actual = @($check.Entries | ForEach-Object FullName | Sort-Object)
        $expected = @(@($expectedHashes.Keys) + @('SHA256SUMS.txt') | Sort-Object)
        if (@(Compare-Object $actual $expected).Count -ne 0 -or
            @($check.Entries | Where-Object Length -eq 0).Count -ne 0) { throw 'Unexpected ZIP contents.' }
        foreach ($name in $expectedHashes.Keys) {
            $stream = $check.GetEntry($name).Open()
            try { if ((Get-StreamHash $stream) -ne $expectedHashes[$name]) { throw "ZIP hash mismatch: $name" } }
            finally { $stream.Dispose() }
        }
        $reader = [System.IO.StreamReader]::new($check.GetEntry('SHA256SUMS.txt').Open())
        try { if ($reader.ReadToEnd() -cne $sums) { throw 'ZIP checksum manifest mismatch.' } }
        finally { $reader.Dispose() }
    } finally { $check.Dispose() }
    '{0}  {1}' -f (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant(),
                   [System.IO.Path]::GetFileName($archivePath)
}
[System.IO.File]::WriteAllText((Join-Path $output 'SHA256SUMS.txt'), ($archiveHashes -join "`n") + "`n", $utf8)
Write-Output $output
Write-Output $archiveHashes
