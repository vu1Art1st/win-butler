[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$package = Get-AppxPackage -Name 'WinButler.ContextMenuFixture' -ErrorAction SilentlyContinue
if ($package) {
    Remove-AppxPackage -Package $package.PackageFullName
    Write-Host "Removed package $($package.PackageFullName)"
} else {
    Write-Host 'Packaged COM fixture is not installed.'
}
