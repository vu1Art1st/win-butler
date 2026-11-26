# Packaged COM fixture

This fixture builds a 64-bit MSIX package containing an `IExplorerCommand` implementation and a `windows.fileExplorerContextMenus` manifest declaration.

Build and sign:

```powershell
.\Build-Fixture.ps1
```

The output is:

```text
artifacts\win-butler-contextmenu-fixture.msix
```

Installing an MSIX changes the current user's package state and requires a trusted test certificate plus administrator rights. It is intentionally opt-in:

```powershell
.\Build-Fixture.ps1 -Install
```

The fixture is only for a disposable Windows 11 VM. Do not install it on a production workstation.
