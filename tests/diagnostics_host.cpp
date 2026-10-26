#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main() {
    const auto binary = std::filesystem::absolute("build/VYNE.asi");
    const auto log_path = binary.parent_path() / "VYNE/diagnostics.log";
    const auto module = LoadLibraryW(binary.c_str());
    if (!module) return 1;
    const auto wait = reinterpret_cast<BOOL(*)(DWORD)>(GetProcAddress(module, "VYNE_WaitForStartup"));
    if (!wait || !wait(5000)) return 5;
    if (!FindResourceW(module, L"FX_ASI_BUILD", MAKEINTRESOURCEW(3258))) return 6;
    const auto run = reinterpret_cast<BOOL(*)()>(GetProcAddress(module, "VYNE_RunDiagnostics"));
    if (!run || !run()) return 2;
    const auto first_size = std::filesystem::file_size(log_path);
    if (!run() || std::filesystem::file_size(log_path) != first_size) return 3;
    std::ifstream input(log_path);
    const std::string log{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (log.find("export_hooks=disabled") == std::string::npos ||
        log.find("render_target_dimensions=unmeasured") == std::string::npos) return 4;
    FreeLibrary(module);
    std::cout << "PASS: x64 ASI load, exported diagnostics, file output, once-only initialization, unload.\n";
    return 0;
}
