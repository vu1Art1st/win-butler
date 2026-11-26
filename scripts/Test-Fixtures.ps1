[CmdletBinding()]
param(
    [string]$ScannerPath = '',
    [string]$OutputDir = 'artifacts\fixture-tests'
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ([string]::IsNullOrWhiteSpace($ScannerPath)) {
    $ScannerPath = Join-Path $root 'build-shell\Release\win_butler_scan.exe'
}
$ScannerPath = (Resolve-Path -LiteralPath $ScannerPath).Path
New-Item -ItemType Directory -Force -Path (Join-Path $root $OutputDir) | Out-Null

function Invoke-ScanJson {
    param([string]$OutputFile)
    $json = & $ScannerPath | ConvertFrom-Json
    $json | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $OutputFile -Encoding utf8
    return $json
}

$classicClsid = '{6D7B8E3A-5A1B-4E71-8B1D-6C7F0A2E9D11}'
$report = [ordered]@{
    startedAt = (Get-Date).ToString('o')
    scanner = $ScannerPath
    classic = $false
    shellex = $false
    residualAfterCleanup = $true
}

try {
    & (Join-Path $root 'fixtures\classic-shell\Install.ps1')
    & (Join-Path $root 'fixtures\shellex\Install.ps1')

    $scan = Invoke-ScanJson (Join-Path $root "$OutputDir\fixture-scan.json")
    $classicItems = @($scan.items | Where-Object { $_.displayName -eq 'win-butler classic fixture' })
    $shellexItems = @($scan.items | Where-Object {
        $_.clsid -eq $classicClsid -or $_.displayName -eq 'WinButlerFixture'
    })

    $report.classic = $classicItems.Count -gt 0
    $report.shellex = $shellexItems.Count -gt 0
    if (-not $report.classic -or -not $report.shellex) {
        throw 'Fixture scan assertions failed.'
    }
}
finally {
    & (Join-Path $root 'fixtures\shellex\Install.ps1') -Uninstall
    & (Join-Path $root 'fixtures\classic-shell\Install.ps1') -Uninstall
    $post = Invoke-ScanJson (Join-Path $root "$OutputDir\fixture-cleanup-scan.json")
    $residual = @($post.items | Where-Object {
        $_.displayName -eq 'win-butler classic fixture' -or $_.clsid -eq $classicClsid
    })
    $report.residualAfterCleanup = $residual.Count -eq 0
}

$report.finishedAt = (Get-Date).ToString('o')
$reportPath = Join-Path $root "$OutputDir\report.json"
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath -Encoding utf8
$report | Format-List

if (-not $report.residualAfterCleanup) {
    throw 'Fixture cleanup left residual registry items.'
}
