[CmdletBinding()]
param(
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$artifacts = Join-Path $root 'artifacts'
$resultRoot = Join-Path $artifacts 'sandbox-results'
$reportPath = Join-Path $resultRoot 'report.json'
$wsbPath = Join-Path $artifacts 'packaged-com-validation.wsb'
$sandboxExe = 'C:\Windows\System32\WindowsSandbox.exe'

if (-not (Test-Path -LiteralPath $sandboxExe)) {
    throw 'Windows Sandbox is not available on this machine.'
}
if (-not (Test-Path -LiteralPath (Join-Path $artifacts 'win-butler-contextmenu-fixture.msix'))) {
    throw 'Build the Packaged COM fixture before running the sandbox validation.'
}

New-Item -ItemType Directory -Force -Path $resultRoot | Out-Null
if (Test-Path -LiteralPath $reportPath) {
    Remove-Item -LiteralPath $reportPath -Force
}

$rootXml = [Security.SecurityElement]::Escape($root)
$artifactsXml = [Security.SecurityElement]::Escape($artifacts)
$wsb = @"
<Configuration>
  <MappedFolders>
    <MappedFolder>
      <HostFolder>$rootXml</HostFolder>
      <SandboxFolder>C:\winbutler-src</SandboxFolder>
      <ReadOnly>true</ReadOnly>
    </MappedFolder>
    <MappedFolder>
      <HostFolder>$artifactsXml</HostFolder>
      <SandboxFolder>C:\winbutler-artifacts</SandboxFolder>
      <ReadOnly>false</ReadOnly>
    </MappedFolder>
  </MappedFolders>
  <Networking>Disable</Networking>
  <MemoryInMB>4096</MemoryInMB>
  <LogonCommand>
    <Command>powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\winbutler-src\scripts\sandbox\Run-PackagedComValidation.ps1</Command>
  </LogonCommand>
</Configuration>
"@
$wsb | Set-Content -LiteralPath $wsbPath -Encoding utf8

$process = Start-Process -FilePath $sandboxExe -ArgumentList $wsbPath -PassThru
$deadline = (Get-Date).AddSeconds($TimeoutSeconds)
while ((Get-Date) -lt $deadline) {
    if (Test-Path -LiteralPath $reportPath) {
        Start-Sleep -Milliseconds 500
        $report = Get-Content -Raw -LiteralPath $reportPath | ConvertFrom-Json
        $report | ConvertTo-Json -Depth 8
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        if ($report.errors.Count -gt 0) {
            exit 1
        }
        exit 0
    }
    if ($process.HasExited) {
        throw "Windows Sandbox exited before writing a report. Exit code: $($process.ExitCode)"
    }
    Start-Sleep -Seconds 2
}

Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
throw "Windows Sandbox validation timed out after $TimeoutSeconds seconds."
