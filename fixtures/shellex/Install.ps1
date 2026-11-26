[CmdletBinding()]
param(
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'
$clsid = '{6D7B8E3A-5A1B-4E71-8B1D-6C7F0A2E9D11}'
$clsidKey = "HKCU:\Software\Classes\CLSID\$clsid\InprocServer32"
$handlerKey = 'HKCU:\Software\Classes\*\shellex\ContextMenuHandlers\WinButlerFixture'

if ($Uninstall) {
    if (Test-Path -LiteralPath $handlerKey) {
        Remove-Item -LiteralPath $handlerKey -Recurse -Force
    }
    $clsidRoot = "HKCU:\Software\Classes\CLSID\$clsid"
    if (Test-Path -LiteralPath $clsidRoot) {
        Remove-Item -LiteralPath $clsidRoot -Recurse -Force
    }
    Write-Host 'Removed ShellEx fixture registration.'
    exit 0
}

$dll = Join-Path $PSScriptRoot '..\..\build-fixtures\fixtures\shellex\Release\win_butler_fixture_shellex.dll'
$dll = (Resolve-Path -LiteralPath $dll).Path

New-Item -Path $clsidKey -Force | Out-Null
Set-ItemProperty -LiteralPath $clsidKey -Name '(Default)' -Value $dll
Set-ItemProperty -LiteralPath $clsidKey -Name 'ThreadingModel' -Value 'Apartment'

New-Item -Path $handlerKey -Force | Out-Null
Set-ItemProperty -LiteralPath $handlerKey -Name '(Default)' -Value $clsid

Write-Host "Registered ShellEx fixture: $dll"
Write-Host 'Expected scanner result: ContextMenuHandler in the File scene under HKCU.'
