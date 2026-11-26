#include "winbutler/shell/catalog.h"

#include "manifest.h"
#include "registry.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cwctype>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>

namespace winbutler::shell {

namespace {

using internal::JoinRegistryPath;
using internal::KeyExists;
using internal::NormalizeClsid;
using internal::ParseContextMenuManifest;
using internal::ReadStringValue;
using internal::RegistryKey;
using internal::ValueExists;

constexpr wchar_t kClasses[] = L"Software\\Classes";
constexpr wchar_t kBlocked[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Blocked";
constexpr wchar_t kApproved[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";
constexpr wchar_t kPackagedCom[] = L"Software\\Classes\\PackagedCom\\Package";
constexpr wchar_t kPackageRepository[] =
    L"Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\PackageRepository\\Packages";

struct SceneRoot {
    MenuScene scene;
    const wchar_t* relativePath;
};

constexpr std::array<SceneRoot, 7> kSceneRoots{{
    {MenuScene::File, L"*"},
    {MenuScene::AllFilesystemObjects, L"AllFilesystemObjects"},
    {MenuScene::Directory, L"Directory"},
    {MenuScene::DirectoryBackground, L"Directory\\Background"},
    {MenuScene::Drive, L"Drive"},
    {MenuScene::Folder, L"Folder"},
    {MenuScene::DesktopBackground, L"DesktopBackground"},
}};

constexpr std::array<Hive, 2> kHives{{Hive::CurrentUser, Hive::LocalMachine}};

HKEY RootHandle(Hive hive) {
    return hive == Hive::CurrentUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
}

std::wstring Join(const std::wstring& left, const std::wstring& right) {
    return JoinRegistryPath(left, right);
}

void AddDiagnostic(ScanResult& result, std::string code, std::string message) {
    result.diagnostics.push_back({std::move(code), std::move(message)});
}

void AddItemDiagnostic(MenuItem& item, std::string code, std::string message) {
    item.diagnostics.push_back({std::move(code), std::move(message)});
}

std::wstring DefaultValue(const RegistryKey& key) {
    return key.StringValue(nullptr).value_or(L"");
}

std::wstring PreferredDisplayName(
    const RegistryKey& key, const std::wstring& fallback, const wchar_t* localizedName = L"MUIVerb") {
    const std::wstring localized = key.StringValue(localizedName).value_or(L"");
    if (!localized.empty()) {
        return localized;
    }
    const std::wstring defaultValue = DefaultValue(key);
    if (!defaultValue.empty()) {
        return defaultValue;
    }
    return fallback;
}

bool IsBlocked(const std::wstring& clsid) {
    if (clsid.empty()) {
        return false;
    }
    const std::wstring withoutBraces =
        (clsid.size() >= 2 && clsid.front() == L'{' && clsid.back() == L'}')
            ? clsid.substr(1, clsid.size() - 2)
            : clsid;
    for (const Hive hive : kHives) {
        if (ValueExists(RootHandle(hive), kBlocked, clsid.c_str()) ||
            ValueExists(RootHandle(hive), kBlocked, withoutBraces.c_str())) {
            return true;
        }
    }
    return false;
}

std::wstring ApprovedName(const std::wstring& clsid) {
    if (clsid.empty()) {
        return {};
    }
    for (const Hive hive : kHives) {
        const auto value = ReadStringValue(RootHandle(hive), kApproved, clsid.c_str());
        if (value.has_value() && !value->empty()) {
            return *value;
        }
    }
    return {};
}

std::wstring ResolveComServer(const std::wstring& clsid) {
    if (clsid.empty()) {
        return {};
    }
    const std::wstring clsidRoot = Join(kClasses, L"CLSID\\" + clsid);
    const std::array<const wchar_t*, 2> serverKeys{{L"InprocServer32", L"LocalServer32"}};
    for (const Hive hive : kHives) {
        for (const wchar_t* serverKey : serverKeys) {
            const std::wstring path = Join(clsidRoot, serverKey);
            const auto value = ReadStringValue(RootHandle(hive), path, nullptr);
            if (value.has_value() && !value->empty()) {
                return internal::ExpandEnvironment(*value);
            }
        }
    }
    return {};
}

std::wstring PackageFamilyName(const std::wstring& packageFullName) {
    const std::size_t first = packageFullName.find(L'_');
    const std::size_t last = packageFullName.rfind(L'_');
    if (first == std::wstring::npos || last == std::wstring::npos || first == last) {
        return packageFullName;
    }
    return packageFullName.substr(0, first) + L"_" + packageFullName.substr(last + 1);
}

std::wstring PackageInstallLocation(const std::wstring& packageFullName) {
    const std::wstring repositoryPath = Join(kPackageRepository, packageFullName);
    auto location = ReadStringValue(HKEY_CLASSES_ROOT, repositoryPath, L"Path");
    if (location.has_value() && !location->empty()) {
        return *location;
    }
    for (const Hive hive : kHives) {
        location = ReadStringValue(RootHandle(hive), repositoryPath, L"Path");
        if (location.has_value() && !location->empty()) {
            return *location;
        }
    }
    return {};
}

MenuScene SceneFromManifestType(const std::wstring& type) {
    std::wstring lower = type;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    if (lower == L"directory") {
        return MenuScene::Directory;
    }
    if (lower == L"drive") {
        return MenuScene::Drive;
    }
    if (lower == L"folder") {
        return MenuScene::Folder;
    }
    if (lower.find(L"background") != std::wstring::npos) {
        return MenuScene::DirectoryBackground;
    }
    return MenuScene::File;
}

std::string StableId(std::string_view prefix, std::string_view hive, std::string_view scene, std::string_view value) {
    std::ostringstream stream;
    stream << prefix << '|' << hive << '|' << scene << '|' << value;
    return stream.str();
}

void ScanShellVerbs(ScanResult& result) {
    for (const Hive hive : kHives) {
        for (const SceneRoot& scene : kSceneRoots) {
            const std::wstring shellPath = Join(kClasses, Join(scene.relativePath, L"shell"));
            const RegistryKey shellKey = RegistryKey::OpenRead(RootHandle(hive), shellPath);
            if (!shellKey.valid()) {
                continue;
            }

            for (const std::wstring& verbName : shellKey.Subkeys()) {
                const std::wstring verbPath = Join(shellPath, verbName);
                const RegistryKey verbKey = RegistryKey::OpenRead(RootHandle(hive), verbPath);
                if (!verbKey.valid()) {
                    continue;
                }

                MenuItem item;
                item.displayName = ToUtf8(PreferredDisplayName(verbKey, verbName));
                item.scene = scene.scene;
                item.kind = MenuItemKind::ShellVerb;
                item.hive = hive;
                item.registryPath = HiveName(hive) + "\\" + ToUtf8(verbPath);
                item.command = ToUtf8(DefaultValue(RegistryKey::OpenRead(
                    RootHandle(hive), Join(verbPath, L"Command"))));
                item.icon = ToUtf8(verbKey.StringValue(L"Icon").value_or(L""));
                item.requiresElevation = hive == Hive::LocalMachine;
                item.state = (verbKey.HasValue(L"LegacyDisable") || verbKey.HasValue(L"ProgrammaticAccessOnly"))
                                 ? EffectiveState::Disabled
                                 : EffectiveState::Enabled;
                item.stableId = StableId("verb", HiveName(hive), SceneName(scene.scene), item.registryPath);
                result.items.push_back(std::move(item));
            }
        }
    }
}

void ScanShellEx(ScanResult& result) {
    for (const Hive hive : kHives) {
        for (const SceneRoot& scene : kSceneRoots) {
            const std::wstring handlerPath =
                Join(kClasses, Join(scene.relativePath, L"shellex\\ContextMenuHandlers"));
            const RegistryKey handlers = RegistryKey::OpenRead(RootHandle(hive), handlerPath);
            if (!handlers.valid()) {
                continue;
            }

            for (const std::wstring& handlerName : handlers.Subkeys()) {
                const std::wstring itemPath = Join(handlerPath, handlerName);
                const RegistryKey handlerKey = RegistryKey::OpenRead(RootHandle(hive), itemPath);
                if (!handlerKey.valid()) {
                    continue;
                }

                const std::wstring rawClsid =
                    DefaultValue(handlerKey).empty() ? handlerName : DefaultValue(handlerKey);
                const std::wstring clsid = NormalizeClsid(rawClsid);
                const std::wstring approved = ApprovedName(clsid);

                MenuItem item;
                item.displayName = ToUtf8(approved.empty() ? handlerName : approved);
                item.scene = scene.scene;
                item.kind = MenuItemKind::ContextMenuHandler;
                item.hive = hive;
                item.registryPath = HiveName(hive) + "\\" + ToUtf8(itemPath);
                item.clsid = ToUtf8(clsid);
                item.executablePath = ToUtf8(ResolveComServer(clsid));
                item.requiresElevation = hive == Hive::LocalMachine;
                item.state = IsBlocked(clsid) ? EffectiveState::Disabled : EffectiveState::Enabled;
                item.stableId = StableId(
                    "shellex", HiveName(hive), SceneName(scene.scene),
                    clsid.empty() ? item.registryPath : item.clsid);
                if (clsid.empty()) {
                    AddItemDiagnostic(item, "invalid-clsid", "ContextMenuHandlers value is not a CLSID.");
                    item.state = EffectiveState::Unknown;
                } else if (item.executablePath.empty()) {
                    AddItemDiagnostic(item, "missing-com-server", "CLSID has no readable InprocServer32 or LocalServer32.");
                }
                result.items.push_back(std::move(item));
            }
        }
    }
}

void ScanPackagedCom(ScanResult& result) {
    std::set<std::wstring> packageNames;
    std::set<std::string> emittedStableIds;
    for (const Hive hive : kHives) {
        const RegistryKey packages = RegistryKey::OpenRead(RootHandle(hive), kPackagedCom);
        if (!packages.valid()) {
            continue;
        }
        for (const std::wstring& packageName : packages.Subkeys()) {
            packageNames.insert(packageName);
        }
    }

    for (const std::wstring& packageFullName : packageNames) {
        ++result.packageCandidates;
        const std::wstring installLocation = PackageInstallLocation(packageFullName);
        if (installLocation.empty()) {
            AddDiagnostic(result, "package-location-missing", ToUtf8(packageFullName));
            continue;
        }

        const std::wstring manifestPath =
            Join(installLocation, L"AppxManifest.xml");
        const auto manifest = ParseContextMenuManifest(manifestPath);
        if (!manifest.parsed) {
            AddDiagnostic(result, "manifest-unreadable", ToUtf8(manifestPath));
            continue;
        }
        if (!manifest.declaresContextMenus) {
            continue;
        }
        ++result.packagesWithContextMenus;

        std::set<std::wstring> manifestClsids;
        for (const std::wstring& rawClsid : manifest.clsidValues) {
            const std::wstring normalized = NormalizeClsid(rawClsid);
            if (!normalized.empty()) {
                manifestClsids.insert(normalized);
            }
        }

        const std::wstring classRoot = Join(kPackagedCom, packageFullName + L"\\Class");
        const RegistryKey classes = RegistryKey::OpenRead(HKEY_LOCAL_MACHINE, classRoot);
        if (!classes.valid()) {
            const RegistryKey userClasses = RegistryKey::OpenRead(HKEY_CURRENT_USER, classRoot);
            if (!userClasses.valid()) {
                continue;
            }
        }

        for (const Hive hive : kHives) {
            const RegistryKey classKeyRoot = RegistryKey::OpenRead(RootHandle(hive), classRoot);
            if (!classKeyRoot.valid()) {
                continue;
            }
            for (const std::wstring& className : classKeyRoot.Subkeys()) {
                const std::wstring clsid = NormalizeClsid(className);
                if (clsid.empty() || manifestClsids.find(clsid) == manifestClsids.end()) {
                    continue;
                }

                const std::wstring classPath = Join(classRoot, className);
                const RegistryKey classKey = RegistryKey::OpenRead(RootHandle(hive), classPath);
                const std::wstring dllPath =
                    classKey.StringValue(L"DllPath").value_or(
                        classKey.StringValue(L"InprocServerDllPath_x64").value_or(L""));
                const DWORD serverId = classKey.DwordValue(L"ServerId").value_or(0);
                const std::wstring serverPath = Join(kPackagedCom, packageFullName + L"\\Server\\" + std::to_wstring(serverId));
                const RegistryKey serverKey = RegistryKey::OpenRead(RootHandle(hive), serverPath);
                const std::wstring serverDisplay =
                    serverKey.StringValue(L"DisplayName").value_or(
                        serverKey.StringValue(L"ApplicationDisplayName").value_or(L""));

                std::vector<internal::ContextMenuVerb> matchingVerbs;
                for (const auto& verb : manifest.verbs) {
                    if (NormalizeClsid(verb.clsid) == clsid) {
                        matchingVerbs.push_back(verb);
                    }
                }
                if (matchingVerbs.empty()) {
                    matchingVerbs.push_back({L"", clsid, L"*"});
                }

                std::map<std::pair<int, std::wstring>, std::vector<std::wstring>> groupedVerbs;
                for (const auto& verb : matchingVerbs) {
                    groupedVerbs[{
                        static_cast<int>(SceneFromManifestType(verb.itemType)),
                        verb.id
                    }].push_back(verb.itemType);
                }

                for (const auto& [groupKey, itemTypes] : groupedVerbs) {
                    const MenuScene scene = static_cast<MenuScene>(groupKey.first);
                    const std::wstring& verbId = groupKey.second;
                    MenuItem item;
                    item.displayName = ToUtf8(!verbId.empty() ? verbId : serverDisplay);
                    if (item.displayName.empty()) {
                        item.displayName = ToUtf8(className);
                    }
                    item.scene = scene;
                    item.kind = MenuItemKind::PackagedComCommand;
                    item.hive = hive;
                    item.registryPath = HiveName(hive) + "\\" + ToUtf8(classPath);
                    item.clsid = ToUtf8(clsid);
                    item.packageFullName = ToUtf8(packageFullName);
                    item.packageFamilyName = ToUtf8(PackageFamilyName(packageFullName));
                    item.command = ToUtf8(verbId);
                    std::set<std::wstring> uniqueItemTypes(itemTypes.begin(), itemTypes.end());
                    for (const std::wstring& itemType : uniqueItemTypes) {
                        item.itemTypes.push_back(ToUtf8(itemType));
                    }
                    if (!dllPath.empty()) {
                        const std::wstring executable = Join(installLocation, dllPath);
                        item.executablePath = ToUtf8(executable);
                        if (!std::filesystem::exists(std::filesystem::path(executable))) {
                            AddItemDiagnostic(item, "package-file-missing", "Packaged COM DLL path does not exist.");
                        }
                    } else {
                        AddItemDiagnostic(item, "package-dll-missing", "PackagedCom class has no DllPath.");
                    }
                    item.state = IsBlocked(clsid) ? EffectiveState::Disabled : EffectiveState::Enabled;
                    item.stableId = StableId(
                        "packaged", HiveName(hive), SceneName(item.scene),
                        item.packageFamilyName + "|" + item.packageFullName + "|" + item.clsid + "|" +
                            item.command);
                    if (!emittedStableIds.insert(item.stableId).second) {
                        continue;
                    }
                    result.items.push_back(std::move(item));
                }
            }
        }
    }
}

}  // namespace

std::string ToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int required = WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), required, nullptr, nullptr);
    return result;
}

std::wstring ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int required =
        MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), required);
    return result;
}

std::string HiveName(Hive hive) {
    return hive == Hive::CurrentUser ? "HKCU" : "HKLM";
}

std::string SceneName(MenuScene scene) {
    switch (scene) {
        case MenuScene::File:
            return "File";
        case MenuScene::AllFilesystemObjects:
            return "AllFilesystemObjects";
        case MenuScene::Directory:
            return "Directory";
        case MenuScene::DirectoryBackground:
            return "DirectoryBackground";
        case MenuScene::Drive:
            return "Drive";
        case MenuScene::Folder:
            return "Folder";
        case MenuScene::DesktopBackground:
            return "DesktopBackground";
    }
    return "Unknown";
}

std::string KindName(MenuItemKind kind) {
    switch (kind) {
        case MenuItemKind::ShellVerb:
            return "ShellVerb";
        case MenuItemKind::ContextMenuHandler:
            return "ContextMenuHandler";
        case MenuItemKind::PackagedComCommand:
            return "PackagedComCommand";
    }
    return "Unknown";
}

std::string StateName(EffectiveState state) {
    switch (state) {
        case EffectiveState::Enabled:
            return "Enabled";
        case EffectiveState::Disabled:
            return "Disabled";
        case EffectiveState::Unknown:
            return "Unknown";
    }
    return "Unknown";
}

ScanResult ScanReadOnly() {
    const auto start = std::chrono::steady_clock::now();
    ScanResult result;
    ScanShellVerbs(result);
    ScanShellEx(result);
    ScanPackagedCom(result);
    const auto end = std::chrono::steady_clock::now();
    result.elapsedMilliseconds =
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    std::sort(result.items.begin(), result.items.end(), [](const MenuItem& left, const MenuItem& right) {
        if (left.scene != right.scene) {
            return left.scene < right.scene;
        }
        if (left.kind != right.kind) {
            return left.kind < right.kind;
        }
        return left.displayName < right.displayName;
    });
    return result;
}

}  // namespace winbutler::shell
