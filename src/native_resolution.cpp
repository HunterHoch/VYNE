#include "native_resolution.h"
#include "native_mode_probe.h"
#include "hook_retry.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <atomic>
#include <array>
#include <mutex>
#include <fstream>
#include <cstring>
#include <intrin.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfapi.h>

namespace {
using Microsoft::WRL::ComPtr;
using Limit = void(*)(unsigned*, unsigned*, unsigned, unsigned);
using ExportTexture = unsigned char(*)(void*, unsigned, unsigned, unsigned);
using CreateTexture = HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*, const D3D11_TEXTURE2D_DESC*, const D3D11_SUBRESOURCE_DATA*, ID3D11Texture2D**);
using Draw = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT);
using DrawIndexed = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, INT);
Limit original_limit = nullptr;
ExportTexture original_export_texture = nullptr;
CreateTexture original_create_texture = nullptr;
Draw original_draw = nullptr;
DrawIndexed original_draw_indexed = nullptr;
std::uintptr_t image_base = 0;
std::filesystem::path log_directory;
std::atomic_bool installed{false};
std::atomic_int verification{0}; // 0 awaiting source, 1 observed source, -1 failed.
std::atomic_int encoder_result{0}; // Finalize succeeds (1), failure/cancel (-1).
std::atomic_uint64_t encoder_epoch{0};
std::atomic_uint64_t window_target{0};
using FitWindow = bool(*)(RECT*, DWORD);
FitWindow original_fit_window = nullptr;
using CreateWriter = HRESULT(WINAPI*)(LPCWSTR, IMFByteStream*, IMFAttributes*, IMFSinkWriter**);
CreateWriter original_create_writer = nullptr;
std::mutex texture_mutex;
ComPtr<ID3D11Texture2D> encode_texture;
thread_local bool constructing_export_texture = false;

void note(const std::string& message) {
    std::ofstream log(log_directory / L"resolution-export.log", std::ios::app);
    log << "tick=" << GetTickCount64() << ' ' << message << '\n';
}
bool matches(std::uintptr_t address, const char* bytes, size_t length) {
    MEMORY_BASIC_INFORMATION region{};
    return VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region)) && region.State == MEM_COMMIT &&
        !(region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
        address + length <= reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize &&
        std::memcmp(reinterpret_cast<void*>(address), bytes, length) == 0;
}
void fail(const char* reason) {
    if (verification.exchange(-1) != -1) note(reason);
    vyne::fail_resolution_export();
}
class ObservedWriter final : public IMFSinkWriter {
    std::atomic<ULONG> references_{1};
    IMFSinkWriter* writer_;
    unsigned width_, height_;
    const std::uint64_t epoch_ = encoder_epoch.load();
    std::atomic_bool writing_{false};
    void fail_writer(const char* reason) {
        if (epoch_ == encoder_epoch.load()) { encoder_result.store(-1); fail(reason); }
    }
    bool check_video(IMFMediaType* type) {
        GUID major{};
        if (!type || FAILED(type->GetGUID(MF_MT_MAJOR_TYPE, &major))) return false;
        if (major != MFMediaType_Video) return true;
        UINT32 width = 0, height = 0;
        if (FAILED(MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &width, &height)) || width != width_ || height != height_) {
            fail_writer("refused: Media Foundation video dimensions differ from selected mode"); return false;
        }
        note("encoder video type=" + std::to_string(width) + "x" + std::to_string(height)); return true;
    }
public:
    ObservedWriter(IMFSinkWriter* writer, unsigned width, unsigned height) : writer_(writer), width_(width), height_(height) {}
    ~ObservedWriter() { writer_->Release(); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_POINTER;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IMFSinkWriter)) {
            *object = static_cast<IMFSinkWriter*>(this); AddRef(); return S_OK;
        }
        return writer_->QueryInterface(iid, object);
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references_; }
    ULONG STDMETHODCALLTYPE Release() override { const auto count = --references_; if (!count) delete this; return count; }
    HRESULT STDMETHODCALLTYPE AddStream(IMFMediaType* type, DWORD* index) override {
        return check_video(type) ? writer_->AddStream(type, index) : E_INVALIDARG;
    }
    HRESULT STDMETHODCALLTYPE SetInputMediaType(DWORD index, IMFMediaType* type, IMFAttributes* attributes) override {
        return check_video(type) ? writer_->SetInputMediaType(index, type, attributes) : E_INVALIDARG;
    }
    HRESULT STDMETHODCALLTYPE BeginWriting() override {
        const auto result = writer_->BeginWriting(); writing_ = SUCCEEDED(result);
        if (FAILED(result)) { note("BeginWriting HRESULT=" + std::to_string(result)); fail_writer("native encoder BeginWriting failed"); }
        return result;
    }
    HRESULT STDMETHODCALLTYPE WriteSample(DWORD index, IMFSample* sample) override {
        const auto result = writer_->WriteSample(index, sample);
        if (FAILED(result)) { note("WriteSample HRESULT=" + std::to_string(result)); fail_writer("native encoder WriteSample failed"); }
        return result;
    }
    HRESULT STDMETHODCALLTYPE SendStreamTick(DWORD index, LONGLONG time) override { return writer_->SendStreamTick(index, time); }
    HRESULT STDMETHODCALLTYPE PlaceMarker(DWORD index, void* context) override { return writer_->PlaceMarker(index, context); }
    HRESULT STDMETHODCALLTYPE NotifyEndOfSegment(DWORD index) override { return writer_->NotifyEndOfSegment(index); }
    HRESULT STDMETHODCALLTYPE Flush(DWORD index) override {
        if (writing_.load()) fail_writer("native encoder flushed/cancelled; output will not be delivered");
        return writer_->Flush(index);
    }
    HRESULT STDMETHODCALLTYPE Finalize() override {
        const auto result = writer_->Finalize(); writing_ = false;
        if (FAILED(result)) { note("Finalize HRESULT=" + std::to_string(result)); fail_writer("native encoder Finalize failed"); }
        else if (epoch_ == encoder_epoch.load() && encoder_result.load() != -1) { encoder_result.store(1); note("native encoder finalized successfully; original project audio and timestamps preserved"); }
        return result;
    }
    HRESULT STDMETHODCALLTYPE GetServiceForStream(DWORD index, REFGUID service, REFIID iid, void** object) override {
        return writer_->GetServiceForStream(index, service, iid, object);
    }
    HRESULT STDMETHODCALLTYPE GetStatistics(DWORD index, MF_SINK_WRITER_STATISTICS* stats) override { return writer_->GetStatistics(index, stats); }
};
HRESULT WINAPI create_writer(LPCWSTR url, IMFByteStream* stream, IMFAttributes* attributes, IMFSinkWriter** output) {
    const auto result = original_create_writer(url, stream, attributes, output);
    unsigned width = 0, height = 0;
    if (SUCCEEDED(result) && output && *output && vyne::resolution_export_dimensions(width, height)) {
        try { *output = new ObservedWriter(*output, width, height); note("observing selected-resolution native encoder lifecycle"); }
        catch (...) { encoder_result.store(-1); fail("could not observe native encoder"); }
    } else if (FAILED(result) && vyne::resolution_export_dimensions(width, height)) {
        encoder_result.store(-1); fail("native MFCreateSinkWriterFromURL failed");
    }
    return result;
}
void limit(unsigned* width, unsigned* height, unsigned source_width, unsigned source_height) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - image_base;
    original_limit(width, height, source_width, source_height);
    unsigned requested_width = 0, requested_height = 0;
    // These are all four independently identified callers: native capture size,
    // encoder config defaults, encoder setup and encoder config reset.
    if ((caller == 0x324b49 || caller == 0x170323a || caller == 0x1703599 || caller == 0x1706e5c) &&
        width && height && vyne::resolution_export_dimensions(requested_width, requested_height)) {
        *width = requested_width; *height = requested_height;
    }
}
bool fit_window(RECT* rectangle, DWORD style) {
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - image_base;
    const auto result = original_fit_window(rectangle, style);
    const auto target = window_target.load();
    // Native mode request and its frame-boundary resize validation both use
    // this monitor-fitting routine. Preserve its normal result for every
    // other caller and outside the temporary VYNE mode transaction.
    if (target && rectangle && (caller == 0x13a0398 || caller == 0x13a0865)) {
        RECT desired{0, 0, static_cast<LONG>(target >> 32), static_cast<LONG>(target)};
        if (AdjustWindowRectEx(&desired, style, FALSE, 0)) {
            rectangle->right = rectangle->left + desired.right - desired.left;
            rectangle->bottom = rectangle->top + desired.bottom - desired.top;
        } else fail("could not calculate selected-resolution client rectangle");
    }
    return result;
}
unsigned char export_texture(void* context, unsigned width, unsigned height, unsigned format) {
    unsigned requested_width = 0, requested_height = 0;
    const bool active = vyne::resolution_export_dimensions(requested_width, requested_height);
    if (active && (width != requested_width || height != requested_height)) {
        fail("refused: native VideoEncode dimensions differ from selected mode"); return 0;
    }
    constructing_export_texture = active;
    const auto result = original_export_texture(context, width, height, format);
    constructing_export_texture = false;
    if (active && !result) fail("native VideoEncode allocation failed");
    return result;
}
HRESULT STDMETHODCALLTYPE create_texture(ID3D11Device* device, const D3D11_TEXTURE2D_DESC* descriptor,
    const D3D11_SUBRESOURCE_DATA* initial, ID3D11Texture2D** output) {
    const auto result = original_create_texture(device, descriptor, initial, output);
    if (constructing_export_texture && SUCCEEDED(result) && output && *output && descriptor &&
        (descriptor->BindFlags & D3D11_BIND_RENDER_TARGET)) {
        D3D11_TEXTURE2D_DESC actual{}; (*output)->GetDesc(&actual);
        unsigned width = 0, height = 0;
        if (vyne::resolution_export_dimensions(width, height) && actual.Width == width && actual.Height == height) {
            std::lock_guard guard(texture_mutex); encode_texture = *output;
            note("GPU VideoEncode=" + std::to_string(actual.Width) + "x" + std::to_string(actual.Height) + " format=" + std::to_string(actual.Format));
        } else fail("refused: actual GPU VideoEncode dimensions mismatch");
    }
    return result;
}
void inspect_source(ID3D11DeviceContext* context) {
    unsigned width = 0, height = 0;
    if (verification.load() != 0 || !vyne::resolution_export_dimensions(width, height)) return;
    ComPtr<ID3D11Texture2D> target;
    { std::lock_guard guard(texture_mutex); target = encode_texture; }
    if (!target) return;
    ComPtr<ID3D11RenderTargetView> view;
    context->OMGetRenderTargets(1, view.GetAddressOf(), nullptr);
    if (!view) return;
    ComPtr<ID3D11Resource> resource; view->GetResource(resource.GetAddressOf());
    if (resource.Get() != target.Get()) return;
    D3D11_VIEWPORT viewport{}; UINT count = 1;
    context->RSGetViewports(&count, &viewport);
    if (count != 1 || viewport.Width != static_cast<float>(width) || viewport.Height != static_cast<float>(height)) {
        fail("refused: first VideoEncode draw viewport does not match selected dimensions"); return;
    }
    std::array<ID3D11ShaderResourceView*, 16> sources{};
    context->PSGetShaderResources(0, static_cast<UINT>(sources.size()), sources.data());
    bool full_resolution_source = false;
    for (auto source : sources) {
        if (!source) continue;
        ComPtr<ID3D11Resource> source_resource; source->GetResource(source_resource.GetAddressOf());
        ComPtr<ID3D11Texture2D> texture;
        if (SUCCEEDED(source_resource.As(&texture))) {
            D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc);
            if (desc.Width == width && desc.Height == height &&
                (desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
                 desc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS || desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
                 desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || desc.Format == DXGI_FORMAT_R10G10B10A2_UNORM ||
                 desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)) full_resolution_source = true;
        }
        source->Release();
    }
    if (!full_resolution_source) { fail("refused: first VideoEncode draw has no full-resolution color source"); return; }
    verification.store(1);
    note("GPU scene_source=" + std::to_string(width) + "x" + std::to_string(height) + " capture_target=matching viewport=matching; native audio/timebase preserved");
}
void STDMETHODCALLTYPE draw(ID3D11DeviceContext* context, UINT count, UINT first) {
    try { inspect_source(context); } catch (...) { fail("GPU source verification failed"); }
    original_draw(context, count, first);
}
void STDMETHODCALLTYPE draw_indexed(ID3D11DeviceContext* context, UINT count, UINT first, INT base) {
    try { inspect_source(context); } catch (...) { fail("GPU source verification failed"); }
    original_draw_indexed(context, count, first, base);
}
}

bool vyne::install_native_resolution(std::uintptr_t base, IDXGISwapChain* chain, const std::filesystem::path& logs) {
    image_base = base; log_directory = logs;
    const auto initialized = MH_Initialize();
    if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED) return false;
    if (!matches(base + 0x31e3dc, "\x48\x89\x5c\x24\x08\x57\x48\x83\xec\x30\x41\xba\x80\x02\x00\x00", 16) ||
        !matches(base + 0x31e3f7, "\xb8\x80\x07\x00\x00", 5) ||
        !matches(base + 0x31e410, "\xb9\x38\x04\x00\x00", 5) ||
        !matches(base + 0x325594, "\x40\x53\x48\x81\xec\xa0\x00\x00\x00\x89\x11\x48\x8b\xd9", 14) ||
        !matches(base + 0x1395a74, "\x48\x89\x5c\x24\x08\x48\x89\x74\x24\x10\x55\x57\x41\x56", 14) ||
        !matches(base + 0x1395b7c, "\x3b\xf8\x7e\x13", 4) ||
        !matches(base + 0x1395b93, "\x3b\xf1\x7e\x15", 4)) return false;
    ComPtr<ID3D11Device> device;
    note("graphics setup: GetDevice");
    if (FAILED(chain->GetDevice(IID_PPV_ARGS(&device)))) return false;
    note("graphics setup: GetImmediateContext");
    ComPtr<ID3D11DeviceContext> context; device->GetImmediateContext(context.GetAddressOf());
    const auto device_table = *reinterpret_cast<void***>(device.Get());
    const auto context_table = *reinterpret_cast<void***>(context.Get());
    const auto media = LoadLibraryW(L"mfreadwrite.dll");
    note("graphics setup: Media Foundation library acquired");
    const auto create_media_writer = media ? reinterpret_cast<void*>(GetProcAddress(media, "MFCreateSinkWriterFromURL")) : nullptr;
    if (!create_media_writer) return false;
    struct Hook { void* target; void* replacement; void** original; };
    const std::array<Hook, 7> hooks{{
        {reinterpret_cast<void*>(base + 0x31e3dc), reinterpret_cast<void*>(&limit), reinterpret_cast<void**>(&original_limit)},
        {reinterpret_cast<void*>(base + 0x325594), reinterpret_cast<void*>(&export_texture), reinterpret_cast<void**>(&original_export_texture)},
        {device_table[5], reinterpret_cast<void*>(&create_texture), reinterpret_cast<void**>(&original_create_texture)},
        {context_table[13], reinterpret_cast<void*>(&draw), reinterpret_cast<void**>(&original_draw)},
        {context_table[12], reinterpret_cast<void*>(&draw_indexed), reinterpret_cast<void**>(&original_draw_indexed)},
        {create_media_writer, reinterpret_cast<void*>(&create_writer), reinterpret_cast<void**>(&original_create_writer)},
        {reinterpret_cast<void*>(base + 0x1395a74), reinterpret_cast<void*>(&fit_window), reinterpret_cast<void**>(&original_fit_window)}
    }};
    unsigned created = 0;
    note("graphics setup: creating hook batch");
    for (; created < hooks.size(); ++created) {
        const auto result = MH_CreateHook(hooks[created].target, hooks[created].replacement, hooks[created].original);
        if (result != MH_OK) { note("hook creation failed index=" + std::to_string(created) + " " + MH_StatusToString(result)); break; }
    }
    if (created == hooks.size()) {
        bool queued = true;
        for (const auto& hook : hooks) if (MH_QueueEnableHook(hook.target) != MH_OK) queued = false;
        unsigned attempts = 0;
        if (queued) {
            note("graphics setup: enabling hook batch");
            const auto result = retry_hook_snapshot([] { return MH_ApplyQueued(); }, [](unsigned delay) { Sleep(delay); }, attempts);
            if (result == MH_OK) {
                installed.store(true); note("selected-resolution export hooks enabled; 1080p clamp is overridden only inside a VYNE session"); return true;
            }
            note("hook activation failed " + std::string(MH_StatusToString(result)) + " attempts=" + std::to_string(attempts));
        }
    }
    for (unsigned i = 0; i < created; ++i) {
        unsigned attempts = 0;
        const auto disabled = retry_hook_snapshot([&] { return MH_DisableHook(hooks[i].target); }, [](unsigned delay) { Sleep(delay); }, attempts);
        if (disabled == MH_OK || disabled == MH_ERROR_DISABLED) MH_RemoveHook(hooks[i].target);
    }
    note("selected-resolution hook installation failed; native export remains stock");
    return false;
}
bool vyne::native_resolution_available() noexcept { return installed.load(); }
void vyne::reset_resolution_gpu_observation() noexcept { std::lock_guard guard(texture_mutex); encode_texture.Reset(); }
void vyne::begin_resolution_gpu_observation() noexcept { reset_resolution_gpu_observation(); ++encoder_epoch; verification.store(0); encoder_result.store(0); }
bool vyne::resolution_gpu_verified() noexcept { return verification.load() == 1; }
int vyne::resolution_encoder_result() noexcept { return encoder_result.load(); }
void vyne::set_resolution_window_target(unsigned width, unsigned height) noexcept {
    window_target.store(width && height ? (static_cast<std::uint64_t>(width) << 32) | height : 0);
}
