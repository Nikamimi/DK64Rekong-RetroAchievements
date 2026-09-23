param(
    [string]$Label = '0.2.1-private-smoke'
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$build = Join-Path $root 'build'
$nrm = Join-Path $build 'dk64_ra_probe.nrm'
$so = Join-Path $build 'native-linux/dk64_ra_probe.so'
$guide = Join-Path $root 'docs/private-linux-tester-guide.md'
$notices = Join-Path $root 'THIRD_PARTY_NOTICES.md'
$files = [ordered]@{
    'dk64_ra_probe.nrm' = $nrm
    'dk64_ra_probe.so' = $so
    'TESTER_README.md' = $guide
    'THIRD_PARTY_NOTICES.md' = $notices
}
foreach ($source in $files.Values) {
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Missing package input: $source"
    }
}

# Refuse to ship binaries older than their inputs. Rebuild and rerun this script.
$nativeSources = Get-ChildItem -LiteralPath (Join-Path $root 'native') -File |
    Where-Object { $_.Extension -in '.cpp', '.h', '.txt' }
if ($nativeSources | Where-Object { $_.LastWriteTimeUtc -gt (Get-Item -LiteralPath $so).LastWriteTimeUtc }) {
    throw 'Linux .so is older than a native source/build file; rebuild it first.'
}
$guestSources = @(Join-Path $root 'mod.toml') +
    @(Get-ChildItem -LiteralPath (Join-Path $root 'src'), (Join-Path $root 'include') -File |
        ForEach-Object FullName)
if ($guestSources | Where-Object {
        (Get-Item -LiteralPath $_).LastWriteTimeUtc -gt (Get-Item -LiteralPath $nrm).LastWriteTimeUtc
    }) {
    throw 'The .nrm is older than a guest source/manifest file; rebuild it first.'
}

$date = Get-Date -Format 'yyyyMMdd'
$archivePath = Join-Path $build "dk64-ra-linux-x86_64-glibc238-$Label-$date.zip"
if (Test-Path -LiteralPath $archivePath) {
    throw "A package already exists; do not silently replace a tester artifact: $archivePath"
}

$archive = [System.IO.Compression.ZipFile]::Open($archivePath,
    [System.IO.Compression.ZipArchiveMode]::Create)
try {
    $hashLines = foreach ($entryName in $files.Keys) {
        $source = $files[$entryName]
        [void][System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $archive, $source, $entryName, [System.IO.Compression.CompressionLevel]::Optimal)
        '{0}  {1}' -f (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant(),
                       $entryName
    }
    $entry = $archive.CreateEntry('SHA256SUMS.txt')
    $writer = [System.IO.StreamWriter]::new($entry.Open(),
        [System.Text.UTF8Encoding]::new($false))
    try { $writer.Write(($hashLines -join "`n") + "`n") }
    finally { $writer.Dispose() }
}
finally { $archive.Dispose() }

$check = [System.IO.Compression.ZipFile]::OpenRead($archivePath)
try {
    $actual = @($check.Entries | ForEach-Object FullName | Sort-Object)
    $expected = @(($files.Keys + @('SHA256SUMS.txt')) | Sort-Object)
    if (@(Compare-Object $actual $expected).Count -ne 0 -or
        @($check.Entries | Where-Object Length -eq 0).Count -ne 0) {
        throw "Unexpected or empty ZIP entry in $archivePath"
    }
}
finally { $check.Dispose() }

Write-Output $archivePath
Write-Output "SHA256 $((Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant())"
