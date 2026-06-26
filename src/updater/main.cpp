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
    if (mode != L"--install") {
        LocalFree(argv);
        return 1;
    }

    const std::filesystem::path newFile = argv[2];
    const std::filesystem::path targetFile = argv[3];
    const DWORD pid = static_cast<DWORD>(std::wcstoul(argv[4], nullptr, 10));

    LocalFree(argv);

    waitForProcess(pid, 120000);

    if (!replaceExecutable(newFile, targetFile)) {
        return 2;
    }

    if (!launchExecutable(targetFile)) {
        return 3;
    }

    return 0;
}
