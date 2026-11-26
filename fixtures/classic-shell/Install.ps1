[CmdletBinding()]
param(
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'
$keyPath = 'HKCU:\Software\Classes\*\shell\WinButlerFixture'

if ($Uninstall) {
    if (Test-Path -LiteralPath $keyPath) {
        Remove-Item -LiteralPath $keyPath -Recurse -Force
    }
    Write-Host 'Removed classic shell verb fixture.'
    exit 0
}

New-Item -Path $keyPath -Force | Out-Null
New-Item -Path (Join-Path $keyPath 'command') -Force | Out-Null
Set-ItemProperty -LiteralPath $keyPath -Name '(Default)' -Value 'win-butler classic fixture'
Set-ItemProperty -LiteralPath (Join-Path $keyPath 'command') -Name '(Default)' `
    -Value 'notepad.exe "%1"'

Write-Host 'Installed classic shell verb fixture.'
Write-Host 'Expected scanner result: ShellVerb in the File scene under HKCU.'
