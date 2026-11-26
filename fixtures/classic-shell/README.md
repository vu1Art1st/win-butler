# Classic shell verb fixture

This fixture writes a per-user context-menu verb:

`HKCU\Software\Classes\*\shell\WinButlerFixture`

Install:

```powershell
.\Install.ps1
```

Remove:

```powershell
.\Install.ps1 -Uninstall
```

The fixture never writes to `HKLM`. Restart Explorer after installing or removing it if the menu does not refresh immediately.
