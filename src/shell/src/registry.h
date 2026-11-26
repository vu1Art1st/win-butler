#pragma once

#include <Windows.h>

#include <optional>
#include <string>
#include <vector>

namespace winbutler::shell::internal {

class RegistryKey {
public:
    RegistryKey() = default;
    RegistryKey(HKEY handle) : handle_(handle) {}
    ~RegistryKey();

    RegistryKey(const RegistryKey&) = delete;
    RegistryKey& operator=(const RegistryKey&) = delete;

    RegistryKey(RegistryKey&& other) noexcept;
    RegistryKey& operator=(RegistryKey&& other) noexcept;

    static RegistryKey Open(HKEY root, const std::wstring& path, REGSAM access);
    static RegistryKey OpenRead(HKEY root, const std::wstring& path);

    bool valid() const { return handle_ != nullptr; }
    HKEY get() const { return handle_; }

    std::vector<std::wstring> Subkeys() const;
    std::optional<std::wstring> StringValue(const wchar_t* name) const;
    std::optional<DWORD> DwordValue(const wchar_t* name) const;
    bool HasValue(const wchar_t* name) const;

private:
    HKEY handle_ = nullptr;
};

std::optional<std::wstring> ReadStringValue(HKEY root, const std::wstring& path, const wchar_t* name);
bool ValueExists(HKEY root, const std::wstring& path, const wchar_t* name);
bool KeyExists(HKEY root, const std::wstring& path);
std::wstring ExpandEnvironment(const std::wstring& value);
std::wstring NormalizeClsid(const std::wstring& value);
bool IsClsid(const std::wstring& value);
std::wstring JoinRegistryPath(const std::wstring& left, const std::wstring& right);

}  // namespace winbutler::shell::internal
