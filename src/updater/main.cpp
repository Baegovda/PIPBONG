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
    const std::wstring workingDirectory = targetFile.parent_path().wstring();
    HINSTANCE result = ShellExecuteW(nullptr,
                                   L"open",
                                   targetFile.c_str(),
                                   nullptr,
                                   workingDirectory.c_str(),
                                   SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
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
