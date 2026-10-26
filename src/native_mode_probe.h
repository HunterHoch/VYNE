#pragma once
#include <windows.h>
#include <string>
namespace vyne {
void start_native_mode_probe(HMODULE module) noexcept;
void request_native_mode_probe(HMODULE module) noexcept;
bool queue_resolution_check(unsigned width, unsigned height) noexcept;
bool queue_resolution_export(unsigned width, unsigned height) noexcept;
bool resolution_export_ready() noexcept;
bool claim_resolution_export() noexcept;
bool resolution_export_started() noexcept;
bool resolution_export_dimensions(unsigned& width, unsigned& height) noexcept;
void fail_resolution_export() noexcept;
int resolution_export_result() noexcept;
bool resolution_check_busy() noexcept;
const char* resolution_check_status() noexcept;
}
