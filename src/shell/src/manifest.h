#pragma once

#include <string>
#include <vector>

namespace winbutler::shell::internal {

struct ContextMenuVerb {
    std::wstring id;
    std::wstring clsid;
    std::wstring itemType;
};

struct ContextMenuManifest {
    bool parsed = false;
    bool declaresContextMenus = false;
    std::vector<std::wstring> clsidValues;
    std::vector<std::wstring> itemTypes;
    std::vector<ContextMenuVerb> verbs;
    std::wstring error;
};

ContextMenuManifest ParseContextMenuManifest(const std::wstring& manifestPath);

}  // namespace winbutler::shell::internal
