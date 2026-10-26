#include <windows.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include "native_mode_probe.h"
#include "export_menu.h"

namespace {
HMODULE plugin_module = nullptr;
INIT_ONCE initialized = INIT_ONCE_STATIC_INIT;
HANDLE startup_thread = nullptr;

std::wstring module_path(HMODULE module) {
    std::vector<wchar_t> buffer(32768);
    const auto length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) return L"";
    return {buffer.data(), length};
}

void write_version(std::wofstream& log, const std::wstring& path) {
    DWORD unused = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &unused);
    std::vector<BYTE> data(size);
    if (!size || !GetFileVersionInfoW(path.c_str(), 0, size, data.data())) {
        log << L"host_file_version=unavailable\n";
        return;
    }
    VS_FIXEDFILEINFO* info = nullptr;
    UINT length = 0;
    if (VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &length)
        && length >= sizeof(VS_FIXEDFILEINFO) && info->dwSignature == 0xfeef04bd) {
        log << L"host_file_version=" << HIWORD(info->dwFileVersionMS) << L'.'
            << LOWORD(info->dwFileVersionMS) << L'.' << HIWORD(info->dwFileVersionLS)
            << L'.' << LOWORD(info->dwFileVersionLS) << L"\n";
    }
}

BOOL CALLBACK diagnose(PINIT_ONCE, PVOID, PVOID*) noexcept {
    try {
        const auto own_path = module_path(plugin_module);
        if (own_path.empty()) return FALSE;
        const auto directory = std::filesystem::path(own_path).parent_path() / L"VYNE";
        std::filesystem::create_directories(directory);
        std::wofstream log(directory / L"diagnostics.log", std::ios::app);
        if (!log) return FALSE;
        SYSTEMTIME time{};
        GetSystemTime(&time);
        log << L"VYNE 0.4.2 diagnostics UTC " << time.wYear << L'-' << time.wMonth
            << L'-' << time.wDay << L' ' << time.wHour << L':' << time.wMinute << L':' << time.wSecond
            << L"\npid=" << GetCurrentProcessId() << L"\nplugin=" << own_path << L"\n";
        const auto host = module_path(nullptr);
        log << L"host=" << host << L"\n";
        write_version(log, host);
        for (const auto name : {L"d3d11.dll", L"d3d12.dll", L"dxgi.dll", L"ScriptHookV.dll"}) {
            const auto module = GetModuleHandleW(name);
            log << name << L"=" << (module ? module_path(module) : L"not loaded") << L"\n";
        }
        log << L"render_api=unverified (loaded DLLs do not prove active renderer)\n"
            << L"game_build=unverified (host file version is diagnostic only)\n"
            << L"export_hooks=pending_validation (see export-menu.log)\nrender_target_dimensions=unmeasured\n";
        log.flush();
        return log.good() ? TRUE : FALSE;
    } catch (...) {
        OutputDebugStringW(L"VYNE: diagnostics failed; no game changes applied.\n");
        return FALSE;
    }
}
}

// Explicit initialization also supports isolated host checks. FiveM itself
// uses LoadLibrary, so its startup path is the worker scheduled by DllMain.
extern "C" __declspec(dllexport) BOOL VYNE_RunDiagnostics() noexcept {
    return InitOnceExecuteOnce(&initialized, diagnose, nullptr, nullptr);
}

extern "C" __declspec(dllexport) void InitializeASI() noexcept {
    VYNE_RunDiagnostics();
}

namespace {
DWORD WINAPI startup(void*) {
    // FiveM uses LoadLibrary rather than InitializeASI. The new thread starts
    // after DllMain releases the loader lock; keep our code mapped while it runs.
    HMODULE reference = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(&startup), &reference)) return 1;
    VYNE_RunDiagnostics();
    vyne::install_export_menu(plugin_module);
    // Graphics objects can exist while FiveM is still constructing them.
    // The native Export menu schedules graphics setup after its first build;
    // never touch the swapchain/device or install graphics hooks at startup.
    FreeLibraryAndExitThread(reference, 0);
}
}

extern "C" __declspec(dllexport) BOOL VYNE_WaitForStartup(DWORD timeout) noexcept {
    return startup_thread && WaitForSingleObject(startup_thread, timeout) == WAIT_OBJECT_0;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        plugin_module = module;
        startup_thread = CreateThread(nullptr, 0, startup, nullptr, 0, nullptr);
        if (!startup_thread) return FALSE;
    } else if (reason == DLL_PROCESS_DETACH && startup_thread) {
        CloseHandle(startup_thread);
    }
    return TRUE;
}
