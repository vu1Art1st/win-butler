#include "manifest.h"

#include "registry.h"
#include "winbutler/shell/catalog.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string_view>
#include <utility>

namespace winbutler::shell::internal {

namespace {

std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

std::wstring ReadTextFile(const std::wstring& path) {
    std::ifstream stream(std::filesystem::path(path), std::ios::binary);
    if (!stream) {
        return {};
    }
    stream.seekg(0, std::ios::end);
    const auto size = stream.tellg();
    if (size <= 0) {
        return {};
    }
    stream.seekg(0, std::ios::beg);

    std::string bytes(static_cast<std::size_t>(size), '\0');
    stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        return {};
    }

    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF &&
        static_cast<unsigned char>(bytes[1]) == 0xFE) {
        return std::wstring(
            reinterpret_cast<const wchar_t*>(bytes.data() + 2),
            (bytes.size() - 2) / sizeof(wchar_t));
    }
    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFE &&
        static_cast<unsigned char>(bytes[1]) == 0xFF) {
        std::wstring result;
        result.reserve((bytes.size() - 2) / 2);
        for (std::size_t index = 2; index + 1 < bytes.size(); index += 2) {
            const auto high = static_cast<unsigned char>(bytes[index]);
            const auto low = static_cast<unsigned char>(bytes[index + 1]);
            result.push_back(static_cast<wchar_t>((high << 8) | low));
        }
        return result;
    }

    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.erase(0, 3);
    }
    return winbutler::shell::ToWide(bytes);
}

std::size_t FindInsensitive(const std::wstring& haystack, const std::wstring& needle, std::size_t start = 0) {
    if (needle.empty() || start >= haystack.size()) {
        return std::wstring::npos;
    }
    const std::wstring lowerHaystack = ToLower(haystack);
    const std::wstring lowerNeedle = ToLower(needle);
    return lowerHaystack.find(lowerNeedle, start);
}

std::pair<std::wstring, std::size_t> FindContainingTag(const std::wstring& text, std::size_t position) {
    const std::size_t open = text.rfind(L'<', position);
    if (open == std::wstring::npos) {
        return {};
    }
    const std::size_t close = text.find(L'>', open);
    if (close == std::wstring::npos) {
        return {};
    }

    std::size_t nameStart = open + 1;
    while (nameStart < close && std::iswspace(text[nameStart])) {
        ++nameStart;
    }
    std::size_t nameEnd = nameStart;
    while (nameEnd < close && !std::iswspace(text[nameEnd]) && text[nameEnd] != L'/' && text[nameEnd] != L'>') {
        ++nameEnd;
    }
    return {text.substr(nameStart, nameEnd - nameStart), close + 1};
}

std::vector<std::wstring> ExtractAttributes(
    const std::wstring& block, const std::wstring& attributeName) {
    std::vector<std::wstring> result;
    const std::wregex pattern(
        attributeName + LR"regex(\s*=\s*"([^"]+)")regex",
        std::regex_constants::icase | std::regex_constants::optimize);
    for (std::wsregex_iterator it(block.begin(), block.end(), pattern), end; it != end; ++it) {
        result.push_back((*it)[1].str());
    }
    return result;
}

std::vector<ContextMenuVerb> ExtractVerbs(const std::wstring& block) {
    std::vector<ContextMenuVerb> result;
    const std::wregex itemTypePattern(
        LR"(<(?:[A-Za-z_][\w.-]*:)?ItemType\b([^>]*)>([\s\S]*?)</(?:[A-Za-z_][\w.-]*:)?ItemType\s*>)",
        std::regex_constants::icase | std::regex_constants::optimize);
    const std::wregex verbPattern(
        LR"(<(?:[A-Za-z_][\w.-]*:)?Verb\b[^>]*>)",
        std::regex_constants::icase | std::regex_constants::optimize);

    for (std::wsregex_iterator itemIt(block.begin(), block.end(), itemTypePattern), itemEnd;
         itemIt != itemEnd;
         ++itemIt) {
        const std::wstring attributes = (*itemIt)[1].str();
        const auto types = ExtractAttributes(attributes, L"Type");
        const std::wstring itemType = types.empty() ? std::wstring() : types.front();
        const std::wstring body = (*itemIt)[2].str();

        for (std::wsregex_iterator verbIt(body.begin(), body.end(), verbPattern), verbEnd;
             verbIt != verbEnd;
             ++verbIt) {
            const std::wstring verbTag = verbIt->str();
            const auto ids = ExtractAttributes(verbTag, L"Id");
            const auto clsids = ExtractAttributes(verbTag, L"Clsid");
            if (!ids.empty() && !clsids.empty()) {
                result.push_back({ids.front(), clsids.front(), itemType});
            }
        }
    }
    return result;
}

}  // namespace

ContextMenuManifest ParseContextMenuManifest(const std::wstring& manifestPath) {
    ContextMenuManifest result;
    const std::wstring text = ReadTextFile(manifestPath);
    if (text.empty()) {
        result.error = L"Unable to read AppxManifest.xml";
        return result;
    }

    result.parsed = true;
    constexpr std::wstring_view marker = L"windows.fileExplorerContextMenus";
    const std::size_t markerPosition = FindInsensitive(text, std::wstring(marker));
    if (markerPosition == std::wstring::npos) {
        return result;
    }

    result.declaresContextMenus = true;
    const auto [tagName, contentStart] = FindContainingTag(text, markerPosition);
    if (tagName.empty()) {
        result.error = L"Unable to locate context-menu extension tag";
        return result;
    }

    const std::wstring closingTag = L"</" + tagName + L">";
    const std::size_t closing = FindInsensitive(text, closingTag, contentStart);
    const std::size_t contentEnd = closing == std::wstring::npos ? text.size() : closing;
    const std::wstring block = text.substr(contentStart, contentEnd - contentStart);

    result.clsidValues = ExtractAttributes(block, L"Clsid");
    result.itemTypes = ExtractAttributes(block, L"Type");
    result.verbs = ExtractVerbs(block);
    return result;
}

}  // namespace winbutler::shell::internal
