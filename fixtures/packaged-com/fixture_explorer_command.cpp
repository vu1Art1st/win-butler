#include <Windows.h>
#include <ShObjIdl_core.h>
#include <Shlwapi.h>

#include <atomic>
#include <iterator>
#include <new>

namespace {

constexpr wchar_t kClsidText[] = L"{4F5F6A7B-8C9D-4E0F-8123-456789ABCDEF}";
const CLSID kClsid = {0x4F5F6A7B, 0x8C9D, 0x4E0F, {0x81, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF}};

HMODULE g_module = nullptr;
std::atomic<long> g_objectCount{0};

class FixtureExplorerCommand final : public IExplorerCommand {
public:
    FixtureExplorerCommand() { ++g_objectCount; }
    ~FixtureExplorerCommand() { --g_objectCount; }

    IFACEMETHODIMP QueryInterface(REFIID riid, void** result) override {
        if (result == nullptr) {
            return E_POINTER;
        }
        *result = nullptr;
        if (riid == IID_IUnknown || riid == IID_IExplorerCommand) {
            *result = static_cast<IExplorerCommand*>(this);
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

    IFACEMETHODIMP GetTitle(IShellItemArray*, LPWSTR* name) override {
        if (name == nullptr) {
            return E_POINTER;
        }
        return SHStrDupW(L"win-butler Packaged COM fixture", name);
    }

    IFACEMETHODIMP GetIcon(IShellItemArray*, LPWSTR* icon) override {
        if (icon == nullptr) {
            return E_POINTER;
        }
        *icon = nullptr;
        return E_NOTIMPL;
    }

    IFACEMETHODIMP GetToolTip(IShellItemArray*, LPWSTR* toolTip) override {
        if (toolTip == nullptr) {
            return E_POINTER;
        }
        return SHStrDupW(L"Invokes the win-butler test command.", toolTip);
    }

    IFACEMETHODIMP GetCanonicalName(GUID* commandName) override {
        if (commandName == nullptr) {
            return E_POINTER;
        }
        *commandName = kClsid;
        return S_OK;
    }

    IFACEMETHODIMP GetState(IShellItemArray*, BOOL, EXPCMDSTATE* state) override {
        if (state == nullptr) {
            return E_POINTER;
        }
        *state = ECS_ENABLED;
        return S_OK;
    }

    IFACEMETHODIMP Invoke(IShellItemArray*, IBindCtx*) override {
        MessageBoxW(
            nullptr, L"win-butler Packaged COM fixture invoked.", L"win-butler",
            MB_OK | MB_ICONINFORMATION);
        return S_OK;
    }

    IFACEMETHODIMP GetFlags(EXPCMDFLAGS* flags) override {
        if (flags == nullptr) {
            return E_POINTER;
        }
        *flags = ECF_DEFAULT;
        return S_OK;
    }

    IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** commands) override {
        if (commands == nullptr) {
            return E_POINTER;
        }
        *commands = nullptr;
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
        auto* command = new (std::nothrow) FixtureExplorerCommand();
        if (command == nullptr) {
            return E_OUTOFMEMORY;
        }
        const HRESULT status = command->QueryInterface(riid, result);
        command->Release();
        return status;
    }

    IFACEMETHODIMP LockServer(BOOL lock) override {
        lock ? ++g_objectCount : --g_objectCount;
        return S_OK;
    }

private:
    std::atomic<ULONG> referenceCount_{1};
};

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
