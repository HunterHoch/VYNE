#pragma once
#include <windows.h>
#include <string>
namespace vyne {
void start_native_mode_probe(HMODULE module) noexcept;
bool queue_resolution_check(unsigned width, unsigned height) noexcept;
bool resolution_check_busy() noexcept;
const char* resolution_check_status() noexcept;
}
