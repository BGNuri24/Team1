param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$ProjectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$Output = Join-Path $ProjectRoot "build-vs\x64\$Configuration"
foreach ($Test in @('MazeGameTests', 'MazeGameLogicTests', 'MazeInputTests')) {
    $Executable = Join-Path $Output "$Test.exe"
    if (!(Test-Path -LiteralPath $Executable)) { throw "Build the solution in $Configuration | x64 first: $Executable" }
    $TestDirectory = Join-Path $Output "test-data\$Test"
    New-Item -ItemType Directory -Path $TestDirectory -Force | Out-Null
    Push-Location $TestDirectory
    try {
        & $Executable
        if ($LASTEXITCODE -ne 0) { throw "$Test failed with exit code $LASTEXITCODE" }
    } finally {
        Pop-Location
    }
}
Write-Host 'All three test suites passed.'
