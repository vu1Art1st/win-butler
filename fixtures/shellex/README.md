# ShellEx fixture

Build with the root project:

```powershell
cmake -S . -B build-fixtures -G "Visual Studio 18 2026" -A Win32 -DWINBUTLER_BUILD_UI=OFF -DWINBUTLER_BUILD_FIXTURES=ON
cmake --build build-fixtures --config Release
```

Install or remove the per-user registration:

```powershell
.\Install.ps1
.\Install.ps1 -Uninstall
```

The fixture is a 32-bit in-process `IContextMenu` handler. It writes only to `HKCU`.
