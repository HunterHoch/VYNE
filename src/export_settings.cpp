#include "export_settings.h"
#include <shobjidl.h>
#include <shlobj.h>
#include <mutex>
#include <atomic>
#include <vector>
#include <wrl/client.h>

namespace {
std::mutex lock;
vyne::ExportSettings settings;
std::filesystem::path configuration;
std::atomic_bool picker_open{false};
std::atomic_bool refresh_pending{false};
constexpr unsigned widths[] = {1920, 2560, 3840};
constexpr unsigned heights[] = {1080, 1440, 2160};

bool save_locked() {
    const auto content = L"\xfeff[Export]\r\nResolutionCheck=" + std::to_wstring(settings.resolution_check ? 1 : 0) +
        L"\r\nResolution=" + std::to_wstring(settings.resolution) + L"\r\nDirectory=" + settings.output.wstring() + L"\r\n";
    auto temporary = configuration; temporary += L".tmp";
    const auto file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const auto bytes = static_cast<DWORD>(content.size() * sizeof(wchar_t));
    const bool good = WriteFile(file, content.data(), bytes, &written, nullptr) && written == bytes && FlushFileBuffers(file);
    CloseHandle(file);
    if (!good) return false;
    if (!MoveFileExW(temporary.c_str(), configuration.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return false;
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, configuration.c_str());
    return true;
}

DWORD WINAPI folder_thread(void* parameter) {
    const auto owner = static_cast<HWND>(parameter);
    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(initialized)) {
        try {
        Microsoft::WRL::ComPtr<IFileDialog> dialog;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
            FILEOPENDIALOGOPTIONS options{};
            if (SUCCEEDED(dialog->GetOptions(&options)) &&
                SUCCEEDED(dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR))) {
                dialog->SetTitle(L"VYNE export directory");
                if (SUCCEEDED(dialog->Show(owner))) {
                    Microsoft::WRL::ComPtr<IShellItem> item;
                    if (SUCCEEDED(dialog->GetResult(&item))) {
                        PWSTR path = nullptr;
                        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                            std::lock_guard guard(lock);
                            const auto previous = settings.output;
                            settings.output = path;
                            if (!save_locked()) settings.output = previous;
                            CoTaskMemFree(path);
                        }
                    }
                }
            }
        }
        } catch (...) { OutputDebugStringW(L"VYNE: folder picker failed; previous directory preserved where possible.\n"); }
        CoUninitialize();
    }
    picker_open.store(false);
    refresh_pending.store(true);
    return 0;
}
}

void vyne::initialize_export_settings(const std::filesystem::path& directory) {
    std::lock_guard guard(lock);
    std::filesystem::create_directories(directory);
    configuration = directory / L"Export.ini";
    settings.resolution_check = GetPrivateProfileIntW(L"Export", L"ResolutionCheck", 0, configuration.c_str()) == 1;
    settings.resolution = GetPrivateProfileIntW(L"Export", L"Resolution", 2, configuration.c_str());
    if (settings.resolution >= 3) settings.resolution = 2;
    std::vector<wchar_t> path(32768);
    GetPrivateProfileStringW(L"Export", L"Directory", L"", path.data(), static_cast<DWORD>(path.size()), configuration.c_str());
    settings.output = path.data();
    if (settings.output.empty()) {
        PWSTR videos = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Videos, 0, nullptr, &videos))) {
            settings.output = std::filesystem::path(videos) / L"VYNE";
            CoTaskMemFree(videos);
        }
    }
}

vyne::ExportSettings vyne::export_settings() { std::lock_guard guard(lock); return settings; }
bool vyne::resolution_check_enabled() noexcept { std::lock_guard guard(lock); return settings.resolution_check; }
unsigned vyne::export_width(unsigned index) noexcept { return widths[index < 3 ? index : 2]; }
unsigned vyne::export_height(unsigned index) noexcept { return heights[index < 3 ? index : 2]; }
void vyne::step_export_setting(ExportRow row, int direction) {
    if (!direction) return;
    std::lock_guard guard(lock);
    const auto previous = settings;
    if (row == ExportRow::mode) settings.resolution_check = !settings.resolution_check;
    else if (row == ExportRow::resolution) settings.resolution = (settings.resolution + (direction > 0 ? 1 : 2)) % 3;
    if (!save_locked()) settings = previous;
}
bool vyne::export_row_enabled(ExportRow row) noexcept {
    return row == ExportRow::mode || row == ExportRow::directory ||
        (row == ExportRow::resolution && resolution_check_enabled());
}
const char* vyne::export_row_label(ExportRow row) noexcept {
    constexpr const char* labels[] = {"VYNE export", "Check resolution", "VYNE frame rate", "VYNE bitrate", "VYNE file format", "VYNE color profile", "Output location"};
    const auto i = static_cast<unsigned>(row); return i < 7 ? labels[i] : "VYNE";
}
const char* vyne::export_row_help(ExportRow row) noexcept {
    switch (row) {
    case ExportRow::mode: return "Native export uses Rockstar's encoder and copies the finished MP4 to Output location. Resolution check only tests the selected render mode for five seconds and restores; it creates no video.";
    case ExportRow::resolution: return "Applies only to Resolution check, which creates no video. Native export uses Rockstar's own output dimensions; this control cannot produce a 4K clip. VYNE full-resolution capture is not implemented yet.";
    case ExportRow::directory: return "Press left or right to choose where finished native MP4 exports are copied. The Video Gallery original is preserved. The Windows folder picker runs separately from the render thread.";
    case ExportRow::fps: return "VYNE replay timing is not implemented yet. Use the stock frame-rate control with Native export.";
    case ExportRow::bitrate: return "VYNE encoding is not implemented yet. Use the stock bitrate control with Native export.";
    case ExportRow::format: return "MP4/H.264 and PNG sequence backends are not implemented yet. This option stays disabled until real output is available.";
    default: return "VYNE color conversion and output tagging are not implemented yet. No unsupported color profile is advertised.";
    }
}
std::string vyne::export_row_value(ExportRow row) {
    const auto value = export_settings();
    switch (row) {
    case ExportRow::mode: return value.resolution_check ? "Resolution check" : "Native export";
    case ExportRow::resolution: return value.resolution_check ? std::to_string(export_width(value.resolution)) + " x " + std::to_string(export_height(value.resolution)) : "Game settings";
    case ExportRow::directory: {
        if (picker_open.load()) return "Choosing folder...";
        auto name = value.output.lexically_normal().filename().wstring();
        if (name.empty()) name = value.output.parent_path().filename().wstring();
        if (name.empty()) name = value.output.root_path().wstring();
        if (name.empty()) return "Choose folder";
        // Keep the native value column short; full path lives in linked help.
        // Do not split a UTF-16 surrogate pair when abbreviating folder names.
        if (name.size() > 12) {
            size_t keep = 11;
            if (name[keep - 1] >= 0xd800 && name[keep - 1] <= 0xdbff) --keep;
            name.resize(keep); name += L"\x2026";
        }
        return std::filesystem::path(name).u8string();
    }
    default: return "Not available yet";
    }
}
void vyne::choose_export_directory(HWND owner) {
    if (picker_open.exchange(true)) return;
    const auto thread = CreateThread(nullptr, 0, folder_thread, owner, 0, nullptr);
    if (thread) CloseHandle(thread);
    else { picker_open.store(false); refresh_pending.store(true); }
}
bool vyne::consume_export_settings_refresh() noexcept { return refresh_pending.exchange(false); }
