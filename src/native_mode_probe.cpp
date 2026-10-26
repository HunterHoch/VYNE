#include "native_mode_probe.h"
#include "graphics_mode.h"
#include "native_resolution.h"
#include <d3d11.h>
#include <dxgi.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <cstring>

namespace {
using Present = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using ChangeMode = bool(*)(std::uint32_t*);
std::atomic<Present> original_present{nullptr};
IDXGISwapChain* game_chain = nullptr;
ChangeMode change_mode = nullptr;
std::uint32_t* requested_mode = nullptr;
std::filesystem::path directory;
std::unique_ptr<vyne::GraphicsModeSession> session;
std::uint64_t frame_number = 0, restore_at = 0;
bool key_down = false;
bool hotkeys_enabled = false;
std::atomic_bool probe_ready{false}, probe_busy{false};
std::atomic<std::uint64_t> queued_dimensions{0};
std::atomic_int probe_phase{0};
std::atomic_bool export_mode{false}, export_started{false}, export_failed{false};
std::atomic_int export_result{0};
std::atomic<std::uint64_t> export_dimensions{0};
std::uint64_t export_deadline = 0;
std::uint64_t finalize_deadline = 0;
std::uint64_t next_mode_observation = 0;
bool* playback_active = nullptr;
int* playback_type = nullptr;
bool* delayed_close = nullptr;

void note(const std::string& message) {
    std::ofstream log(directory / L"mode-probe.log", std::ios::app);
    log << "tick=" << GetTickCount64() << ' ' << message << '\n';
}

bool readable(const void* pointer, size_t count) noexcept {
    MEMORY_BASIC_INFORMATION info{};
    if (!pointer || !VirtualQuery(pointer, &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const auto begin = reinterpret_cast<uintptr_t>(pointer);
    const auto end = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    return begin <= end && count <= end - begin;
}

bool snapshot(void*, vyne::GraphicsState& out) noexcept {
    if (!game_chain || !readable(requested_mode, 24)) return false;
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(game_chain->GetDesc(&desc)) || !desc.Windowed) return false;
    std::memcpy(out.native_state.data(), requested_mode, 24);
    // The engine's queued mode must already match the real swapchain before
    // taking a baseline; otherwise another mode change is still in flight.
    if (out.native_state[4] != 0 || out.native_state[0] != desc.BufferDesc.Width || out.native_state[1] != desc.BufferDesc.Height)
        return false;
    out.width = desc.BufferDesc.Width; out.height = desc.BufferDesc.Height;
    out.refresh_numerator = desc.BufferDesc.RefreshRate.Numerator;
    out.refresh_denominator = desc.BufferDesc.RefreshRate.Denominator;
    if (!out.refresh_denominator) out.refresh_denominator = 1;
    out.window_mode = 1; // Probe supports an already-windowed swapchain only.
    return true;
}

bool request(void*, const vyne::GraphicsState& state) noexcept {
    auto native = state.native_state;
    native[0] = state.width; native[1] = state.height;
    // Leave refresh/fullscreen/opaque sixth field exactly as captured. Calling
    // the game-owned entry respects FiveM's own wrapper and resource reset.
    // Its false return can mean queued, not rejected; verify queue and pixels.
    if (vyne::native_resolution_available()) vyne::set_resolution_window_target(state.width, state.height);
    const auto accepted = change_mode(native.data());
    const auto queued = readable(requested_mode, 24) && std::memcmp(requested_mode, native.data(), 24) == 0;
    try { note("native_mode_request=" + std::to_string(state.width) + "x" + std::to_string(state.height) +
        " return=" + std::to_string(accepted) + " queue_matches=" + std::to_string(queued)); } catch (...) {}
    return queued;
}

bool dimensions(void*, vyne::RenderDimensions& out) noexcept {
    ID3D11Texture2D* buffer = nullptr;
    if (FAILED(game_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&buffer)))) return false;
    D3D11_TEXTURE2D_DESC desc{};
    buffer->GetDesc(&desc); buffer->Release();
    out = {desc.Width, desc.Height, frame_number};
    return true;
}

bool write_recovery(const vyne::GraphicsState& state) {
    std::ofstream recovery(directory / L"mode-recovery.txt", std::ios::trunc);
    recovery << "VYNE build3258 probe original mode\n" << state.width << ' ' << state.height << '\n';
    for (auto field : state.native_state) recovery << field << ' ';
    recovery << "\nProbe was started; verify original graphics state after a hard crash.\n";
    recovery.flush();
    return recovery.good();
}

void probe_tick() {
    ++frame_number;
    const auto now = GetTickCount64();
    const bool pressed = hotkeys_enabled && (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    DXGI_SWAP_CHAIN_DESC desc{};
    const bool foreground = SUCCEEDED(game_chain->GetDesc(&desc)) &&
        GetForegroundWindow() == desc.OutputWindow;
    auto queued = queued_dimensions.exchange(0);
    if (pressed && !key_down && foreground && !session && !probe_busy.exchange(true)) {
        queued = (std::uint64_t{3840} << 32) | 2160;
    }
    if (queued && foreground && !session) {
        vyne::GraphicsState before{};
        if (!snapshot(nullptr, before) || !write_recovery(before)) {
            note("refused: stable windowed mode/recovery snapshot unavailable");
            probe_phase.store(-1); probe_busy.store(false);
        } else {
            auto target = before; target.width = static_cast<unsigned>(queued >> 32); target.height = static_cast<unsigned>(queued);
            target.native_state[0] = target.width; target.native_state[1] = target.height;
            session = std::make_unique<vyne::GraphicsModeSession>(
                vyne::GraphicsBackend{nullptr, snapshot, request, dimensions, true});
            note("requested=" + std::to_string(target.width) + "x" + std::to_string(target.height) + " original=" + std::to_string(before.width) + "x" + std::to_string(before.height));
            probe_phase.store(2);
            session->begin(target, now);
            if (!session->error().empty()) note(session->error());
        }
    } else if (queued) {
        probe_phase.store(-1); probe_busy.store(false);
        note("menu request refused: game not focused or another session is active");
    }
    key_down = pressed;
    if (!session) return;
    if (now >= next_mode_observation) {
        next_mode_observation = now + 1000;
        vyne::RenderDimensions actual{}; const auto measured = dimensions(nullptr, actual);
        note("mode_wait state=" + std::to_string(static_cast<int>(session->status())) +
            " GPU=" + (measured ? std::to_string(actual.width) + "x" + std::to_string(actual.height) : "unavailable") +
            " focused=" + std::to_string(foreground));
    }
    const auto prior = session->status();
    const bool exporting = export_mode.load();
    const bool running = export_started.load();
    bool cancel = (GetAsyncKeyState(VK_F7) & 0x8000) ||
        (!foreground && !running) || (exporting && export_failed.load());
    if (exporting && cancel) export_failed.store(true);
    bool native_bake_running = false;
    if (exporting && running) {
        if (!export_deadline) export_deadline = now + 30ull * 60 * 1000;
        const bool baking = readable(playback_active, 1) && *playback_active &&
            readable(playback_type, 4) && *playback_type == 1;
        native_bake_running = baking;
        if (baking) {
            vyne::RenderDimensions measured{};
            const auto expected = export_dimensions.load();
            if (!dimensions(nullptr, measured) || measured.width != static_cast<unsigned>(expected >> 32) ||
                measured.height != static_cast<unsigned>(expected)) {
                export_failed.store(true); cancel = true;
            }
        }
        if (!baking) {
            if (!finalize_deadline) finalize_deadline = now + 30000;
            const auto encoder = vyne::resolution_encoder_result();
            if (encoder || export_failed.load() || now >= finalize_deadline) {
                if (!vyne::resolution_gpu_verified() || encoder != 1) export_failed.store(true);
                vyne::reset_resolution_gpu_observation();
                session->finish(now);
            }
        } else if (cancel || (export_deadline && now >= export_deadline)) {
            export_failed.store(true);
            // Native Update owns teardown. Do not Close playback inside Present.
            if (readable(delayed_close, 1)) *delayed_close = true;
            probe_phase.store(8);
        }
    } else if (cancel) {
        if (session->status() == vyne::ModeStatus::switching || session->status() == vyne::ModeStatus::ready)
            note("cancel_reason=" + std::string(!foreground ? "focus_lost" : export_failed.load() ? "export_failure" : "F7"));
        session->cancel(now);
    }
    // Resource reset must wait for native playback teardown. In particular,
    // the generic session's ready-state drift handler would otherwise restore
    // while GTA still owns active encoder/capture resources.
    if (!native_bake_running) session->tick(now);
    if (session->status() == vyne::ModeStatus::ready && !restore_at) {
        restore_at = now + (exporting ? 30000 : 5000);
        vyne::RenderDimensions measured{}; dimensions(nullptr, measured);
        probe_phase.store(exporting ? 6 : 3);
        note("measured_backbuffer=" + std::to_string(measured.width) + "x" + std::to_string(measured.height) +
            (exporting ? " stable_frames=3 waiting_for_native_export_on_UI_thread" : " stable_frames=3 scene_target=UNVERIFIED restore_in_ms=5000"));
    }
    if (restore_at && now >= restore_at && !export_started.load()) session->finish(now);
    if (session->status() == vyne::ModeStatus::restoring) probe_phase.store(4);
    if (session->status() != prior) note("state=" + std::to_string(static_cast<int>(session->status())) + " " + session->error());
    if (session->status() == vyne::ModeStatus::restored) {
        if (exporting) export_result.store(export_started.load() && !export_failed.load() && vyne::resolution_gpu_verified() ? 1 : -1);
        note(exporting ? "export_session_restore_verified=original_queue_and_backbuffer" : "restore_verified=original_queue_and_backbuffer (not a completed export)");
        std::ofstream recovery(directory / L"mode-recovery.txt", std::ios::trunc);
        recovery << "restored\n";
        session.reset(); restore_at = 0;
        probe_phase.store(5); probe_busy.store(false);
        export_mode.store(false); export_started.store(false); export_dimensions.store(0);
        export_deadline = 0; finalize_deadline = 0; vyne::reset_resolution_gpu_observation();
        vyne::set_resolution_window_target(0, 0); next_mode_observation = 0;
    } else if (session->status() == vyne::ModeStatus::restore_failed) {
        if (exporting) export_result.store(-1);
        // Keep the session and recovery record. Never silently re-arm after
        // failure; user must inspect settings and restart the diagnostic.
        restore_at = 0;
        probe_phase.store(-2);
    } else if (session->status() == vyne::ModeStatus::failed) {
        if (exporting) export_result.store(-1);
        note(session->error()); session.reset(); probe_phase.store(-1); probe_busy.store(false);
        export_mode.store(false); export_dimensions.store(0); export_started.store(false);
        vyne::set_resolution_window_target(0, 0); next_mode_observation = 0;
    }
}

HRESULT STDMETHODCALLTYPE on_present(IDXGISwapChain* chain, UINT sync, UINT flags) {
    if (chain == game_chain && !(flags & DXGI_PRESENT_TEST)) {
        try { probe_tick(); }
        catch (...) { OutputDebugStringW(L"VYNE: mode probe exception; inspect recovery record.\n"); }
    }
    return original_present.load()(chain, sync, flags);
}

bool bind_build3258(uintptr_t base) {
    // Derived by independently reading the local, mapped build3258 body. The
    // first five bytes are FiveM's existing jump; validate an untouched body.
    const unsigned char guard[] = {0x8b,0x43,0x04,0x89,0x05,0x4a,0x0e,0xae,0x00,
        0x8b,0x43,0x08,0x89,0x05,0x45,0x0e,0xae,0x00};
    const auto body = reinterpret_cast<const void*>(base + 0x13a0211);
    if (!readable(body, sizeof(guard)) || std::memcmp(body, guard, sizeof(guard))) return false;
    const auto reference = reinterpret_cast<const unsigned char*>(base + 0x13a02dd);
    if (!readable(reference, 7) || reference[0] != 0x4c || reference[1] != 0x8b || reference[2] != 0x3d) return false;
    std::int32_t displacement{}; std::memcpy(&displacement, reference + 3, 4);
    const auto chain_slot = reinterpret_cast<IDXGISwapChain**>(base + 0x13a02e4 + displacement);
    if (!readable(chain_slot, sizeof(void*)) || !*chain_slot) return false;
    requested_mode = reinterpret_cast<std::uint32_t*>(base + 0x1e81060);
    if (!readable(requested_mode, 24)) return false;
    game_chain = *chain_slot;
    change_mode = reinterpret_cast<ChangeMode>(base + 0x13a01c0);
    const unsigned char playback_guard[] = {0xc6,0x05,0x02,0xf3,0xe7,0x01,0x01,0x89,0x0d,0xd2,0xf2,0xe7,0x01};
    if (!readable(reinterpret_cast<void*>(base + 0x198f05), sizeof(playback_guard)) ||
        std::memcmp(reinterpret_cast<void*>(base + 0x198f05), playback_guard, sizeof(playback_guard))) return false;
    playback_active = reinterpret_cast<bool*>(base + 0x201820e);
    playback_type = reinterpret_cast<int*>(base + 0x20181e4);
    delayed_close = reinterpret_cast<bool*>(base + 0x201820f);
    return true;
}
}

void vyne::start_native_mode_probe(HMODULE module) noexcept {
    try {
        wchar_t path[32768]{};
        if (!GetModuleFileNameW(module, path, 32768)) return;
        directory = std::filesystem::path(path).parent_path() / L"VYNE";
        hotkeys_enabled = GetPrivateProfileIntW(L"Diagnostics", L"EnableModeProbe", 0, (directory / L"VYNE.ini").c_str()) == 1;
        const bool menu_enabled = GetPrivateProfileIntW(L"Diagnostics", L"EnableExportMenu", 1, (directory / L"VYNE.ini").c_str()) == 1;
        if (!hotkeys_enabled && !menu_enabled) return;
        std::filesystem::create_directories(directory);
        wchar_t host[32768]{}; GetModuleFileNameW(nullptr, host, 32768);
        if (std::filesystem::path(host).filename() != L"FiveM_b3258_GTAProcess.exe") {
            note("refused: mode probe supports FiveM Legacy build3258 only"); return;
        }
        if (std::filesystem::exists(directory / L"mode-recovery.txt")) {
            std::ifstream marker(directory / L"mode-recovery.txt"); std::string first;
            std::getline(marker, first);
            if (first != "restored") { note("refused: unresolved previous recovery record"); return; }
        }
        const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        bool bound = false;
        for (unsigned attempt = 0; attempt < 180; ++attempt) {
            if (bind_build3258(base)) { bound = true; break; }
            Sleep(500);
        }
        if (!bound) { note("refused: current-build signatures/swapchain unavailable"); return; }
        // Pin before publishing a callback; this opt-in probe lives until exit.
        HMODULE pinned = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&on_present), &pinned)) return;
        if (!install_native_resolution(base, game_chain, directory)) {
            note("selected-resolution export unavailable: native resolution guards/hooks failed; resolution check remains available");
        }
        if (!readable(game_chain, sizeof(void*))) { note("refused: invalid swapchain object"); return; }
        const auto table = *reinterpret_cast<void***>(game_chain);
        if (!readable(table, 18 * sizeof(void*))) { note("refused: invalid swapchain vtable"); return; }
        auto slot = table + 8; auto original = *slot;
        HMODULE method_owner = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(original), &method_owner) ||
            (method_owner != GetModuleHandleW(L"dxgi.dll") && method_owner != GetModuleHandleW(L"rage-graphics-five.dll"))) {
            note("refused: unrecognized swapchain Present owner"); return;
        }
        original_present.store(reinterpret_cast<Present>(original));
        DWORD old_protection{};
        if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old_protection)) return;
        const auto previous = InterlockedCompareExchangePointer(slot, reinterpret_cast<void*>(&on_present), original);
        DWORD unused{}; VirtualProtect(slot, sizeof(void*), old_protection, &unused);
        if (previous != original) { note("refused: another component changed Present during setup"); return; }
        probe_ready.store(true); probe_phase.store(1);
        note("armed: native VYNE menu can request a resolution check; F7 or focus loss cancels; legacy F6=" + std::to_string(hotkeys_enabled));
    } catch (...) { OutputDebugStringW(L"VYNE: optional mode probe initialization failed.\n"); }
}

namespace {
bool queue_mode(unsigned width, unsigned height, bool exporting) noexcept {
    if (!probe_ready.load() || probe_busy.exchange(true)) return false;
    if (!((width == 1920 && height == 1080) || (width == 2560 && height == 1440) || (width == 3840 && height == 2160))) {
        probe_busy.store(false); return false;
    }
    export_dimensions.store(exporting ? (static_cast<std::uint64_t>(width) << 32) | height : 0);
    export_mode.store(exporting); export_started.store(false); export_failed.store(false);
    export_result.store(0);
    if (exporting) vyne::begin_resolution_gpu_observation();
    probe_phase.store(2);
    queued_dimensions.store((static_cast<std::uint64_t>(width) << 32) | height);
    return true;
}
}
bool vyne::queue_resolution_check(unsigned width, unsigned height) noexcept { return queue_mode(width, height, false); }
bool vyne::queue_resolution_export(unsigned width, unsigned height) noexcept {
    return native_resolution_available() && queue_mode(width, height, true);
}
bool vyne::resolution_export_ready() noexcept { return export_mode.load() && probe_phase.load() == 6 && !export_started.load(); }
bool vyne::claim_resolution_export() noexcept {
    if (!export_mode.load() || export_failed.load()) return false;
    int expected = 6;
    return probe_phase.compare_exchange_strong(expected, 9);
}
bool vyne::resolution_export_started() noexcept {
    if (!export_mode.load() || probe_phase.load() != 9 || export_failed.load()) return false;
    // Called after native TriggerExport on its owning UI thread.
    if (!readable(playback_active, 1) || !*playback_active || !readable(playback_type, 4) || *playback_type != 1) {
        export_failed.store(true); return false;
    }
    export_started.store(true); probe_phase.store(7);
    return true;
}
bool vyne::resolution_export_dimensions(unsigned& width, unsigned& height) noexcept {
    if (!export_mode.load() || !probe_busy.load() || probe_phase.load() < 6 || export_failed.load()) return false;
    const auto packed = export_dimensions.load();
    width = static_cast<unsigned>(packed >> 32); height = static_cast<unsigned>(packed);
    return width && height;
}
void vyne::fail_resolution_export() noexcept { export_failed.store(true); }
int vyne::resolution_export_result() noexcept { return export_result.load(); }
bool vyne::resolution_check_busy() noexcept { return probe_busy.load(); }
const char* vyne::resolution_check_status() noexcept {
    switch (probe_phase.load()) {
    case 1: return "Ready for resolution check";
    case 2: return "Switching resolution";
    case 3: return "Target reached; restoring shortly";
    case 4: return "Restoring original resolution";
    case 5: return "Original resolution restored";
    case 6: return "Resolution ready; starting native export";
    case 7: return "Exporting at selected resolution";
    case 8: return "Closing native export before restoring resolution";
    case 9: return "Starting native export";
    case -1: return "Resolution check refused; see mode-probe.log";
    case -2: return "Restore failed; inspect recovery record";
    default: return "Resolution backend unavailable; see mode-probe.log";
    }
}
