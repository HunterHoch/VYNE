#include "export_menu.h"
#include "export_menu_layout.h"
#include "export_settings.h"
#include "native_mode_probe.h"
#include "hook_retry.h"
#include "native_output.h"
#include <MinHook.h>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <atomic>
#include <array>

namespace {
using Build = int(*)(unsigned, char, unsigned);
using Adjust = void(*)(int, unsigned*);
using ToggleText = const char*(*)(unsigned*, unsigned*);
using Text = const char*(*)(void*, unsigned, const char*);
using Selectable = bool(*)(unsigned, int, unsigned);
using Trigger = void(*)();
using Refresh = void(*)(int);
using Allocate = void*(*)(std::uint64_t, std::uint64_t);
using Free = void(*)(void*);
Build original_build = nullptr;
Adjust original_adjust = nullptr;
ToggleText original_toggle = nullptr;
Text original_text = nullptr;
Selectable original_selectable = nullptr;
Trigger original_trigger = nullptr;
Allocate allocate = nullptr;
Free release = nullptr;
Refresh refresh_menu = nullptr;
vyne::NativeArray<vyne::NativeEditorMenu>* menus = nullptr;
int* active_column = nullptr;
int* selected_items = nullptr;
int* column_menus = nullptr;
std::filesystem::path directory;
std::atomic_int export_index{-1}, description_index{-1};
std::atomic_uint description_header{0}, description_body{0};
thread_local int building = -1;
thread_local std::string value_text;
std::atomic_bool export_refused{false};

bool readable(const void* pointer, size_t size) noexcept {
    MEMORY_BASIC_INFORMATION info{};
    if (!pointer || !VirtualQuery(pointer, &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    const auto p = reinterpret_cast<uintptr_t>(pointer);
    const auto end = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    return p <= end && size <= end - p;
}
void note(const std::string& message) {
    std::ofstream log(directory / L"export-menu.log", std::ios::app);
    log << "tick=" << GetTickCount64() << ' ' << message << '\n';
}

vyne::NativeEditorMenu* menu(unsigned index) noexcept {
    if (!readable(menus, sizeof(*menus)) || !menus->data || menus->count > 512 || index >= menus->count || menus->capacity < menus->count)
        return nullptr;
    const auto item = menus->data + index;
    return readable(item, sizeof(*item)) ? item : nullptr;
}
vyne::NativeExportOption* option(vyne::NativeEditorMenu* item, unsigned index) noexcept {
    if (!item || !item->options.data || index >= item->options.count || item->options.count > 16 || item->options.capacity < item->options.count)
        return nullptr;
    auto row = item->options.data + index;
    return readable(row, sizeof(*row)) ? row : nullptr;
}
int focused_row() noexcept {
    if (!readable(active_column, 4) || *active_column < 0 || *active_column > 2) return -1;
    if (!readable(column_menus, 12) || !readable(selected_items, 12)) return -1;
    const auto column = *active_column;
    if (column_menus[column] != export_index.load() || selected_items[column] < 0) return -1;
    auto row = option(menu(static_cast<unsigned>(column_menus[column])), static_cast<unsigned>(selected_items[column]));
    return row ? vyne::export_option_row(row->toggle) : -1;
}

bool inject() {
    if (!readable(menus, sizeof(*menus)) || !menus->data || !menus->count || menus->count > 512) return false;
    for (unsigned index = 0; index < menus->count; ++index) {
        auto item = menu(index);
        if (!item || item->id != 0x65dd979d) continue; // Independently hashed EXPORT_SETTINGS.
        if (item->type != 0 || item->data_type != 0 || !item->options.count || item->options.count > 16) return false;
        if (!readable(item->options.data, item->options.count * sizeof(vyne::NativeExportOption))) return false;
        for (unsigned i = 0; i < item->options.count; ++i) {
            if (vyne::export_option_row(item->options.data[i].toggle) >= 0) { export_index.store(static_cast<int>(index)); return true; }
        }
        const auto added = vyne::compose_export_options(item->options.data, item->options.count);
        if (added.empty()) { note("injection refused: no stock toggle template or native 16-row limit exceeded"); return false; }
        auto replacement = static_cast<vyne::NativeExportOption*>(allocate(added.size() * sizeof(added[0]), 16));
        if (!replacement) { note("game allocator refused menu buffer"); return false; }
        std::memcpy(replacement, added.data(), added.size() * sizeof(added[0]));
        auto previous = item->options.data;
        item->options.data = replacement;
        item->options.count = item->options.capacity = static_cast<std::uint16_t>(added.size());
        release(previous); // Native array takes ownership; its normal Close frees replacement.
        description_index.store(-1); description_header.store(0); description_body.store(0);
        unsigned linked = 0;
        for (const auto& row : added) if (row.toggle == vyne::export_option_key(0)) linked = row.link;
        for (unsigned other = 0; other < menus->count; ++other) {
            auto panel = menu(other);
            if (linked && panel && panel->id == linked && readable(panel->content, 8)) {
                const auto text = static_cast<const unsigned*>(panel->content);
                description_header.store(text[0]); description_body.store(text[1]); description_index.store(static_cast<int>(other)); break;
            }
        }
        export_index.store(static_cast<int>(index));
        note("native EXPORT_SETTINGS: inserted 7 VYNE rows; total=" + std::to_string(added.size()));
        return true;
    }
    return false;
}

int build(unsigned index, char branches, unsigned start_column) {
    static bool observed = false;
    if (!observed) { observed = true; note("native BuildMenu callback reached; first index=" + std::to_string(index)); }
    try { inject(); } catch (...) { OutputDebugStringW(L"VYNE: native menu injection refused.\n"); }
    const auto previous = building;
    building = static_cast<int>(index);
    const auto result = original_build(index, branches, start_column);
    building = previous;
    return result;
}

bool selectable(unsigned index, int column, unsigned selected) {
    if (static_cast<int>(index) == export_index.load()) {
        if (auto row = option(menu(index), selected)) {
            const auto ours = vyne::export_option_row(row->toggle);
            if (ours >= 0) return !vyne::resolution_check_busy() && vyne::export_row_enabled(static_cast<vyne::ExportRow>(ours));
            // While in check mode the original FPS/bitrate do not configure a
            // video export. Keep them intact and re-enable them when VYNE is off.
            if (row->toggle && vyne::resolution_check_enabled()) return false;
        }
    }
    return original_selectable(index, column, selected);
}

void adjust(int direction, unsigned* hash) {
    const auto row = hash ? vyne::export_option_row(*hash) : -1;
    if (row >= 0 && focused_row() == row && !vyne::resolution_check_busy() && vyne::export_row_enabled(static_cast<vyne::ExportRow>(row))) {
        try {
            if (row == static_cast<int>(vyne::ExportRow::directory)) vyne::choose_export_directory(GetForegroundWindow());
            else vyne::step_export_setting(static_cast<vyne::ExportRow>(row), direction);
            export_refused.store(false);
        } catch (...) { OutputDebugStringW(L"VYNE: failed to save export option.\n"); }
    }
    original_adjust(direction, hash); // Native selection sound/redraw; private hashes match no stock setter.
}

const char* toggle_text(unsigned* hash, unsigned* unique) {
    const auto row = hash ? vyne::export_option_row(*hash) : -1;
    const auto index = export_index.load();
    const auto item = index >= 0 ? menu(static_cast<unsigned>(index)) : nullptr;
    if (row >= 0 && item && item->id == 0x65dd979d) {
        try { value_text = vyne::export_row_value(static_cast<vyne::ExportRow>(row)); return value_text.c_str(); }
        catch (...) { return "Unavailable"; }
    }
    return original_toggle(hash, unique);
}

const char* text(void* self, unsigned hash, const char* debug) {
    if (building == export_index.load() && building >= 0) {
        const auto row = vyne::export_option_row(hash);
        if (row >= 0) return vyne::export_row_label(static_cast<vyne::ExportRow>(row));
    }
    if (building == description_index.load() && building >= 0 && hash) {
        const auto row = focused_row();
        if (row >= 0) {
            if (hash == description_header.load()) return vyne::export_row_label(static_cast<vyne::ExportRow>(row));
            if (hash == description_body.load()) {
                if (row == static_cast<int>(vyne::ExportRow::directory)) {
                    value_text = vyne::native_output_status() + " " + vyne::export_row_help(vyne::ExportRow::directory);
                    return value_text.c_str();
                }
                return vyne::export_row_help(static_cast<vyne::ExportRow>(row));
            }
        } else if (hash == description_body.load() && vyne::resolution_check_enabled()) {
            return export_refused.load() ? "VYNE could not start the resolution check. See mode-probe.log; native export was not started." : vyne::resolution_check_status();
        }
    }
    return original_text(self, hash, debug);
}

void trigger_export() {
    try {
    const auto settings = vyne::export_settings();
    if (!settings.resolution_check) {
        try { vyne::watch_native_export(settings.output); }
        catch (...) { OutputDebugStringW(L"VYNE: output watch unavailable; native exporter remains available.\n"); }
        original_trigger(); return;
    }
    const bool queued = vyne::queue_resolution_check(vyne::export_width(settings.resolution), vyne::export_height(settings.resolution));
    export_refused.store(!queued);
    note(queued ? "native Export pressed: queued VYNE resolution check; no bake/file started" : "native Export pressed: mode check unavailable/busy; native bake not started");
    if (refresh_menu) refresh_menu(-1);
    } catch (...) { OutputDebugStringW(L"VYNE: Export preflight failed; no stock fallback started.\n"); }
}

bool matches(uintptr_t base, uintptr_t rva, const char* bytes, size_t size) {
    const auto pointer = reinterpret_cast<const void*>(base + rva);
    return readable(pointer, size) && std::memcmp(pointer, bytes, size) == 0;
}
}

void vyne::install_export_menu(HMODULE module) noexcept {
    try {
        wchar_t host[32768]{}; GetModuleFileNameW(nullptr, host, 32768);
        if (std::filesystem::path(host).filename() != L"FiveM_b3258_GTAProcess.exe") return;
        wchar_t path[32768]{}; if (!GetModuleFileNameW(module, path, 32768)) return;
        directory = std::filesystem::path(path).parent_path() / L"VYNE";
        std::filesystem::create_directories(directory);
        if (GetPrivateProfileIntW(L"Diagnostics", L"EnableExportMenu", 1, (directory / L"VYNE.ini").c_str()) != 1) return;
        initialize_export_settings(directory);
        const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        struct Hook { uintptr_t rva; const char* guard; size_t size; void* replacement; void** original; };
        const std::array<Hook, 6> hooks{{
            {0x15eff8, "\x88\x54\x24\x10\x89\x4c\x24\x08\x55\x53\x56\x57", 12, reinterpret_cast<void*>(&build), reinterpret_cast<void**>(&original_build)},
            {0x15cc74, "\x48\x89\x5c\x24\x08\x48\x89\x74\x24\x10\x48\x89\x7c\x24\x18", 15, reinterpret_cast<void*>(&adjust), reinterpret_cast<void**>(&original_adjust)},
            {0x1811e0, "\x48\x89\x5c\x24\x10\x48\x89\x7c\x24\x18\x55\x48\x8b\xec", 14, reinterpret_cast<void*>(&toggle_text), reinterpret_cast<void**>(&original_toggle)},
            {0xdc05fc, "\x48\x83\xec\x28", 4, reinterpret_cast<void*>(&text), reinterpret_cast<void**>(&original_text)},
            {0x192b1c, "\x48\x89\x5c\x24\x08\x48\x89\x6c\x24\x10\x48\x89\x74\x24\x20", 15, reinterpret_cast<void*>(&selectable), reinterpret_cast<void**>(&original_selectable)},
            {0x1b6a34, "\x48\x83\xec\x38\x48\x8b\x05\x11\x38\xe6\x01", 11, reinterpret_cast<void*>(&trigger_export), reinterpret_cast<void**>(&original_trigger)}
        }};
        for (const auto& hook : hooks) if (!matches(base, hook.rva, hook.guard, hook.size)) {
            note("hook validation refused at RVA=" + std::to_string(hook.rva)); return;
        }
        if (!matches(base, 0x12e0, "\x44\x8b\x05\xe9\x5d\x02\x03", 7) ||
            !matches(base, 0x1318, "\x48\x83\xec\x28\x4c\x8b\xc1\x48\x85\xc9", 10) ||
            !matches(base, 0xdc0605, "\x48\x85\xc0", 3)) {
            note("allocator/text-wrapper guard refused; menu left stock"); return;
        }
        menus = reinterpret_cast<NativeArray<NativeEditorMenu>*>(base + 0x201bf98);
        active_column = reinterpret_cast<int*>(base + 0x20191ac);
        selected_items = reinterpret_cast<int*>(base + 0x20191a0);
        column_menus = reinterpret_cast<int*>(base + 0x1d6ae60);
        allocate = reinterpret_cast<Allocate>(base + 0x12e0);
        release = reinterpret_cast<Free>(base + 0x1318);
        if (matches(base, 0x19e664, "\x48\x89\x5c\x24\x08\x57\x48\x83\xec\x20", 10))
            refresh_menu = reinterpret_cast<Refresh>(base + 0x19e664);
        const auto init = MH_Initialize();
        if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) { note("MinHook initialization failed"); return; }
        unsigned created = 0;
        for (; created < hooks.size(); ++created) {
            const auto& hook = hooks[created];
            if (MH_CreateHook(reinterpret_cast<void*>(base + hook.rva), hook.replacement, hook.original) != MH_OK) break;
        }
        if (created != hooks.size()) {
            for (unsigned i = 0; i < created; ++i) MH_RemoveHook(reinterpret_cast<void*>(base + hooks[i].rva));
            note("hook creation failed; no VYNE menu hooks enabled"); return;
        }
        HMODULE pinned = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&install_export_menu), &pinned)) {
            for (const auto& hook : hooks) MH_RemoveHook(reinterpret_cast<void*>(base + hook.rva));
            return;
        }
        for (const auto& hook : hooks) {
            if (MH_QueueEnableHook(reinterpret_cast<void*>(base + hook.rva)) != MH_OK) {
                for (const auto& cleanup : hooks) MH_RemoveHook(reinterpret_cast<void*>(base + cleanup.rva));
                note("hook queue failed; menu left stock"); return;
            }
        }
        unsigned attempts = 0;
        DWORD win32_error = 0;
        const auto applied = retry_hook_snapshot([&]() {
            const auto result = MH_ApplyQueued(); win32_error = GetLastError(); return result;
        }, [](unsigned delay) { Sleep(delay); }, attempts);
        if (applied != MH_OK) {
            note("hook enable failed: " + std::string(MH_StatusToString(applied)) + " Win32=" + std::to_string(win32_error) + " attempts=" + std::to_string(attempts));
            for (const auto& hook : hooks) {
                unsigned cleanup_attempts = 0;
                const auto disabled = retry_hook_snapshot([&]() { return MH_DisableHook(reinterpret_cast<void*>(base + hook.rva)); }, [](unsigned delay) { Sleep(delay); }, cleanup_attempts);
                if (disabled == MH_OK || disabled == MH_ERROR_DISABLED) MH_RemoveHook(reinterpret_cast<void*>(base + hook.rva));
                else note("hook cleanup failed RVA=" + std::to_string(hook.rva) + " " + MH_StatusToString(disabled));
            }
            return;
        }
        note("native menu hook batch enabled after attempts=" + std::to_string(attempts));
        note("native menu hooks enabled for build3258; data injection occurs on native BuildMenu");
    } catch (...) { OutputDebugStringW(L"VYNE: native Export menu initialization failed.\n"); }
}
