param([string]$SourcePath)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$destination = Join-Path $projectRoot 'third_party\raylib'
$marker = Join-Path $destination '5.5.ready'
if (Test-Path -LiteralPath $marker) { return }

$downloadFolder = $null
try {
    if (!$SourcePath) {
        $downloadFolder = Join-Path ([System.IO.Path]::GetTempPath()) ('MazeGame-raylib-' + [guid]::NewGuid().ToString('N'))
        New-Item -ItemType Directory -Path $downloadFolder | Out-Null
        $archive = Join-Path $downloadFolder 'raylib.zip'
        Write-Host 'Downloading raylib 5.5 from its official repository...'
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -UseBasicParsing -Uri 'https://github.com/raysan5/raylib/archive/refs/tags/5.5.zip' -OutFile $archive
        Expand-Archive -LiteralPath $archive -DestinationPath $downloadFolder
        $SourcePath = Join-Path $downloadFolder 'raylib-5.5'
    }
    $header = Join-Path $SourcePath 'src\raylib.h'
    if (!(Test-Path -LiteralPath $header) -or
        !(Select-String -LiteralPath $header -Pattern '^#define RAYLIB_VERSION\s+"5\.5"')) {
        throw 'Expected raylib 5.5 sources containing src\raylib.h.'
    }
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $SourcePath 'src'),(Join-Path $SourcePath 'LICENSE') -Destination $destination -Recurse -Force
    Set-Content -LiteralPath $marker -Value 'raylib 5.5' -Encoding Ascii
    Write-Host 'raylib 5.5 is ready.'
} finally {
    if ($downloadFolder) {
        $resolvedDownload = [System.IO.Path]::GetFullPath($downloadFolder)
        $tempPrefix = Join-Path ([System.IO.Path]::GetTempPath()) 'MazeGame-raylib-'
        if (!$resolvedDownload.StartsWith($tempPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw 'Refusing to clean a directory outside the raylib temporary folder.'
        }
        Remove-Item -LiteralPath $resolvedDownload -Recurse -Force
    }
}
