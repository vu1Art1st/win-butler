#include "registry.h"

#include <objbase.h>

#include <algorithm>
#include <cwctype>
#include <utility>
#include <vector>

namespace winbutler::shell::internal {

namespace {

constexpr REGSAM kRegistryView = KEY_WOW64_64KEY;

std::wstring Trim(const std::wstring& value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](wchar_t ch) {
        return std::iswspace(ch) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](wchar_t ch) {
        return std::iswspace(ch) != 0;
    }).base();
    if (first >= last) {
        return {};
    }
    return std::wstring(first, last);
}

}  // namespace

RegistryKey::~RegistryKey() {
    if (handle_ != nullptr) {
        RegCloseKey(handle_);
    }
}

RegistryKey::RegistryKey(RegistryKey&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

RegistryKey& RegistryKey::operator=(RegistryKey&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    if (handle_ != nullptr) {
        RegCloseKey(handle_);
    }
    handle_ = std::exchange(other.handle_, nullptr);
    return *this;
}

RegistryKey RegistryKey::Open(HKEY root, const std::wstring& path, REGSAM access) {
    HKEY handle = nullptr;
    const LSTATUS status = RegOpenKeyExW(root, path.c_str(), 0, access | kRegistryView, &handle);
    if (status != ERROR_SUCCESS) {
        return {};
    }
    return RegistryKey(handle);
}

RegistryKey RegistryKey::OpenRead(HKEY root, const std::wstring& path) {
    return Open(root, path, KEY_READ);
}

std::vector<std::wstring> RegistryKey::Subkeys() const {
    std::vector<std::wstring> result;
    if (handle_ == nullptr) {
        return result;
    }

    DWORD subkeyCount = 0;
    DWORD maxSubkeyLength = 0;
    LSTATUS status = RegQueryInfoKeyW(
        handle_, nullptr, nullptr, nullptr, &subkeyCount, &maxSubkeyLength, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (status != ERROR_SUCCESS) {
        return result;
    }

    result.reserve(subkeyCount);
    std::vector<wchar_t> buffer(static_cast<std::size_t>(maxSubkeyLength) + 1);
    for (DWORD index = 0; index < subkeyCount; ++index) {
        DWORD length = static_cast<DWORD>(buffer.size());
        status = RegEnumKeyExW(handle_, index, buffer.data(), &length, nullptr, nullptr, nullptr, nullptr);
        if (status == ERROR_MORE_DATA) {
            buffer.resize(length + 1);
            --index;
            continue;
        }
        if (status == ERROR_SUCCESS) {
            result.emplace_back(buffer.data(), length);
        }
    }
    return result;
}

std::optional<std::wstring> RegistryKey::StringValue(const wchar_t* name) const {
    if (handle_ == nullptr) {
        return std::nullopt;
    }

    DWORD type = 0;
    DWORD size = 0;
    LSTATUS status = RegQueryValueExW(handle_, name, nullptr, &type, nullptr, &size);
    if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || size == 0) {
        return std::nullopt;
    }

    std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1, L'\0');
    status = RegQueryValueExW(
        handle_, name, nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &size);
    if (status != ERROR_SUCCESS) {
        return std::nullopt;
    }

    std::size_t length = size / sizeof(wchar_t);
    while (length > 0 && buffer[length - 1] == L'\0') {
        --length;
    }
    return std::wstring(buffer.data(), length);
}

std::optional<DWORD> RegistryKey::DwordValue(const wchar_t* name) const {
    if (handle_ == nullptr) {
        return std::nullopt;
    }

    DWORD type = 0;
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS status = RegQueryValueExW(
        handle_, name, nullptr, &type, reinterpret_cast<LPBYTE>(&value), &size);
    if (status != ERROR_SUCCESS || type != REG_DWORD) {
        return std::nullopt;
    }
    return value;
}

bool RegistryKey::HasValue(const wchar_t* name) const {
    if (handle_ == nullptr) {
        return false;
    }
    return RegQueryValueExW(handle_, name, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
}

std::optional<std::wstring> ReadStringValue(HKEY root, const std::wstring& path, const wchar_t* name) {
    const RegistryKey key = RegistryKey::OpenRead(root, path);
    if (!key.valid()) {
        return std::nullopt;
    }
    return key.StringValue(name);
}

bool ValueExists(HKEY root, const std::wstring& path, const wchar_t* name) {
    const RegistryKey key = RegistryKey::OpenRead(root, path);
    return key.valid() && key.HasValue(name);
}

bool KeyExists(HKEY root, const std::wstring& path) {
    return RegistryKey::OpenRead(root, path).valid();
}

std::wstring ExpandEnvironment(const std::wstring& value) {
    if (value.empty()) {
        return value;
    }

    DWORD required = ExpandEnvironmentStringsW(value.c_str(), nullptr, 0);
    if (required == 0) {
        return value;
    }
    std::vector<wchar_t> buffer(required);
    const DWORD written = ExpandEnvironmentStringsW(value.c_str(), buffer.data(), required);
    if (written == 0 || written > required) {
        return value;
    }
    return std::wstring(buffer.data(), written - 1);
}

std::wstring NormalizeClsid(const std::wstring& value) {
    std::wstring trimmed = Trim(value);
    if (trimmed.empty()) {
        return {};
    }

    if (trimmed.front() != L'{') {
        trimmed.insert(trimmed.begin(), L'{');
    }
    if (trimmed.back() != L'}') {
        trimmed.push_back(L'}');
    }

    CLSID clsid{};
    if (FAILED(CLSIDFromString(trimmed.c_str(), &clsid))) {
        return {};
    }

    wchar_t buffer[64]{};
    if (StringFromGUID2(clsid, buffer, static_cast<int>(std::size(buffer))) == 0) {
        return {};
    }
    return buffer;
}

bool IsClsid(const std::wstring& value) {
    return !NormalizeClsid(value).empty();
}

std::wstring JoinRegistryPath(const std::wstring& left, const std::wstring& right) {
    if (left.empty()) {
        return right;
    }
    if (right.empty()) {
        return left;
    }
    if (left.back() == L'\\') {
        return left + right;
    }
    return left + L"\\" + right;
}

}  // namespace winbutler::shell::internal
