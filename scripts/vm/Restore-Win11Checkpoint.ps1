[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [string]$VmName,
    [Parameter(Mandatory)]
    [string]$CheckpointName
)

$ErrorActionPreference = 'Stop'
$vm = Get-VM -Name $VmName
if ($PSCmdlet.ShouldProcess($VmName, "Restore checkpoint $CheckpointName")) {
    Stop-VM -VM $vm -Force -ErrorAction SilentlyContinue
    Restore-VMSnapshot -VMName $VmName -Name $CheckpointName -Confirm:$false
    Start-VM -VM $vm
    Write-Host "Restored and started $VmName from $CheckpointName"
}
