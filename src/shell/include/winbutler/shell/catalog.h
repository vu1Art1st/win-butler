#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace winbutler::shell {

enum class Hive {
    CurrentUser,
    LocalMachine
};

enum class MenuScene {
    File,
    AllFilesystemObjects,
    Directory,
    DirectoryBackground,
    Drive,
    Folder,
    DesktopBackground
};

enum class MenuItemKind {
    ShellVerb,
    ContextMenuHandler,
    PackagedComCommand
};

enum class EffectiveState {
    Enabled,
    Disabled,
    Unknown
};

struct Diagnostic {
    std::string code;
    std::string message;
};

struct MenuItem {
    std::string stableId;
    std::string displayName;
    MenuScene scene = MenuScene::File;
    MenuItemKind kind = MenuItemKind::ShellVerb;
    Hive hive = Hive::CurrentUser;
    std::string registryPath;
    std::string clsid;
    std::string packageFullName;
    std::string packageFamilyName;
    std::string executablePath;
    std::string command;
    std::string icon;
    std::string publisher;
    std::string version;
    EffectiveState state = EffectiveState::Unknown;
    bool requiresElevation = false;
    std::vector<std::string> itemTypes;
    std::vector<Diagnostic> diagnostics;
};

struct ScanResult {
    std::vector<MenuItem> items;
    std::vector<Diagnostic> diagnostics;
    std::uint64_t elapsedMilliseconds = 0;
    std::size_t packageCandidates = 0;
    std::size_t packagesWithContextMenus = 0;
};

ScanResult ScanReadOnly();

std::string ToUtf8(const std::wstring& value);
std::wstring ToWide(const std::string& value);

std::string HiveName(Hive hive);
std::string SceneName(MenuScene scene);
std::string KindName(MenuItemKind kind);
std::string StateName(EffectiveState state);

}  // namespace winbutler::shell
