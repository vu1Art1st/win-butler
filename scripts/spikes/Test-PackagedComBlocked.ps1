[CmdletBinding()]
param(
    [string]$Clsid = '{4F5F6A7B-8C9D-4E0F-8123-456789ABCDEF}',
    [ValidateSet('Get', 'Add', 'Remove')]
    [string]$Action = 'Get',
    [switch]$Apply,
    [switch]$RestartExplorer
)

$ErrorActionPreference = 'Stop'
$blockedPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Blocked'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$backupDir = Join-Path $root 'artifacts\spikes'

function Get-BlockedValue {
    if (-not (Test-Path -LiteralPath $blockedPath)) {
        return $null
    }
    $property = Get-ItemProperty -LiteralPath $blockedPath -Name $Clsid -ErrorAction SilentlyContinue
    if ($null -eq $property) {
        return $null
    }
    return $property.$Clsid
}

$current = Get-BlockedValue
$state = [ordered]@{
    timestamp = (Get-Date).ToString('o')
    clsid = $Clsid
    blockedPath = $blockedPath
    currentlyBlocked = $null -ne $current
    currentValue = $current
    action = $Action
    applied = $false
}

if ($Action -eq 'Get' -or -not $Apply) {
    $state | ConvertTo-Json -Depth 5
    if ($Action -ne 'Get' -and -not $Apply) {
        Write-Host 'No change applied. Re-run with -Apply to modify the current user Blocked list.'
    }
    exit 0
}

New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
$state | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath (Join-Path $backupDir "blocked-$($Clsid.Trim('{','}'))-before.json") -Encoding utf8

if ($Action -eq 'Add') {
    New-Item -Path $blockedPath -Force | Out-Null
    New-ItemProperty -Path $blockedPath -Name $Clsid -PropertyType String -Value '' -Force | Out-Null
    $state.applied = $true
} elseif ($Action -eq 'Remove') {
    if (Test-Path -LiteralPath $blockedPath) {
        Remove-ItemProperty -LiteralPath $blockedPath -Name $Clsid -ErrorAction SilentlyContinue
    }
    $state.applied = $true
}

if ($RestartExplorer) {
    Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
    Start-Process explorer.exe
}

$state.afterBlocked = $null -ne (Get-BlockedValue)
$state | ConvertTo-Json -Depth 5
