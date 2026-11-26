#include <Windows.h>
#include <ShlObj.h>
#include <Shlwapi.h>

#include <atomic>
#include <cwchar>
#include <iterator>
#include <new>
#include <string>

namespace {

constexpr wchar_t kClsidText[] = L"{6D7B8E3A-5A1B-4E71-8B1D-6C7F0A2E9D11}";
constexpr wchar_t kClsidKey[] = L"Software\\Classes\\CLSID\\{6D7B8E3A-5A1B-4E71-8B1D-6C7F0A2E9D11}";
constexpr wchar_t kHandlerKey[] =
    L"Software\\Classes\\*\\shellex\\ContextMenuHandlers\\WinButlerFixture";
constexpr UINT kCommandId = 1;

const CLSID kClsid = {0x6D7B8E3A, 0x5A1B, 0x4E71, {0x8B, 0x1D, 0x6C, 0x7F, 0x0A, 0x2E, 0x9D, 0x11}};

HMODULE g_module = nullptr;
std::atomic<long> g_objectCount{0};

class FixtureContextMenu final : public IShellExtInit, public IContextMenu {
public:
    FixtureContextMenu() { ++g_objectCount; }
    ~FixtureContextMenu() { --g_objectCount; }

    IFACEMETHODIMP QueryInterface(REFIID riid, void** result) override {
        if (result == nullptr) {
            return E_POINTER;
        }
        *result = nullptr;
        if (riid == IID_IUnknown || riid == IID_IShellExtInit) {
            *result = static_cast<IShellExtInit*>(this);
        } else if (riid == IID_IContextMenu) {
            *result = static_cast<IContextMenu*>(this);
        } else {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override { return ++referenceCount_; }

    IFACEMETHODIMP_(ULONG) Release() override {
        const ULONG remaining = --referenceCount_;
        if (remaining == 0) {
            delete this;
        }
        return remaining;
    }

    IFACEMETHODIMP Initialize(PCIDLIST_ABSOLUTE, IDataObject*, HKEY) override { return S_OK; }

    IFACEMETHODIMP QueryContextMenu(
        HMENU menu, UINT indexMenu, UINT idCmdFirst, UINT, UINT flags) override {
        if ((flags & CMF_DEFAULTONLY) != 0) {
            return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, 0);
        }
        InsertMenuW(
            menu, indexMenu, MF_BYPOSITION | MF_STRING, idCmdFirst + kCommandId,
            L"win-butler ShellEx fixture");
        return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, kCommandId + 1);
    }

    IFACEMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO* command) override {
        if (command == nullptr) {
            return E_INVALIDARG;
        }
        const UINT_PTR verb = reinterpret_cast<UINT_PTR>(command->lpVerb);
        if (HIWORD(verb) != 0) {
            return E_INVALIDARG;
        }
        if (LOWORD(verb) == kCommandId) {
            MessageBoxW(
                command->hwnd, L"win-butler ShellEx fixture invoked.", L"win-butler",
                MB_OK | MB_ICONINFORMATION);
            return S_OK;
        }
        return E_FAIL;
    }

    IFACEMETHODIMP GetCommandString(
        UINT_PTR, UINT, UINT*, char*, UINT) override {
        return E_NOTIMPL;
    }

private:
    std::atomic<ULONG> referenceCount_{1};
};

class FixtureClassFactory final : public IClassFactory {
public:
    IFACEMETHODIMP QueryInterface(REFIID riid, void** result) override {
        if (result == nullptr) {
            return E_POINTER;
        }
        *result = nullptr;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *result = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override { return ++referenceCount_; }

    IFACEMETHODIMP_(ULONG) Release() override {
        const ULONG remaining = --referenceCount_;
        if (remaining == 0) {
            delete this;
        }
        return remaining;
    }

    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** result) override {
        if (result == nullptr) {
            return E_POINTER;
        }
        *result = nullptr;
        if (outer != nullptr) {
            return CLASS_E_NOAGGREGATION;
        }
        auto* instance = new (std::nothrow) FixtureContextMenu();
        if (instance == nullptr) {
            return E_OUTOFMEMORY;
        }
        const HRESULT status = instance->QueryInterface(riid, result);
        instance->Release();
        return status;
    }

    IFACEMETHODIMP LockServer(BOOL lock) override {
        lock ? ++g_objectCount : --g_objectCount;
        return S_OK;
    }

private:
    std::atomic<ULONG> referenceCount_{1};
};

HRESULT WriteDefaultString(HKEY root, const wchar_t* path, const wchar_t* value) {
    HKEY key = nullptr;
    LSTATUS status = RegCreateKeyExW(root, path, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr);
    if (status != ERROR_SUCCESS) {
        return HRESULT_FROM_WIN32(status);
    }
    status = RegSetValueExW(
        key, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(value),
        static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return HRESULT_FROM_WIN32(status);
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow() {
    return g_objectCount.load() == 0 ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(
    REFCLSID clsid, REFIID riid, void** result) {
    if (result == nullptr) {
        return E_POINTER;
    }
    *result = nullptr;
    if (clsid != kClsid) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    auto* factory = new (std::nothrow) FixtureClassFactory();
    if (factory == nullptr) {
        return E_OUTOFMEMORY;
    }
    const HRESULT status = factory->QueryInterface(riid, result);
    factory->Release();
    return status;
}

STDAPI DllRegisterServer() {
    wchar_t dllPath[MAX_PATH]{};
    if (GetModuleFileNameW(g_module, dllPath, static_cast<DWORD>(std::size(dllPath))) == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    const std::wstring inprocPath = std::wstring(kClsidKey) + L"\\InprocServer32";
    HRESULT status = WriteDefaultString(HKEY_CURRENT_USER, inprocPath.c_str(), dllPath);
    if (FAILED(status)) {
        return status;
    }
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, inprocPath.c_str(), 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
        RegSetValueExW(
            key, L"ThreadingModel", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(L"Apartment"), sizeof(L"Apartment"));
        RegCloseKey(key);
    }
    status = WriteDefaultString(HKEY_CURRENT_USER, kHandlerKey, kClsidText);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return status;
}

STDAPI DllUnregisterServer() {
    RegDeleteTreeW(HKEY_CURRENT_USER, kHandlerKey);
    RegDeleteTreeW(HKEY_CURRENT_USER, kClsidKey);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}
