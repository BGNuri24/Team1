param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = $null
if (Test-Path -LiteralPath $vswhere) {
    $msbuild = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\Current\Bin\MSBuild.exe' | Select-Object -First 1
}
if (!$msbuild) {
    $command = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($command) { $msbuild = $command.Source }
}
if (!$msbuild) { throw 'Install Visual Studio with Desktop development with C++, then retry.' }

& $msbuild (Join-Path $PSScriptRoot 'MazeGame.vcxproj') /m "/p:Configuration=$Configuration" /p:Platform=x64 /verbosity:minimal /nologo
exit $LASTEXITCODE
