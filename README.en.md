# win-butler

[中文](README.md)

win-butler is a Windows system-management tool. Phase one focuses on File Explorer context-menu management.

## Status

M0 technical validation and the first read-only catalog demo are complete:

- A 32-bit Win32 scanner recognizes classic `shell` verbs, `shellex` ContextMenuHandlers, and Windows 11 Packaged COM.
- The EUI-NEO desktop app runs with Chinese/English switching, scene filtering, search, and details.
- Test fixtures exist for classic verbs, ShellEx DLLs, and Packaged COM MSIX packages.
- The read-only scan currently completes in roughly 60 to 140 ms on the development machine.
- This build is read-only and exposes no registry-write action.

## Phase One Scope

- Windows 11 modern/classic context-menu style switching
- Inventory for files, folders, folder background, and drives
- Management of classic `shell` verbs and `shellex` ContextMenuHandlers
- Windows 11 Packaged COM context-menu discovery and management
- Advanced editing of third-party menu commands
- Reversible registry changes, change previews, and local audit history

## Technical Direction

- Shell layer: native C++ using Win32, COM, Registry, and Windows package APIs
- UI: EUI-NEO `v0.6.5`
- Build: CMake 3.24+ with MSVC 2022 or newer
- Target: Windows 11 x86, 32-bit process
- Delivery: portable ZIP only, no installer
- License: MIT

The UI runs unelevated by default. Machine-wide writes will be routed through an allowlisted elevation broker.

## Build

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A Win32
cmake --build build --config Release --parallel
```

Run the desktop app:

```powershell
.\build\Release\win_butler.exe
```

Run the read-only scanner:

```powershell
.\build\Release\win_butler_scan.exe --summary
.\build\Release\win_butler_scan.exe
```

Run tests:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

## Fixtures

- `fixtures/classic-shell`: classic shell verb registration
- `fixtures/shellex`: buildable 32-bit `IContextMenu` DLL
- `fixtures/packaged-com`: buildable and signed 64-bit Packaged COM MSIX

Fixtures are for disposable Windows 11 test environments. The Packaged COM fixture is not installed by default.

## Documentation

- [Development plan](docs/DEVELOPMENT_PLAN.md)
- [M0 technical validation](docs/M0_TECHNICAL_VALIDATION.md)
- [VM snapshot matrix](docs/VM_MATRIX.md)
- [Reference analysis and license boundary](docs/REFERENCE_ANALYSIS.md)

## License

[MIT](LICENSE)
