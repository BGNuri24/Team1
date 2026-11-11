param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$Vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$Msbuild = $null
if (Test-Path -LiteralPath $Vswhere) {
    $Msbuild = & $Vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find 'MSBuild\Current\Bin\MSBuild.exe' | Select-Object -First 1
}
if (!$Msbuild) {
    $Command = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($Command) { $Msbuild = $Command.Source }
}
if (!$Msbuild) { throw 'Install Visual Studio with Desktop development with C++, then retry.' }

& $Msbuild (Join-Path $PSScriptRoot 'MazeGame.vcxproj') /m "/p:Configuration=$Configuration" /p:Platform=x64 /verbosity:minimal /nologo
exit $LASTEXITCODE
