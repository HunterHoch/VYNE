#include "native_mode_probe.h"
#include "graphics_mode.h"
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
    change_mode(native.data());
    return readable(requested_mode, 24) && std::memcmp(requested_mode, native.data(), 24) == 0;
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
    const bool pressed = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    DXGI_SWAP_CHAIN_DESC desc{};
    const bool foreground = SUCCEEDED(game_chain->GetDesc(&desc)) &&
        GetForegroundWindow() == desc.OutputWindow;
    if (pressed && !key_down && foreground && !session) {
        vyne::GraphicsState before{};
        if (!snapshot(nullptr, before) || !write_recovery(before)) {
            note("refused: stable windowed mode/recovery snapshot unavailable");
        } else {
            auto target = before; target.width = 3840; target.height = 2160;
            target.native_state[0] = target.width; target.native_state[1] = target.height;
            session = std::make_unique<vyne::GraphicsModeSession>(
                vyne::GraphicsBackend{nullptr, snapshot, request, dimensions, true});
            note("requested=3840x2160 original=" + std::to_string(before.width) + "x" + std::to_string(before.height));
            session->begin(target, now);
            if (!session->error().empty()) note(session->error());
        }
    }
    key_down = pressed;
    if (!session) return;
    const auto prior = session->status();
    if ((GetAsyncKeyState(VK_F7) & 0x8000) || !foreground) session->cancel(now);
    session->tick(now);
    if (session->status() == vyne::ModeStatus::ready && !restore_at) {
        restore_at = now + 5000;
        note("measured_backbuffer=3840x2160 stable_frames=3 scene_target=UNVERIFIED restore_in_ms=5000");
    }
    if (restore_at && now >= restore_at) session->finish(now);
    if (session->status() != prior) note("state=" + std::to_string(static_cast<int>(session->status())) + " " + session->error());
    if (session->status() == vyne::ModeStatus::restored) {
        note("restore_verified=original_queue_and_backbuffer (not a completed export)");
        std::ofstream recovery(directory / L"mode-recovery.txt", std::ios::trunc);
        recovery << "restored\n";
        session.reset(); restore_at = 0;
    } else if (session->status() == vyne::ModeStatus::restore_failed) {
        // Keep the session and recovery record. Never silently re-arm after
        // failure; user must inspect settings and restart the diagnostic.
        restore_at = 0;
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
    return true;
}
}

void vyne::start_native_mode_probe(HMODULE module) noexcept {
    try {
        wchar_t path[32768]{};
        if (!GetModuleFileNameW(module, path, 32768)) return;
        directory = std::filesystem::path(path).parent_path() / L"VYNE";
        if (GetPrivateProfileIntW(L"Diagnostics", L"EnableModeProbe", 0, (directory / L"VYNE.ini").c_str()) != 1) return;
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
        note("armed: F6 requests 4K for five seconds; F7 or focus loss cancels; native export untouched");
    } catch (...) { OutputDebugStringW(L"VYNE: optional mode probe initialization failed.\n"); }
}
