#pragma once
#include <filesystem>
#include <string>
namespace vyne {
bool finalized_native_mp4(const std::filesystem::path& source) noexcept;
bool copy_native_mp4(const std::filesystem::path& source, const std::filesystem::path& directory,
                     std::filesystem::path& delivered) noexcept;
void watch_native_export(const std::filesystem::path& output_directory);
std::string native_output_status();
}
