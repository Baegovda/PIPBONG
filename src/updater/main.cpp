#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <string>
#include <vector>

namespace {

bool waitForProcess(DWORD pid, DWORD timeoutMs) {
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!process) {
        return true;
    }
    const DWORD result = WaitForSingleObject(process, timeoutMs);
    CloseHandle(process);
    return result == WAIT_OBJECT_0;
}

bool replaceExecutable(const std::filesystem::path& newFile, const std::filesystem::path& targetFile) {
    namespace fs = std::filesystem;
    std::error_code error;
    const fs::path backup = targetFile.wstring() + L".old";

    fs::remove(backup, error);
    error.clear();

    if (fs::exists(targetFile, error)) {
        fs::rename(targetFile, backup, error);
        if (error) {
            if (!MoveFileExW(targetFile.c_str(), backup.c_str(), MOVEFILE_REPLACE_EXISTING)) {
                return false;
            }
        }
    }

    error.clear();
    fs::rename(newFile, targetFile, error);
    if (error) {
        if (!MoveFileExW(newFile.c_str(), targetFile.c_str(), MOVEFILE_REPLACE_EXISTING)) {
            if (fs::exists(backup, error)) {
                fs::rename(backup, targetFile, error);
            }
            return false;
        }
    }

    fs::remove(backup, error);
    fs::remove(newFile, error);
    return true;
}

bool launchExecutable(const std::filesystem::path& targetFile) {
    namespace fs = std::filesystem;
    std::error_code error;
    const fs::path absoluteTarget = fs::absolute(targetFile, error);
    const std::wstring targetPath = absoluteTarget.wstring();
    const std::wstring workingDirectory = absoluteTarget.parent_path().wstring();

    auto requiresRunAsAdmin = [](const std::wstring& exePath) -> bool {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
                          L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers",
                          0,
                          KEY_READ,
                          &key) != ERROR_SUCCESS) {
            return false;
        }

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
        return value.find(L"RUNASADMIN") != std::wstring::npos;
    };

    const wchar_t* verb = requiresRunAsAdmin(targetPath) ? L"runas" : L"open";
    for (int attempt = 0; attempt < 8; ++attempt) {
        if (attempt > 0) {
            Sleep(400);
        }
        HINSTANCE result = ShellExecuteW(nullptr,
                                       verb,
                                       targetPath.c_str(),
                                       nullptr,
                                       workingDirectory.c_str(),
                                       SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) > 32) {
            return true;
        }
    }
    return false;
}

std::filesystem::path currentModulePath() {
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }
    return std::filesystem::path(buffer);
}

bool pathsReferToSameFile(const std::filesystem::path& left, const std::filesystem::path& right) {
    namespace fs = std::filesystem;
    std::error_code error;
    if (fs::equivalent(left, right, error)) {
        return true;
    }
    error.clear();
    const fs::path absoluteLeft = fs::absolute(left, error);
    const fs::path absoluteRight = fs::absolute(right, error);
    return _wcsicmp(absoluteLeft.wstring().c_str(), absoluteRight.wstring().c_str()) == 0;
}

bool runProcess(const std::wstring& commandLine, DWORD timeoutMs) {
    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    std::vector<wchar_t> buffer(commandLine.begin(), commandLine.end());
    buffer.push_back(L'\0');

    if (!CreateProcessW(nullptr,
                        buffer.data(),
                        nullptr,
                        nullptr,
                        FALSE,
                        CREATE_NO_WINDOW,
                        nullptr,
                        nullptr,
                        &startupInfo,
                        &processInfo)) {
        return false;
    }

    WaitForSingleObject(processInfo.hProcess, timeoutMs);
    DWORD exitCode = 1;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return exitCode == 0;
}

bool extractZipArchive(const std::filesystem::path& zipFile, const std::filesystem::path& destDir) {
    namespace fs = std::filesystem;
    std::error_code error;
    fs::create_directories(destDir, error);

    const std::wstring command =
        L"tar.exe -xf \"" + zipFile.wstring() + L"\" -C \"" + destDir.wstring() + L"\"";
    return runProcess(command, 300000);
}

bool installFromZip(const std::filesystem::path& zipFile, const std::filesystem::path& installDir) {
    namespace fs = std::filesystem;
    std::error_code error;
    const fs::path extractDir = installDir / L".pipbong-update-staging";
    const fs::path runningUpdater = currentModulePath();
    fs::remove_all(extractDir, error);
    error.clear();

    if (!extractZipArchive(zipFile, extractDir)) {
        fs::remove_all(extractDir, error);
        return false;
    }

    fs::path contentRoot = extractDir;
    {
        std::vector<fs::directory_entry> topLevel;
        for (const auto& entry : fs::directory_iterator(extractDir, error)) {
            if (error) {
                break;
            }
            topLevel.push_back(entry);
        }
        if (topLevel.size() == 1 && topLevel.front().is_directory(error) && !error) {
            contentRoot = topLevel.front().path();
        }
    }

    error.clear();
    for (const auto& entry :
         fs::recursive_directory_iterator(contentRoot, fs::directory_options::skip_permission_denied, error)) {
        if (!entry.is_regular_file(error)) {
            continue;
        }
        const fs::path relative = fs::relative(entry.path(), contentRoot, error);
        if (error) {
            continue;
        }
        const fs::path destination = installDir / relative;
        if (!runningUpdater.empty() && pathsReferToSameFile(destination, runningUpdater)) {
            continue;
        }
        fs::create_directories(destination.parent_path(), error);
        error.clear();
        fs::copy_file(entry.path(), destination, fs::copy_options::overwrite_existing, error);
        if (error) {
            fs::remove_all(extractDir, error);
            return false;
        }
    }

    fs::remove_all(extractDir, error);
    fs::remove(zipFile, error);
    return true;
}

} // namespace

int wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc < 5) {
        if (argv) {
            LocalFree(argv);
        }
        return 1;
    }

    const std::wstring mode = argv[1];
    const std::filesystem::path payload = argv[2];
    const std::filesystem::path target = argv[3];
    const DWORD pid = static_cast<DWORD>(std::wcstoul(argv[4], nullptr, 10));

    LocalFree(argv);

    waitForProcess(pid, 120000);

    bool installed = false;
    if (mode == L"--install") {
        installed = replaceExecutable(payload, target);
    } else if (mode == L"--install-zip") {
        installed = installFromZip(payload, target);
    } else {
        return 1;
    }

    if (!installed) {
        return 2;
    }

    const std::filesystem::path launchTarget =
        (mode == L"--install-zip") ? target / L"PIPBONG.exe" : target;
    if (!launchExecutable(launchTarget)) {
        return 3;
    }

    return 0;
}
