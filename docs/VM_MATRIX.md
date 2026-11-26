# Windows 11 VM snapshot matrix

## Purpose

The VM matrix protects context-menu changes from being validated on only one Windows build. Every destructive or Explorer-restart test runs from a clean checkpoint and restores that checkpoint afterward.

## Target matrix

| Target | Build prefix | Required |
| --- | --- | --- |
| Windows 11 22H2 | 22621 | Yes |
| Windows 11 23H2 | 22631 | Yes |
| Windows 11 24H2 | 26100 | Yes |
| Windows 11 current stable | 26200 | Yes, update when the stable build changes |

The machine-readable definition is [vm/matrix.json](../vm/matrix.json).

## Snapshot convention

```text
winbutler-clean-22h2
winbutler-clean-23h2
winbutler-clean-24h2
winbutler-clean-current
```

Create snapshots with:

```powershell
.\scripts\vm\New-Win11Checkpoints.ps1 -VmName 'Win11-22H2','Win11-24H2'
```

Restore after a test:

```powershell
.\scripts\vm\Restore-Win11Checkpoint.ps1 -VmName 'Win11-22H2' -CheckpointName 'winbutler-clean-22h2'
```

## Test sequence per snapshot

1. Confirm OS build and architecture.
2. Run `win_butler_tests.exe`.
3. Run `win_butler_scan.exe --summary` and store JSON output.
4. Install the classic shell fixture and verify one new `ShellVerb`.
5. Install the ShellEx fixture and verify one new `ContextMenuHandler`.
6. Remove both fixtures and verify zero residual items.
7. Build and install the signed Packaged COM MSIX.
8. Verify the fixture appears in the Packaged COM catalog.
9. Add its CLSID to `HKCU\Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Blocked`.
10. Restart Explorer and verify the menu item disappears.
11. Remove the blocked value, restart Explorer, and verify the menu item returns.
12. Uninstall the package and verify the catalog no longer reports it.
13. Switch the Windows 11 context-menu style from modern to classic and back.
14. Restore the clean checkpoint.

## Automated execution

Inside a prepared VM:

```powershell
.\scripts\vm\Invoke-WinButlerMatrix.ps1 -BuildLabel win11-24h2 -RunFixtures
```

The script writes a report under `artifacts\vm\<build-label>`.

Packaged COM installation and Explorer restart remain explicit opt-in steps because they change package state and the interactive shell.

## Current execution status

The current development host is a single Windows build 26300 environment without an accessible Hyper-V VM inventory. The scanner, UI, classic-shell fixture, ShellEx fixture, and signed Packaged COM package build have been validated locally. A one-off Windows Sandbox run on build 26100 also confirmed package installation and the `Enabled -> Disabled -> Enabled` scanner state transition around the Blocked list. Cross-version VM execution and visual Explorer menu checks remain pending.
