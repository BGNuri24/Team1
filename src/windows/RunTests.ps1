param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output = Join-Path $projectRoot "build-vs\x64\$Configuration"
foreach ($test in @('MazeGameTests', 'MazeGameLogicTests', 'MazeInputTests')) {
    $executable = Join-Path $output "$test.exe"
    if (!(Test-Path -LiteralPath $executable)) { throw "Build the solution in $Configuration | x64 first: $executable" }
    $testDirectory = Join-Path $output "test-data\$test"
    New-Item -ItemType Directory -Path $testDirectory -Force | Out-Null
    Push-Location $testDirectory
    try {
        & $executable
        if ($LASTEXITCODE -ne 0) { throw "$test failed with exit code $LASTEXITCODE" }
    } finally {
        Pop-Location
    }
}
Write-Host 'All three test suites passed.'
