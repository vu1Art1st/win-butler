#include "winbutler/shell/catalog.h"

#include <Windows.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string JsonEscape(const std::string& value) {
    std::ostringstream output;
    for (const unsigned char ch : value) {
        switch (ch) {
            case '"':
                output << "\\\"";
                break;
            case '\\':
                output << "\\\\";
                break;
            case '\b':
                output << "\\b";
                break;
            case '\f':
                output << "\\f";
                break;
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            default:
                if (ch < 0x20) {
                    output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                           << static_cast<int>(ch) << std::dec;
                } else {
                    output << static_cast<char>(ch);
                }
                break;
        }
    }
    return output.str();
}

void WriteDiagnostics(std::ostream& output, const std::vector<winbutler::shell::Diagnostic>& diagnostics) {
    output << '[';
    for (std::size_t index = 0; index < diagnostics.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << "{\"code\":\"" << JsonEscape(diagnostics[index].code) << "\",\"message\":\""
               << JsonEscape(diagnostics[index].message) << "\"}";
    }
    output << ']';
}

void WriteJson(const winbutler::shell::ScanResult& result) {
    std::cout << "{\n";
    std::cout << "  \"elapsedMilliseconds\":" << result.elapsedMilliseconds << ",\n";
    std::cout << "  \"packageCandidates\":" << result.packageCandidates << ",\n";
    std::cout << "  \"packagesWithContextMenus\":" << result.packagesWithContextMenus << ",\n";
    std::cout << "  \"items\":[";
    for (std::size_t index = 0; index < result.items.size(); ++index) {
        const auto& item = result.items[index];
        if (index != 0) {
            std::cout << ',';
        }
        std::cout << "\n    {"
                  << "\"stableId\":\"" << JsonEscape(item.stableId) << "\","
                  << "\"displayName\":\"" << JsonEscape(item.displayName) << "\","
                  << "\"scene\":\"" << winbutler::shell::SceneName(item.scene) << "\","
                  << "\"kind\":\"" << winbutler::shell::KindName(item.kind) << "\","
                  << "\"hive\":\"" << winbutler::shell::HiveName(item.hive) << "\","
                  << "\"state\":\"" << winbutler::shell::StateName(item.state) << "\","
                  << "\"registryPath\":\"" << JsonEscape(item.registryPath) << "\","
                  << "\"clsid\":\"" << JsonEscape(item.clsid) << "\","
                  << "\"packageFullName\":\"" << JsonEscape(item.packageFullName) << "\","
                  << "\"packageFamilyName\":\"" << JsonEscape(item.packageFamilyName) << "\","
                  << "\"executablePath\":\"" << JsonEscape(item.executablePath) << "\","
                  << "\"command\":\"" << JsonEscape(item.command) << "\","
                  << "\"requiresElevation\":" << (item.requiresElevation ? "true" : "false") << ','
                  << "\"itemTypes\":[";
        for (std::size_t typeIndex = 0; typeIndex < item.itemTypes.size(); ++typeIndex) {
            if (typeIndex != 0) {
                std::cout << ',';
            }
            std::cout << '"' << JsonEscape(item.itemTypes[typeIndex]) << '"';
        }
        std::cout << "],"
                  << "\"diagnostics\":";
        WriteDiagnostics(std::cout, item.diagnostics);
        std::cout << '}';
    }
    std::cout << "\n  ],\n  \"diagnostics\":";
    WriteDiagnostics(std::cout, result.diagnostics);
    std::cout << "\n}\n";
}

}  // namespace

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    const bool summary = argc > 1 && std::string(argv[1]) == "--summary";
    const auto result = winbutler::shell::ScanReadOnly();
    if (summary) {
        std::size_t shellVerbs = 0;
        std::size_t shellEx = 0;
        std::size_t packagedCom = 0;
        for (const auto& item : result.items) {
            switch (item.kind) {
                case winbutler::shell::MenuItemKind::ShellVerb:
                    ++shellVerbs;
                    break;
                case winbutler::shell::MenuItemKind::ContextMenuHandler:
                    ++shellEx;
                    break;
                case winbutler::shell::MenuItemKind::PackagedComCommand:
                    ++packagedCom;
                    break;
            }
        }
        std::cout << "items=" << result.items.size() << " shellVerbs=" << shellVerbs
                  << " shellEx=" << shellEx << " packagedCom=" << packagedCom
                  << " packageCandidates=" << result.packageCandidates
                  << " packagesWithContextMenus=" << result.packagesWithContextMenus
                  << " elapsedMs=" << result.elapsedMilliseconds << '\n';
        return 0;
    }

    WriteJson(result);
    return 0;
}
