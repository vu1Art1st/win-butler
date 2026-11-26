[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)]
    [string[]]$VmName,
    [string]$CheckpointPrefix = 'winbutler-baseline'
)

$ErrorActionPreference = 'Stop'
foreach ($name in $VmName) {
    $vm = Get-VM -Name $name
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $checkpointName = "$CheckpointPrefix-$stamp"
    if ($PSCmdlet.ShouldProcess($vm.Name, "Create checkpoint $checkpointName")) {
        Checkpoint-VM -VM $vm -SnapshotName $checkpointName
        Write-Host "Created $($vm.Name) checkpoint: $checkpointName"
    }
}
