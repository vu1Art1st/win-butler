[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$BuildLabel,
    [string]$OutputDir = 'artifacts\vm',
    [switch]$RunFixtures,
    [switch]$AllowBuildMismatch
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$matrix = Get-Content -Raw -LiteralPath (Join-Path $root 'vm\matrix.json') | ConvertFrom-Json
$target = $matrix.targets | Where-Object id -eq $BuildLabel | Select-Object -First 1
if (-not $target) {
    throw "Unknown build label: $BuildLabel"
}

$os = Get-CimInstance Win32_OperatingSystem
$buildNumber = [Environment]::OSVersion.Version.Build
$reportDir = Join-Path $root (Join-Path $OutputDir $BuildLabel)
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
$report = [ordered]@{
    target = $target
    startedAt = (Get-Date).ToString('o')
    computer = $env:COMPUTERNAME
    osCaption = $os.Caption
    osBuild = $buildNumber
    architecture = $env:PROCESSOR_ARCHITECTURE
    buildMatches = $buildNumber.ToString().StartsWith($target.buildPrefix)
    scannerSmoke = $false
    fixtureTests = $false
    errors = @()
}

if (-not $report.buildMatches -and -not $AllowBuildMismatch) {
    throw "OS build $buildNumber does not match target $($target.buildPrefix). Use -AllowBuildMismatch only for diagnostics."
}

try {
    $testExe = Join-Path $root 'build-shell\Release\win_butler_tests.exe'
    $scannerExe = Join-Path $root 'build-shell\Release\win_butler_scan.exe'
    & $testExe
    if ($LASTEXITCODE -ne 0) {
        throw "win_butler_tests.exe failed with exit code $LASTEXITCODE"
    }

    $summary = & $scannerExe --summary
    $summary | Set-Content -LiteralPath (Join-Path $reportDir 'scanner-summary.txt') -Encoding utf8
    $scanJson = & $scannerExe | ConvertFrom-Json
    $scanJson | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $reportDir 'scan.json') -Encoding utf8
    if ($scanJson.items.Count -eq 0) {
        throw 'Read-only scan returned no items.'
    }
    $report.scannerSmoke = $true

    if ($RunFixtures) {
        & (Join-Path $root 'scripts\Test-Fixtures.ps1') `
            -ScannerPath $scannerExe `
            -OutputDir (Join-Path $OutputDir "$BuildLabel\fixtures")
        $report.fixtureTests = $true
    }
}
catch {
    $report.errors += $_.Exception.Message
    throw
}
finally {
    $report.finishedAt = (Get-Date).ToString('o')
    $report | ConvertTo-Json -Depth 10 |
        Set-Content -LiteralPath (Join-Path $reportDir 'report.json') -Encoding utf8
}
