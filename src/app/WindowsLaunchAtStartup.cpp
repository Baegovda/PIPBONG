#include "app/WindowsLaunchAtStartup.h"

#include <QCoreApplication>
#include <QString>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

#ifdef _WIN32
constexpr const wchar_t* kRunKeyPath =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* kRunValueName = L"PIPBONG";

QString quotedExecutablePath() {
    QString path = QCoreApplication::applicationFilePath();
    path.replace(QLatin1Char('/'), QLatin1Char('\\'));
    if (path.contains(QLatin1Char(' '))) {
        path = QStringLiteral("\"%1\"").arg(path);
    }
    return path;
}
#endif

} // namespace

bool WindowsLaunchAtStartup::isRegistered() {
#ifdef _WIN32
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD size = 0;
    const LONG query =
        RegQueryValueExW(key, kRunValueName, nullptr, &type, nullptr, &size);
    RegCloseKey(key);
    return query == ERROR_SUCCESS && type == REG_SZ && size > sizeof(wchar_t);
#else
    return false;
#endif
}

bool WindowsLaunchAtStartup::setRegistered(bool enabled) {
#ifdef _WIN32
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        kRunKeyPath,
                        0,
                        nullptr,
                        0,
                        KEY_SET_VALUE,
                        nullptr,
                        &key,
                        nullptr) != ERROR_SUCCESS) {
        return false;
    }

    bool ok = false;
    if (enabled) {
        const QString command = quotedExecutablePath();
        const std::wstring wide = command.toStdWString();
        const DWORD byteSize = static_cast<DWORD>((wide.size() + 1) * sizeof(wchar_t));
        ok = RegSetValueExW(key,
                            kRunValueName,
                            0,
                            REG_SZ,
                            reinterpret_cast<const BYTE*>(wide.c_str()),
                            byteSize) == ERROR_SUCCESS;
    } else {
        const LONG removed = RegDeleteValueW(key, kRunValueName);
        ok = removed == ERROR_SUCCESS || removed == ERROR_FILE_NOT_FOUND;
    }

    RegCloseKey(key);
    return ok;
#else
    (void)enabled;
    return false;
#endif
}
