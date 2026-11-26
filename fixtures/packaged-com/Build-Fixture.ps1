[CmdletBinding()]
param(
    [string]$Configuration = 'Release',
    [switch]$Install
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmake = if ($cmakeCommand) {
    $cmakeCommand.Source
} else {
    'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
$build = Join-Path $root 'build-packaged-com-fixture'
$artifacts = Join-Path $root 'artifacts'
$layout = Join-Path $build 'layout'
$msix = Join-Path $artifacts 'win-butler-contextmenu-fixture.msix'

& $cmake -S $PSScriptRoot -B $build -G 'Visual Studio 18 2026' -A x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cmake --build $build --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

New-Item -ItemType Directory -Force -Path $artifacts, $layout, (Join-Path $layout 'Assets') | Out-Null
Copy-Item -Force -LiteralPath (Join-Path $build "$Configuration\win_butler_fixture_host.exe") $layout
Copy-Item -Force -LiteralPath (Join-Path $build "$Configuration\win_butler_fixture_explorer_command.dll") $layout
Copy-Item -Force -LiteralPath (Join-Path $PSScriptRoot 'AppxManifest.xml') $layout

Add-Type -AssemblyName System.Drawing
foreach ($size in 44, 150) {
    $bitmap = New-Object System.Drawing.Bitmap($size, $size)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.Clear([System.Drawing.Color]::FromArgb(30, 90, 200))
    $graphics.Dispose()
    $name = if ($size -eq 44) { 'Square44x44Logo.png' } else { 'Square150x150Logo.png' }
    $bitmap.Save((Join-Path $layout "Assets\$name"), [System.Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
}

$makeAppx = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin' -Recurse -Filter MakeAppx.exe |
    Where-Object FullName -Match '\\x64\\MakeAppx.exe$' | Sort-Object FullName -Descending | Select-Object -First 1
$signtool = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin' -Recurse -Filter signtool.exe |
    Where-Object FullName -Match '\\x64\\signtool.exe$' | Sort-Object FullName -Descending | Select-Object -First 1
if (-not $makeAppx -or -not $signtool) {
    throw 'Windows SDK MakeAppx.exe or signtool.exe was not found.'
}

& $makeAppx.FullName pack /d $layout /p $msix /o
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$passwordText = 'winbutler-test'
$password = ConvertTo-SecureString -String $passwordText -Force -AsPlainText
$cert = Get-ChildItem Cert:\CurrentUser\My |
    Where-Object Subject -eq 'CN=WinButler Test' |
    Sort-Object NotAfter -Descending | Select-Object -First 1
if (-not $cert) {
    $cert = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject 'CN=WinButler Test' `
        -CertStoreLocation 'Cert:\CurrentUser\My' `
        -KeyExportPolicy Exportable `
        -KeyUsage DigitalSignature `
        -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3')
}
$pfx = Join-Path $artifacts 'win-butler-test-signing.pfx'
$cer = Join-Path $artifacts 'win-butler-test-signing.cer'
Export-PfxCertificate -Cert $cert -FilePath $pfx -Password $password | Out-Null
Export-Certificate -Cert $cert -FilePath $cer | Out-Null

& $signtool.FullName sign /fd SHA256 /f $pfx /p $passwordText $msix
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Built signed fixture: $msix"

if ($Install) {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'Installing the fixture requires an elevated PowerShell session.'
    }
    Import-Certificate -FilePath $cer `
        -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople' -ErrorAction SilentlyContinue | Out-Null
    Add-AppxPackage -Path $msix
    Write-Host 'Installed the packaged COM fixture.'
}
