param([Parameter(Mandatory = $true)][string]$Destination)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

# Validate that a candidate directory contains every required raylib source file and declares
# version 5.5. Return a boolean without changing the directory so both cached and downloaded
# dependencies use the same acceptance check.
function TestRaylibSource([string]$Path) {
    $RequiredFiles = @(
        'LICENSE', 'src\raylib.h', 'src\config.h', 'src\raymath.h', 'src\rlgl.h',
        'src\raudio.c', 'src\rcore.c', 'src\rmodels.c', 'src\rglfw.c',
        'src\rshapes.c', 'src\rtext.c', 'src\rtextures.c', 'src\utils.c',
        'src\platforms\rcore_desktop_glfw.c',
        'src\external\glfw\include\GLFW\glfw3.h'
    )
    foreach ($File in $RequiredFiles) {
        if (!(Test-Path -LiteralPath (Join-Path $Path $File) -PathType Leaf)) { return $false }
    }
    return [bool](Select-String -LiteralPath (Join-Path $Path 'src\raylib.h') -Pattern '^#define RAYLIB_VERSION\s+"5\.5"' -Quiet)
}

# Remove only a temporary restore directory whose resolved parent and generated-name prefix match
# the expected location. Reject unexpected paths before recursive cleanup so failed dependency
# setup cannot delete unrelated files.
function RemoveRestoreDirectory([string]$Path, [string]$Parent) {
    if (!$Path -or !(Test-Path -LiteralPath $Path)) { return }
    $ResolvedPath = [System.IO.Path]::GetFullPath($Path)
    $ExpectedParent = [System.IO.Path]::GetFullPath($Parent).TrimEnd('\', '/')
    if (![string]::Equals([System.IO.Path]::GetDirectoryName($ResolvedPath), $ExpectedParent, [System.StringComparison]::OrdinalIgnoreCase) -or
        ![System.IO.Path]::GetFileName($ResolvedPath).StartsWith('MazeGame-raylib-', [System.StringComparison]::Ordinal)) {
        throw "Refusing to clean an unexpected raylib setup directory: $ResolvedPath"
    }
    Remove-Item -LiteralPath $ResolvedPath -Recurse -Force
}

$Destination = [System.IO.Path]::GetFullPath($Destination)
if (TestRaylibSource $Destination) {
    Write-Host 'Using cached raylib 5.5.'
    return
}

$CacheParent = Split-Path $Destination -Parent
New-Item -ItemType Directory -Path $CacheParent -Force | Out-Null

# Hold an exclusive cache lock so simultaneous builds cannot install raylib into the same destination.
$LockPath = $Destination + '.restore.lock'
$Lock = $null
$Deadline = [DateTime]::UtcNow.AddSeconds(120)
while (!$Lock) {
    try {
        $Lock = [System.IO.File]::Open($LockPath, [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
    } catch [System.IO.IOException] {
        if ([DateTime]::UtcNow -ge $Deadline) { throw "Timed out waiting for raylib setup. Retry the build when the other setup finishes." }
        Start-Sleep -Milliseconds 250
    }
}

$TemporaryFolder = $null
$StagingFolder = $null
try {
    if (TestRaylibSource $Destination) {
        Write-Host 'Using cached raylib 5.5.'
        return
    }
    if (Test-Path -LiteralPath $Destination) {
        throw "An incomplete raylib folder exists at '$Destination'. Move it aside and retry, or set RAYLIB_SOURCE to a complete raylib 5.5 checkout."
    }

    $FolderName = 'MazeGame-raylib-' + [guid]::NewGuid().ToString('N')
    $TemporaryFolder = Join-Path ([System.IO.Path]::GetTempPath()) $FolderName
    New-Item -ItemType Directory -Path $TemporaryFolder | Out-Null
    $Archive = Join-Path $TemporaryFolder 'raylib-5.5.zip'

    Write-Host 'First build: downloading raylib 5.5. Later builds will reuse the local cache.'
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -UseBasicParsing -Uri 'https://github.com/raysan5/raylib/archive/refs/tags/5.5.zip' -OutFile $Archive -TimeoutSec 120
    Expand-Archive -LiteralPath $Archive -DestinationPath $TemporaryFolder
    $ExtractedSource = Join-Path $TemporaryFolder 'raylib-5.5'
    if (!(TestRaylibSource $ExtractedSource)) { throw 'The downloaded archive does not contain complete raylib 5.5 sources.' }

    # Stage the validated source beside the cache and publish it with a directory move only after copying
    # completes.
    $StagingFolder = Join-Path $CacheParent $FolderName
    New-Item -ItemType Directory -Path $StagingFolder | Out-Null
    Copy-Item -LiteralPath (Join-Path $ExtractedSource 'src'), (Join-Path $ExtractedSource 'LICENSE') -Destination $StagingFolder -Recurse
    [System.IO.Directory]::Move($StagingFolder, $Destination)
    Write-Host "raylib 5.5 is ready at $Destination"
} catch {
    throw "raylib setup failed: $($_.Exception.Message) The first build needs internet access. For offline builds, set RAYLIB_SOURCE to an existing raylib 5.5 source directory."
} finally {
    try {
        RemoveRestoreDirectory $TemporaryFolder ([System.IO.Path]::GetTempPath())
        RemoveRestoreDirectory $StagingFolder $CacheParent
    } finally {
        $Lock.Dispose()
    }
}
