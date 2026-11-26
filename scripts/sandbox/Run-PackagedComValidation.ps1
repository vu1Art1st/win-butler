[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$clsid = '{4F5F6A7B-8C9D-4E0F-8123-456789ABCDEF}'
$blockedPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Blocked'
$artifactRoot = 'C:\winbutler-artifacts'
$resultRoot = Join-Path $artifactRoot 'sandbox-results'
$scanner = 'C:\winbutler-src\build-release\Release\win_butler_scan.exe'
$msix = Join-Path $artifactRoot 'win-butler-contextmenu-fixture.msix'
$cer = Join-Path $artifactRoot 'win-butler-test-signing.cer'
$report = [ordered]@{
    startedAt = (Get-Date).ToString('o')
    osBuild = [Environment]::OSVersion.Version.Build
    packageInstalled = $false
    foundAfterInstall = $false
    stateAfterInstall = $null
    stateAfterBlock = $null
    stateAfterUnblock = $null
    explorerRestartAttempted = $false
    errors = @()
}

New-Item -ItemType Directory -Force -Path $resultRoot | Out-Null

function Invoke-ScanFixture {
    $scan = & $scanner | ConvertFrom-Json
    return @($scan.items | Where-Object { $_.clsid -eq $clsid }) | Select-Object -First 1
}

try {
    try {
        Import-Certificate -FilePath $cer -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople' | Out-Null
    } catch {
        Import-Certificate -FilePath $cer -CertStoreLocation 'Cert:\CurrentUser\TrustedPeople' | Out-Null
    }

    Add-AppxPackage -Path $msix
    $package = Get-AppxPackage -Name 'WinButler.ContextMenuFixture'
    $report.packageInstalled = $null -ne $package

    $afterInstall = Invoke-ScanFixture
    $report.foundAfterInstall = $null -ne $afterInstall
    $report.stateAfterInstall = if ($afterInstall) { $afterInstall.state } else { $null }

    New-Item -Path $blockedPath -Force | Out-Null
    New-ItemProperty -Path $blockedPath -Name $clsid -PropertyType String -Value '' -Force | Out-Null
    $afterBlock = Invoke-ScanFixture
    $report.stateAfterBlock = if ($afterBlock) { $afterBlock.state } else { $null }

    Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
    Start-Process explorer.exe
    Start-Sleep -Seconds 2
    $report.explorerRestartAttempted = $true

    Remove-ItemProperty -LiteralPath $blockedPath -Name $clsid -ErrorAction SilentlyContinue
    $afterUnblock = Invoke-ScanFixture
    $report.stateAfterUnblock = if ($afterUnblock) { $afterUnblock.state } else { $null }
}
catch {
    $report.errors += $_.Exception.Message
}
finally {
    if (Test-Path -LiteralPath $blockedPath) {
        Remove-ItemProperty -LiteralPath $blockedPath -Name $clsid -ErrorAction SilentlyContinue
    }
    $package = Get-AppxPackage -Name 'WinButler.ContextMenuFixture' -ErrorAction SilentlyContinue
    if ($package) {
        Remove-AppxPackage -Package $package.PackageFullName -ErrorAction SilentlyContinue
    }
    $report.finishedAt = (Get-Date).ToString('o')
    $report | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath (Join-Path $resultRoot 'report.json') -Encoding utf8
}
