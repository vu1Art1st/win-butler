#include "eui_neo.h"
#include "winbutler/shell/catalog.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace app {

namespace {

using winbutler::shell::EffectiveState;
using winbutler::shell::MenuScene;
using winbutler::shell::MenuItem;
using winbutler::shell::MenuItemKind;
using winbutler::shell::ScanResult;

struct UiRow {
    std::string id;
    std::string name;
    std::string kind;
    std::string scene;
    std::string state;
    std::string source;
};

struct Strings {
    const char* title;
    const char* subtitle;
    const char* scan;
    const char* scanning;
    const char* search;
    const char* allScenes;
    const char* file;
    const char* allFilesystem;
    const char* directory;
    const char* background;
    const char* drive;
    const char* folder;
    const char* desktopBackground;
    const char* name;
    const char* type;
    const char* scene;
    const char* state;
    const char* source;
    const char* details;
    const char* registryPath;
    const char* clsid;
    const char* package;
    const char* executable;
    const char* noSelection;
    const char* noResults;
    const char* scanFailed;
    const char* packageSummary;
    const char* language;
};

constexpr Strings kZh{
    "win-butler",
    "Windows 右键菜单只读目录",
    "刷新扫描",
    "正在扫描...",
    "搜索菜单项",
    "全部场景",
    "文件",
    "所有文件系统对象",
    "文件夹",
    "文件夹背景",
    "驱动器",
    "文件夹对象",
    "桌面背景",
    "名称",
    "类型",
    "场景",
    "状态",
    "来源",
    "详情",
    "注册表路径",
    "CLSID",
    "包",
    "可执行文件",
    "选择一项查看详情",
    "没有匹配项",
    "扫描失败",
    "候选包 / 含右键菜单的包",
    "English",
};

constexpr Strings kEn{
    "win-butler",
    "Read-only Windows context-menu catalog",
    "Rescan",
    "Scanning...",
    "Search menu items",
    "All scenes",
    "Files",
    "All filesystem objects",
    "Folders",
    "Folder background",
    "Drives",
    "Folder objects",
    "Desktop background",
    "Name",
    "Type",
    "Scene",
    "State",
    "Source",
    "Details",
    "Registry path",
    "CLSID",
    "Package",
    "Executable",
    "Select an item to inspect",
    "No matching items",
    "Scan failed",
    "Package candidates / with context menus",
    "中文",
};

const Strings& Text(bool english) {
    return english ? kEn : kZh;
}

struct AppState {
    std::vector<MenuItem> items;
    eui::Signal<std::string> search;
    eui::Signal<std::vector<std::string>> selectedIds;
    bool english = false;
    bool scanRequested = false;
    bool scanning = true;
    std::string error;
    std::uint64_t elapsedMilliseconds = 0;
    std::size_t packageCandidates = 0;
    std::size_t packagesWithContextMenus = 0;
    int sceneFilter = -1;
    std::uint64_t revision = 1;
};

std::string LowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool ContainsText(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }
    return LowerAscii(haystack).find(LowerAscii(needle)) != std::string::npos;
}

std::string SceneLabel(MenuScene scene, bool english) {
    const auto& text = Text(english);
    switch (scene) {
        case MenuScene::File:
            return text.file;
        case MenuScene::AllFilesystemObjects:
            return text.allFilesystem;
        case MenuScene::Directory:
            return text.directory;
        case MenuScene::DirectoryBackground:
            return text.background;
        case MenuScene::Drive:
            return text.drive;
        case MenuScene::Folder:
            return text.folder;
        case MenuScene::DesktopBackground:
            return text.desktopBackground;
    }
    return {};
}

std::string KindLabel(MenuItemKind kind, bool english) {
    switch (kind) {
        case MenuItemKind::ShellVerb:
            return english ? "Shell verb" : "传统动词";
        case MenuItemKind::ContextMenuHandler:
            return english ? "ShellEx handler" : "ShellEx 处理程序";
        case MenuItemKind::PackagedComCommand:
            return english ? "Packaged COM" : "Packaged COM";
    }
    return {};
}

std::string StateLabel(EffectiveState state, bool english) {
    switch (state) {
        case EffectiveState::Enabled:
            return english ? "Enabled" : "启用";
        case EffectiveState::Disabled:
            return english ? "Disabled" : "禁用";
        case EffectiveState::Unknown:
            return english ? "Unknown" : "未知";
    }
    return {};
}

const MenuItem* SelectedItem(const AppState& state) {
    const auto& selected = state.selectedIds.get();
    if (selected.empty()) {
        return nullptr;
    }
    const auto found = std::find_if(state.items.begin(), state.items.end(), [&](const MenuItem& item) {
        return item.stableId == selected.front();
    });
    return found == state.items.end() ? nullptr : &*found;
}

void ApplyScanResult(AppState* state, const ScanResult& result) {
    state->items = result.items;
    state->elapsedMilliseconds = result.elapsedMilliseconds;
    state->packageCandidates = result.packageCandidates;
    state->packagesWithContextMenus = result.packagesWithContextMenus;
    state->scanning = false;
    state->error.clear();
    state->selectedIds.set({});
    ++state->revision;
}

void StartScan(AppState* state, bool restart) {
    state->scanning = true;
    state->error.clear();
    auto work = [] { return winbutler::shell::ScanReadOnly(); };
    auto then = [state](const app::async::Result<ScanResult>& result) {
        if (result.ok) {
            ApplyScanResult(state, result.value);
        } else {
            state->scanning = false;
            state->error = result.error;
        }
    };
    if (restart) {
        app::async::restart("winbutler.scan", work, then);
    } else {
        app::async::runOnce("winbutler.scan", work, then);
    }
}

void BeginInitialScan(AppState* state) {
    if (state->scanRequested) {
        return;
    }
    state->scanRequested = true;
    StartScan(state, false);
}

std::shared_ptr<std::vector<UiRow>> BuildRows(const AppState& state) {
    auto rows = std::make_shared<std::vector<UiRow>>();
    rows->reserve(state.items.size());
    const std::string search = state.search.get();
    for (const MenuItem& item : state.items) {
        if (state.sceneFilter >= 0 &&
            static_cast<int>(item.scene) != state.sceneFilter) {
            continue;
        }
        if (!ContainsText(item.displayName, search) && !ContainsText(item.registryPath, search) &&
            !ContainsText(item.clsid, search)) {
            continue;
        }
        rows->push_back({
            item.stableId,
            item.displayName,
            KindLabel(item.kind, state.english),
            SceneLabel(item.scene, state.english),
            StateLabel(item.state, state.english),
            winbutler::shell::HiveName(item.hive),
        });
    }
    return rows;
}

void DrawHeader(eui::Ui& ui, const eui::Screen& screen, AppState& state) {
    const auto& text = Text(state.english);
    const auto theme = components::theme::light();
    components::label(ui, "header.title")
        .position(20, 12)
        .size(260, 28)
        .text(text.title)
        .fontSize(22)
        .color(theme.text)
        .build();
    components::label(ui, "header.subtitle")
        .position(20, 40)
        .size(520, 20)
        .text(text.subtitle)
        .fontSize(13)
        .color(components::theme::withAlpha(theme.text, 0.62f))
        .build();

    components::button(ui, "header.language")
        .position(std::max(20.0f, screen.width - 236.0f), 16)
        .size(92, 32)
        .text(text.language)
        .theme(theme, false)
        .onClick([&state] {
            state.english = !state.english;
            ++state.revision;
        })
        .build();
    components::button(ui, "header.scan")
        .position(std::max(20.0f, screen.width - 136.0f), 16)
        .size(116, 32)
        .text(state.scanning ? text.scanning : text.scan)
        .theme(theme, true)
        .onClick([&state] {
            if (!state.scanning) {
                StartScan(&state, true);
            }
        })
        .build();
}

void DrawSidebar(eui::Ui& ui, const eui::Screen& screen, AppState& state) {
    const auto& text = Text(state.english);
    const auto theme = components::theme::light();
    const float width = 184.0f;
    const float top = 84.0f;
    ui.rect("sidebar.surface")
        .position(16, top)
        .size(width, std::max(0.0f, screen.height - top - 16))
        .color(theme.surface)
        .build();

    const std::vector<std::pair<std::string, int>> entries{
        {text.allScenes, -1},
        {text.file, static_cast<int>(MenuScene::File)},
        {text.directory, static_cast<int>(MenuScene::Directory)},
        {text.background, static_cast<int>(MenuScene::DirectoryBackground)},
        {text.drive, static_cast<int>(MenuScene::Drive)},
        {text.folder, static_cast<int>(MenuScene::Folder)},
        {text.desktopBackground, static_cast<int>(MenuScene::DesktopBackground)},
    };

    float y = top + 12;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const bool selected = state.sceneFilter == entries[index].second;
        components::button(ui, "sidebar." + std::to_string(index))
            .position(24, y)
            .size(width - 16, 32)
            .text(entries[index].first)
            .theme(theme, selected)
            .onClick([&state, value = entries[index].second] {
                state.sceneFilter = value;
                ++state.revision;
            })
            .build();
        y += 40;
    }
}

void DrawDetails(eui::Ui& ui, const eui::Screen& screen, const AppState& state, const MenuItem* item) {
    const auto& text = Text(state.english);
    const auto theme = components::theme::light();
    const float left = 216.0f;
    const float width = std::max(0.0f, screen.width - left - 16.0f);
    const float top = std::max(120.0f, screen.height - 188.0f);
    ui.rect("details.surface")
        .position(left, top)
        .size(width, 172)
        .color(theme.surface)
        .build();
    components::label(ui, "details.title")
        .position(left + 12, top + 8)
        .size(width - 24, 24)
        .text(text.details)
        .fontSize(15)
        .color(theme.text)
        .build();

    if (item == nullptr) {
        components::label(ui, "details.empty")
            .position(left + 12, top + 40)
            .size(width - 24, 22)
            .text(text.noSelection)
            .fontSize(13)
            .color(components::theme::withAlpha(theme.text, 0.62f))
            .build();
        return;
    }

    const float labelX = left + 12;
    const float valueX = left + 140;
    const float valueWidth = std::max(0.0f, width - 152.0f);
    float y = top + 38;
    const auto row = [&](const std::string& id, const char* label, const std::string& value) {
        components::label(ui, "details." + id + ".label")
            .position(labelX, y)
            .size(120, 20)
            .text(label)
            .fontSize(12)
            .color(components::theme::withAlpha(theme.text, 0.62f))
            .build();
        components::label(ui, "details." + id + ".value")
            .position(valueX, y)
            .size(valueWidth, 20)
            .text(value.empty() ? "-" : value)
            .fontSize(12)
            .color(theme.text)
            .build();
        y += 23;
    };
    row("name", text.name, item->displayName);
    row("type", text.type, KindLabel(item->kind, state.english));
    row("state", text.state, StateLabel(item->state, state.english));
    row("registry", text.registryPath, item->registryPath);
    if (!item->clsid.empty()) {
        row("clsid", text.clsid, item->clsid);
    }
    if (!item->packageFullName.empty()) {
        row("package", text.package, item->packageFullName);
    }
    if (!item->executablePath.empty()) {
        row("executable", text.executable, item->executablePath);
    }
}

}  // namespace

const DslAppConfig& dslAppConfig() {
    static const auto config = DslAppConfig{}
        .title("win-butler")
        .pageId("winbutler")
        .appId("winbutler")
        .logicalWindowSize(1200, 720)
        .minWindowSize(960, 640)
        .showDebugStatsInTitle(false);
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    auto& state = ui.state<AppState>("winbutler.state");
    BeginInitialScan(&state);

    const auto theme = components::theme::light();
    const auto& text = Text(state.english);
    ui.rect("background")
        .size(screen.width, screen.height)
        .color(theme.background)
        .build();
    DrawHeader(ui, screen, state);
    DrawSidebar(ui, screen, state);

    const float left = 216.0f;
    const float width = std::max(0.0f, screen.width - left - 16.0f);
    components::input(ui, state.english ? "catalog.search.en" : "catalog.search.zh")
        .position(left, 84)
        .size(width, 32)
        .theme(theme)
        .placeholder(text.search)
        .value(state.search.get())
        .onChange([&state](std::string value) {
            state.search.set(std::move(value));
            ++state.revision;
        })
        .build();

    auto rows = BuildRows(state);
    components::dataTable(ui, "catalog.table")
        .position(left, 128)
        .size(width, std::max(160.0f, screen.height - 328.0f))
        .theme(theme)
        .data<UiRow>(
            rows,
            {
                {text.name, [](const UiRow& row) { return row.name; }},
                {text.type, [](const UiRow& row) { return row.kind; }},
                {text.scene, [](const UiRow& row) { return row.scene; }},
                {text.state, [](const UiRow& row) { return row.state; }},
                {text.source, [](const UiRow& row) { return row.source; }},
            },
            state.revision)
        .columnWidths({0.30f, 0.20f, 0.17f, 0.12f, 0.21f})
        .rowKey([rows](std::size_t index) { return (*rows)[index].id; })
        .selectionMode(components::DataTableSelectionMode::Single)
        .bindSelection(state.selectedIds)
        .build();

    if (state.scanning) {
        components::label(ui, "catalog.status")
            .position(left, std::max(128.0f, screen.height - 312.0f))
            .size(width, 24)
            .text(text.scanning)
            .fontSize(13)
            .color(components::theme::withAlpha(theme.text, 0.62f))
            .build();
    } else if (!state.error.empty()) {
        components::label(ui, "catalog.error")
            .position(left, std::max(128.0f, screen.height - 312.0f))
            .size(width, 24)
            .text(text.scanFailed + std::string(": ") + state.error)
            .fontSize(13)
            .color("#B42318")
            .build();
    } else {
        const std::string summary =
            std::to_string(state.items.size()) + " items | " + std::to_string(state.elapsedMilliseconds) +
            " ms | " + text.packageSummary + ": " + std::to_string(state.packageCandidates) + " / " +
            std::to_string(state.packagesWithContextMenus);
        components::label(ui, "catalog.summary")
            .position(left, std::max(128.0f, screen.height - 312.0f))
            .size(width, 24)
            .text(summary)
            .fontSize(12)
            .color(components::theme::withAlpha(theme.text, 0.62f))
            .build();
        if (rows->empty()) {
            components::label(ui, "catalog.empty")
                .position(left, 160)
                .size(width, 24)
                .text(text.noResults)
                .fontSize(13)
                .color(components::theme::withAlpha(theme.text, 0.62f))
                .build();
        }
    }

    DrawDetails(ui, screen, state, SelectedItem(state));
}

}  // namespace app
