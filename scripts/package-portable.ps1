[CmdletBinding()]
param(
    [string]$Version = '0.1.0',
    [string]$Configuration = 'Release',
    [string]$Generator = 'Visual Studio 18 2026',
    [string]$Architecture = 'Win32',
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmake = if ($cmakeCommand) {
    $cmakeCommand.Source
} else {
    'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
}
$ctestCommand = Get-Command ctest -ErrorAction SilentlyContinue
$ctest = if ($ctestCommand) {
    $ctestCommand.Source
} else {
    Join-Path (Split-Path $cmake) 'ctest.exe'
}
$build = Join-Path $root 'build-release'
$artifacts = Join-Path $root 'artifacts'
$stageName = "win-butler-$Version-x86"
$stage = Join-Path $artifacts $stageName
$zip = Join-Path $artifacts "$stageName.zip"

& $cmake -S $root -B $build -G $Generator -A $Architecture -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $cmake --build $build --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $SkipTests) {
    & $ctest --test-dir $build -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

New-Item -ItemType Directory -Force -Path $artifacts | Out-Null
if (Test-Path -LiteralPath $stage) {
    $resolvedStage = (Resolve-Path -LiteralPath $stage).Path
    $resolvedArtifacts = (Resolve-Path -LiteralPath $artifacts).Path
    if (-not $resolvedStage.StartsWith($resolvedArtifacts + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove staging directory outside artifacts: $resolvedStage"
    }
    Remove-Item -LiteralPath $resolvedStage -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $stage, (Join-Path $stage 'licenses'), (Join-Path $stage 'docs') | Out-Null
Copy-Item -LiteralPath (Join-Path $build "$Configuration\win_butler.exe") $stage
Copy-Item -LiteralPath (Join-Path $build "$Configuration\win_butler_scan.exe") $stage
Copy-Item -LiteralPath (Join-Path $build "$Configuration\assets") $stage -Recurse
Copy-Item -LiteralPath (Join-Path $root 'README.md') $stage
Copy-Item -LiteralPath (Join-Path $root 'README.en.md') $stage
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') $stage
Copy-Item -LiteralPath (Join-Path $root 'THIRD_PARTY_NOTICES.md') $stage
Copy-Item -LiteralPath (Join-Path $root 'docs\DEVELOPMENT_PLAN.md') (Join-Path $stage 'docs')
Copy-Item -LiteralPath (Join-Path $root 'docs\M0_TECHNICAL_VALIDATION.md') (Join-Path $stage 'docs')
Copy-Item -LiteralPath (Join-Path $build '_deps\eui_neo-src\LICENSE') (Join-Path $stage 'licenses\EUI-NEO-LICENSE.txt')

if (Test-Path -LiteralPath $zip) {
    Remove-Item -LiteralPath $zip -Force
}
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal

$hash = Get-FileHash -LiteralPath $zip -Algorithm SHA256
"$($hash.Hash)  $([IO.Path]::GetFileName($zip))" |
    Set-Content -LiteralPath "$zip.sha256" -Encoding ascii

[pscustomobject]@{
    Version = $Version
    Architecture = 'x86'
    Zip = $zip
    SizeBytes = (Get-Item -LiteralPath $zip).Length
    SHA256 = $hash.Hash
} | Format-List
