param([Parameter(Mandatory = $true)][string]$Destination)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

function Test-RaylibSource([string]$Path) {
    $requiredFiles = @(
        'LICENSE', 'src\raylib.h', 'src\config.h', 'src\raymath.h', 'src\rlgl.h',
        'src\raudio.c', 'src\rcore.c', 'src\rmodels.c', 'src\rglfw.c',
        'src\rshapes.c', 'src\rtext.c', 'src\rtextures.c', 'src\utils.c',
        'src\platforms\rcore_desktop_glfw.c',
        'src\external\glfw\include\GLFW\glfw3.h'
    )
    foreach ($file in $requiredFiles) {
        if (!(Test-Path -LiteralPath (Join-Path $Path $file) -PathType Leaf)) { return $false }
    }
    return [bool](Select-String -LiteralPath (Join-Path $Path 'src\raylib.h') -Pattern '^#define RAYLIB_VERSION\s+"5\.5"' -Quiet)
}

function Remove-RestoreDirectory([string]$Path, [string]$Parent) {
    if (!$Path -or !(Test-Path -LiteralPath $Path)) { return }
    $resolvedPath = [System.IO.Path]::GetFullPath($Path)
    $expectedParent = [System.IO.Path]::GetFullPath($Parent).TrimEnd('\', '/')
    if (![string]::Equals([System.IO.Path]::GetDirectoryName($resolvedPath), $expectedParent, [System.StringComparison]::OrdinalIgnoreCase) -or
        ![System.IO.Path]::GetFileName($resolvedPath).StartsWith('MazeGame-raylib-', [System.StringComparison]::Ordinal)) {
        throw "Refusing to clean an unexpected raylib setup directory: $resolvedPath"
    }
    Remove-Item -LiteralPath $resolvedPath -Recurse -Force
}

$Destination = [System.IO.Path]::GetFullPath($Destination)
if (Test-RaylibSource $Destination) {
    Write-Host 'Using cached raylib 5.5.'
    return
}

$cacheParent = Split-Path $Destination -Parent
New-Item -ItemType Directory -Path $cacheParent -Force | Out-Null

# Serialise first-time setup when several checkouts build at the same time.
$lockPath = $Destination + '.restore.lock'
$lock = $null
$deadline = [DateTime]::UtcNow.AddSeconds(120)
while (!$lock) {
    try {
        $lock = [System.IO.File]::Open($lockPath, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
    } catch [System.IO.IOException] {
        if ([DateTime]::UtcNow -ge $deadline) { throw "Timed out waiting for raylib setup. Retry the build when the other setup finishes." }
        Start-Sleep -Milliseconds 250
    }
}

$temporaryFolder = $null
$stagingFolder = $null
try {
    if (Test-RaylibSource $Destination) {
        Write-Host 'Using cached raylib 5.5.'
        return
    }
    if (Test-Path -LiteralPath $Destination) {
        throw "An incomplete raylib folder exists at '$Destination'. Move it aside and retry, or set RAYLIB_SOURCE to a complete raylib 5.5 checkout."
    }

    $folderName = 'MazeGame-raylib-' + [guid]::NewGuid().ToString('N')
    $temporaryFolder = Join-Path ([System.IO.Path]::GetTempPath()) $folderName
    New-Item -ItemType Directory -Path $temporaryFolder | Out-Null
    $archive = Join-Path $temporaryFolder 'raylib-5.5.zip'

    Write-Host 'First build: downloading raylib 5.5. Later builds will reuse the local cache.'
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -UseBasicParsing -Uri 'https://github.com/raysan5/raylib/archive/refs/tags/5.5.zip' -OutFile $archive -TimeoutSec 120
    Expand-Archive -LiteralPath $archive -DestinationPath $temporaryFolder
    $extractedSource = Join-Path $temporaryFolder 'raylib-5.5'
    if (!(Test-RaylibSource $extractedSource)) { throw 'The downloaded archive does not contain complete raylib 5.5 sources.' }

    # Install only after extraction succeeds, so an interrupted download can be retried.
    $stagingFolder = Join-Path $cacheParent $folderName
    New-Item -ItemType Directory -Path $stagingFolder | Out-Null
    Copy-Item -LiteralPath (Join-Path $extractedSource 'src'), (Join-Path $extractedSource 'LICENSE') -Destination $stagingFolder -Recurse
    [System.IO.Directory]::Move($stagingFolder, $Destination)
    Write-Host "raylib 5.5 is ready at $Destination"
} catch {
    throw "raylib setup failed: $($_.Exception.Message) The first build needs internet access. For offline builds, set RAYLIB_SOURCE to an existing raylib 5.5 source directory."
} finally {
    try {
        Remove-RestoreDirectory $temporaryFolder ([System.IO.Path]::GetTempPath())
        Remove-RestoreDirectory $stagingFolder $cacheParent
    } finally {
        $lock.Dispose()
    }
}
