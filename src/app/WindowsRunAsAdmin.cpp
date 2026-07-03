#include "app/WindowsRunAsAdmin.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

#ifdef _WIN32
constexpr const wchar_t* kLayersKeyPath =
    L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers";
constexpr const wchar_t* kRunAsAdminFlag = L"RUNASADMIN";

std::wstring executableRegistryPath() {
    const QString path =
        QDir::toNativeSeparators(QFileInfo(QCoreApplication::applicationFilePath()).absoluteFilePath());
    return path.toStdWString();
}
#endif

} // namespace

bool WindowsRunAsAdmin::isRegistered() {
#ifdef _WIN32
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kLayersKeyPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    const std::wstring exePath = executableRegistryPath();
    DWORD type = 0;
    DWORD size = 0;
    const LONG query = RegQueryValueExW(key, exePath.c_str(), nullptr, &type, nullptr, &size);
    if (query != ERROR_SUCCESS || type != REG_SZ || size <= sizeof(wchar_t)) {
        RegCloseKey(key);
        return false;
    }

    std::wstring value(size / sizeof(wchar_t), L'\0');
    const LONG read = RegQueryValueExW(key,
                                       exePath.c_str(),
                                       nullptr,
                                       &type,
                                       reinterpret_cast<BYTE*>(value.data()),
                                       &size);
    RegCloseKey(key);
    if (read != ERROR_SUCCESS) {
        return false;
    }

    while (!value.empty() && value.back() == L'\0') {
        value.pop_back();
    }
    return value.find(kRunAsAdminFlag) != std::wstring::npos;
#else
    return false;
#endif
}

bool WindowsRunAsAdmin::setRegistered(bool enabled) {
#ifdef _WIN32
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        kLayersKeyPath,
                        0,
                        nullptr,
                        0,
                        KEY_SET_VALUE,
                        nullptr,
                        &key,
                        nullptr) != ERROR_SUCCESS) {
        return false;
    }

    const std::wstring exePath = executableRegistryPath();
    bool ok = false;
    if (enabled) {
        const DWORD byteSize =
            static_cast<DWORD>((wcslen(kRunAsAdminFlag) + 1) * sizeof(wchar_t));
        ok = RegSetValueExW(key,
                            exePath.c_str(),
                            0,
                            REG_SZ,
                            reinterpret_cast<const BYTE*>(kRunAsAdminFlag),
                            byteSize) == ERROR_SUCCESS;
    } else {
        const LONG removed = RegDeleteValueW(key, exePath.c_str());
        ok = removed == ERROR_SUCCESS || removed == ERROR_FILE_NOT_FOUND;
    }

    RegCloseKey(key);
    return ok;
#else
    (void)enabled;
    return false;
#endif
}

bool WindowsRunAsAdmin::isProcessElevated() {
#ifdef _WIN32
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return false;
    }

    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const BOOL ok = GetTokenInformation(
        token, TokenElevation, &elevation, sizeof(elevation), &size);
    CloseHandle(token);
    return ok && elevation.TokenIsElevated;
#else
    return false;
#endif
}

bool WindowsRunAsAdmin::relaunchElevated() {
#ifdef _WIN32
    using ShellExecuteWFn = HINSTANCE(WINAPI*)(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);
    HMODULE shell32 = LoadLibraryW(L"shell32.dll");
    if (!shell32) {
        return false;
    }
    const auto shellExecuteW =
        reinterpret_cast<ShellExecuteWFn>(GetProcAddress(shell32, "ShellExecuteW"));
    if (!shellExecuteW) {
        FreeLibrary(shell32);
        return false;
    }

    const std::wstring exePath = executableRegistryPath();
    const std::wstring workDir =
        QDir::toNativeSeparators(QCoreApplication::applicationDirPath()).toStdWString();
    const HINSTANCE result = shellExecuteW(nullptr,
                                           L"runas",
                                           exePath.c_str(),
                                           nullptr,
                                           workDir.c_str(),
                                           SW_SHOWNORMAL);
    FreeLibrary(shell32);
    return reinterpret_cast<intptr_t>(result) > 32;
#else
    return false;
#endif
}
