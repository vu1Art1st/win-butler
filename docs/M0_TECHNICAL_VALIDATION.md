# M0 technical validation

Date: 2026-11-26

## Results

| Area | Result | Evidence |
| --- | --- | --- |
| EUI-NEO x86 build | Passed | EUI-NEO `v0.6.5` built with MSVC `19.50` and the Win32 toolchain |
| Desktop app launch | Passed | `win_butler.exe` launched and rendered the read-only catalog |
| 150% DPI rendering | Passed | DPI-aware window capture showed the full 1200x720 logical layout |
| Traditional shell scan | Passed | 72 shell verbs found on the development host |
| ShellEx scan | Passed | 59 handlers found, including resolved COM DLL paths |
| Packaged COM scan | Passed | 68 package candidates, 15 packages with context-menu declarations, 30 aggregated items |
| Scan performance | Passed | Approximately 60 to 140 ms during repeated runs |
| Classic-shell fixture | Passed | Installed, discovered, removed, and verified with zero residual items |
| ShellEx fixture | Passed | Built as a 32-bit `IContextMenu` DLL, installed, discovered, removed, and verified with zero residual items |
| Packaged COM fixture build | Passed | Built and signed a 64-bit MSIX containing `IExplorerCommand` and `windows.fileExplorerContextMenus` |
| Packaged COM install/block/unblock | Passed in Sandbox | Windows Sandbox build 26100 installed the signed fixture, scanned it as Enabled, changed to Disabled after Blocked, then returned to Enabled |
| Packaged COM visual menu check | Pending local manual check | Scanner state transitions were verified in Sandbox; the visible Explorer effect has not been checked |

## Technical findings

### EUI-NEO

- EUI-NEO `v0.6.5` builds as a 32-bit Windows application from source.
- `logicalWindowSize` is handled correctly at 150% DPI.
- The initial screenshot tooling was not DPI-aware and produced a cropped image. A DPI-aware capture showed the complete layout.
- The UI uses explicit application state and `app::async` for the background scan.

### Traditional shell

- Scanning `HKCU` and `HKLM` separately is required for accurate provenance.
- The 32-bit process must open the 64-bit registry view because Explorer runs as a 64-bit process on x64 Windows.
- `LegacyDisable` is visible and can be reported without loading the handler.

### ShellEx

- CLSID values may be stored in the handler key's default value or in the handler key name.
- Approved-list descriptions improve display names.
- The Blocked list is readable from both HKCU and HKLM.
- The fixture DLL is not loaded during scanning.

### Packaged COM

- `PackagedCom\Package` contains many COM classes unrelated to context menus.
- The manifest must be filtered for `windows.fileExplorerContextMenus`.
- Notepad's installed manifest demonstrates the expected structure:
  - `com:SurrogateServer` declares the CLSID and DLL path.
  - `desktop4:FileExplorerContextMenus` declares the item types and verb CLSID.
- The scanner associates package identity, manifest verb, CLSID, DLL path, and Blocked state.
- Public reference behavior and the fixture design use:
  - `HKCU\Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Blocked`
  - Value name: CLSID with braces
  - Value data: empty string
- Before enabling Packaged COM writes, verify the visible Explorer effect on the current development build; treat unverified Windows builds as read-only.
- Windows Sandbox build 26100 confirmed the package installs, the scanner finds it, and the Blocked-list state transitions are observable:
  - after install: `Enabled`
  - after Blocked value: `Disabled`
  - after removing Blocked value: `Enabled`
- The sandbox report is stored under `artifacts/sandbox-results/report.json` during local validation.

## Known gaps

- Packaged COM display names that are resource references are currently shown as verb IDs or resource strings.
- The current scanner uses the 64-bit registry view only. A later pass can add an explicit 32-bit-view comparison.
- No write operation is exposed in the application yet.
- Packaged COM blocking is not yet promoted from a validated candidate to a production capability.

## Decision

The read-only architecture is viable. Proceed to Packaged COM writes only after a reversible local manual check confirms that blocking removes the item from Explorer and restoring the prior state brings it back on the current development build. No VM matrix or cross-version test infrastructure is required. If behavior is not confirmed for a Windows build, keep Packaged COM entries read-only on that build.
