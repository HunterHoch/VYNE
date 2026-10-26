#pragma once
#include <windows.h>
#include <filesystem>
#include <string>

namespace vyne {
enum class ExportRow : unsigned { mode, resolution, fps, bitrate, format, color, directory, count };
struct ExportSettings {
    bool resolution_check = false;
    unsigned resolution = 2;
    std::filesystem::path output;
};
void initialize_export_settings(const std::filesystem::path& directory);
ExportSettings export_settings();
bool resolution_check_enabled() noexcept;
void step_export_setting(ExportRow row, int direction);
std::string export_row_value(ExportRow row);
const char* export_row_label(ExportRow row) noexcept;
const char* export_row_help(ExportRow row) noexcept;
bool export_row_enabled(ExportRow row) noexcept;
unsigned export_width(unsigned index) noexcept;
unsigned export_height(unsigned index) noexcept;
void choose_export_directory(HWND owner);
}
