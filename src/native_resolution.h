#pragma once
#include <windows.h>
#include <dxgi.h>
#include <filesystem>
#include <cstdint>
namespace vyne {
bool install_native_resolution(std::uintptr_t base, IDXGISwapChain* chain, const std::filesystem::path& logs);
bool native_resolution_available() noexcept;
void reset_resolution_gpu_observation() noexcept;
void begin_resolution_gpu_observation() noexcept;
bool resolution_gpu_verified() noexcept;
int resolution_encoder_result() noexcept;
}
