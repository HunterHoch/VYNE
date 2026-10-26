#pragma once
#include <filesystem>
#include <string>
namespace vyne {
bool finalized_native_mp4(const std::filesystem::path& source) noexcept;
bool copy_native_mp4(const std::filesystem::path& source, const std::filesystem::path& directory,
                     std::filesystem::path& delivered, unsigned expected_width = 0, unsigned expected_height = 0) noexcept;
void watch_native_export(const std::filesystem::path& output_directory, unsigned expected_width = 0,
    unsigned expected_height = 0, int (*verified)() noexcept = nullptr);
std::string native_output_status();
}
