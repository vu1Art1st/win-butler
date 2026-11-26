#include "winbutler/shell/catalog.h"
#include "manifest.h"

#include <filesystem>
#include <iostream>
#include <set>
#include <string>

namespace {

int Fail(const std::string& message) {
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}

}  // namespace

int main() {
    using namespace winbutler::shell;

    if (SceneName(MenuScene::DirectoryBackground) != "DirectoryBackground") {
        return Fail("SceneName changed unexpectedly");
    }
    if (KindName(MenuItemKind::PackagedComCommand) != "PackagedComCommand") {
        return Fail("KindName changed unexpectedly");
    }
    if (StateName(EffectiveState::Disabled) != "Disabled") {
        return Fail("StateName changed unexpectedly");
    }
    if (ToUtf8(ToWide("win-butler")) != "win-butler") {
        return Fail("UTF-8 round trip failed");
    }

    const auto fixtureManifest =
        std::filesystem::path(WINBUTLER_SOURCE_DIR) / "fixtures" / "packaged-com" / "AppxManifest.xml";
    const auto manifest = winbutler::shell::internal::ParseContextMenuManifest(fixtureManifest.wstring());
    if (!manifest.parsed || !manifest.declaresContextMenus) {
        return Fail("fixture AppxManifest.xml did not declare file explorer context menus");
    }
    if (manifest.verbs.empty()) {
        return Fail("fixture AppxManifest.xml did not expose any context-menu verbs");
    }

    const ScanResult result = ScanReadOnly();
    if (result.elapsedMilliseconds > 15000) {
        return Fail("read-only scan exceeded the 15 second safety limit");
    }
    if (result.items.empty()) {
        return Fail("read-only scan returned no menu items");
    }

    std::set<std::string> kinds;
    std::set<std::string> ids;
    for (const MenuItem& item : result.items) {
        kinds.insert(KindName(item.kind));
        if (item.stableId.empty()) {
            return Fail("scan produced an empty stable id");
        }
        if (!ids.insert(item.stableId).second) {
            return Fail("scan produced a duplicate stable id: " + item.stableId);
        }
        if (item.registryPath.empty()) {
            return Fail("scan produced an item without a registry path");
        }
    }

    if (kinds.find("ShellVerb") == kinds.end()) {
        return Fail("scan did not find any traditional shell verbs");
    }

    std::cout << "PASS: items=" << result.items.size()
              << " packageCandidates=" << result.packageCandidates
              << " packagesWithContextMenus=" << result.packagesWithContextMenus
              << " elapsedMs=" << result.elapsedMilliseconds << '\n';
    return 0;
}
