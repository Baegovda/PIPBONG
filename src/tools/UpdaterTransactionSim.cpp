#define wWinMain unusedUpdaterEntry
#include "../updater/main.cpp"
#undef wWinMain
#include <fstream>
#include <iostream>

int main() {
    namespace fs = std::filesystem;
    const fs::path root = fs::current_path() / (L"updater-test-" + std::to_wstring(GetCurrentProcessId()));
    const fs::path source = root / L"payload";
    const fs::path installed = root / L"installed";
    fs::create_directories(source / L"platforms");
    fs::create_directories(installed);
    auto write = [](const fs::path& path, const char* text) { std::ofstream(path) << text; };
    auto read = [](const fs::path& path) { std::ifstream in(path); return std::string(std::istreambuf_iterator<char>(in), {}); };
    auto pack = [&](const wchar_t* name) {
        const fs::path zip = root / name;
        if (!runProcess(L"tar.exe -a -cf \"" + zip.wstring() + L"\" -C \"" + source.wstring() + L"\" .", 10000)) return fs::path{};
        return zip;
    };
    write(installed / L"PIPBONG.exe", "old");
    write(source / L"PIPBONG.exe", "new");
    auto invalidZip = pack(L"invalid.zip");
    if (invalidZip.empty() || installFromZip(invalidZip, installed) || read(installed / L"PIPBONG.exe") != "old") return 1;
    for (const auto* file : {L"Qt6Core.dll", L"Qt6Gui.dll", L"Qt6Widgets.dll", L"platforms/qwindows.dll"}) write(source / file, "new");
    write(installed / L"Qt6Core.dll", "old-core");
    HANDLE locked = CreateFileW((installed / L"Qt6Core.dll").c_str(), GENERIC_READ, FILE_SHARE_READ,
                                 nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (locked == INVALID_HANDLE_VALUE) return 1;
    auto lockedZip = pack(L"locked.zip");
    const bool rejected = !lockedZip.empty() && !installFromZip(lockedZip, installed);
    CloseHandle(locked);
    if (!rejected || read(installed / L"PIPBONG.exe") != "old" || read(installed / L"Qt6Core.dll") != "old-core") return 1;
    // A failed rollback may retain its backup. Check a separate clean installation for success.
    const fs::path fresh = root / L"fresh";
    fs::create_directories(fresh);
    auto validZip = pack(L"valid.zip");
    if (validZip.empty() || !installFromZip(validZip, fresh) || read(fresh / L"PIPBONG.exe") != "new") return 1;
    fs::remove_all(root);
    std::cout << "UpdaterTransactionSim: invalid payload / locked destination rollback / install OK\n";
    return 0;
}
