#include "native_output.h"
#include <windows.h>
#include <shlobj.h>
#include <atomic>
#include <mutex>
#include <map>
#include <memory>
#include <fstream>
#include <array>

namespace {
std::atomic_uint64_t generation{0}, temporary_id{0};
std::mutex status_mutex;
std::string status = "Finished native exports will be copied to the chosen output location.";
void set_status(std::string value) { std::lock_guard guard(status_mutex); status = std::move(value); }
struct File {
    HANDLE handle = INVALID_HANDLE_VALUE;
    explicit File(HANDLE value) : handle(value) {}
    ~File() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    explicit operator bool() const { return handle != INVALID_HANDLE_VALUE; }
};
bool read_at(HANDLE file, std::uint64_t offset, void* output, DWORD size) {
    LARGE_INTEGER position{}; position.QuadPart = static_cast<LONGLONG>(offset);
    DWORD read = 0;
    return SetFilePointerEx(file, position, nullptr, FILE_BEGIN) && ReadFile(file, output, size, &read, nullptr) && read == size;
}
std::uint64_t big_endian(const unsigned char* bytes, unsigned count) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < count; ++i) value = (value << 8) | bytes[i];
    return value;
}
bool complete_mp4(HANDLE file) {
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file, &length) || length.QuadPart < 32) return false;
    const auto total = static_cast<std::uint64_t>(length.QuadPart);
    std::uint64_t offset = 0;
    bool ftyp = false, moov = false, mdat = false;
    while (offset < total) {
        std::array<unsigned char, 16> box{};
        if (total - offset < 8 || !read_at(file, offset, box.data(), 8)) return false;
        auto size = big_endian(box.data(), 4);
        unsigned header = 8;
        if (size == 1) {
            if (total - offset < 16 || !read_at(file, offset + 8, box.data() + 8, 8)) return false;
            size = big_endian(box.data() + 8, 8); header = 16;
        } else if (!size) size = total - offset;
        if (size < header || size > total - offset) return false;
        const auto type = big_endian(box.data() + 4, 4);
        if (type == 0x66747970 && size >= 16) ftyp = true;
        if (type == 0x6d6f6f76 && size > header) moov = true;
        if (type == 0x6d646174 && size > header) mdat = true;
        offset += size;
    }
    return offset == total && ftyp && moov && mdat;
}
struct Stamp {
    std::uintmax_t size;
    std::filesystem::file_time_type written;
    bool operator==(const Stamp& other) const { return size == other.size && written == other.written; }
};
using Listing = std::map<std::filesystem::path, Stamp>;
Listing list(const std::filesystem::path& directory) {
    Listing files;
    std::error_code error;
    for (std::filesystem::directory_iterator i(directory, error), end; !error && i != end; i.increment(error)) {
        if (!i->is_regular_file(error)) continue;
        if (_wcsicmp(i->path().extension().c_str(), L".mp4")) continue;
        const auto size = i->file_size(error); if (error) break;
        const auto written = i->last_write_time(error); if (error) break;
        files.emplace(i->path(), Stamp{size, written});
    }
    return files;
}
struct Watch {
    std::uint64_t id;
    std::filesystem::path source, output;
    Listing before;
};
DWORD WINAPI watch_thread(void* raw) {
    std::unique_ptr<Watch> watch(static_cast<Watch*>(raw));
    try {
        Listing previous;
        const auto deadline = GetTickCount64() + 30ull * 60 * 1000;
        while (generation.load() == watch->id && GetTickCount64() < deadline) {
            const auto current = list(watch->source);
            for (const auto& [path, stamp] : current) {
                const auto baseline = watch->before.find(path);
                if (baseline != watch->before.end() && baseline->second == stamp) continue;
                const auto prior = previous.find(path);
                if (prior == previous.end() || !(prior->second == stamp)) continue;
                std::filesystem::path delivered;
                if (vyne::copy_native_mp4(path, watch->output, delivered)) {
                    set_status("Saved: " + delivered.u8string());
                    const auto receipt = watch->output / L"VYNE-last-export.txt";
                    const auto text = L"\xfeffSource=" + path.wstring() + L"\r\nOutput=" + delivered.wstring() + L"\r\nBackend=Rockstar native export\r\n";
                    File file(CreateFileW(receipt.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
                    if (file) { DWORD written = 0; WriteFile(file.handle, text.data(), static_cast<DWORD>(text.size() * sizeof(wchar_t)), &written, nullptr); }
                    return 0;
                }
            }
            previous = current;
            Sleep(1000);
        }
        if (generation.load() == watch->id) set_status("No finished native MP4 found; export may have been cancelled. Original files were left in Rockstar's Video Gallery.");
    } catch (...) { if (generation.load() == watch->id) set_status("Could not deliver native output; check output location and Rockstar's Video Gallery."); }
    return 0;
}
}

bool vyne::finalized_native_mp4(const std::filesystem::path& source) noexcept {
    File file(CreateFileW(source.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    return file && complete_mp4(file.handle);
}

bool vyne::copy_native_mp4(const std::filesystem::path& source, const std::filesystem::path& directory,
                          std::filesystem::path& delivered) noexcept {
    try {
        File input(CreateFileW(source.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!input || !complete_mp4(input.handle)) return false;
        std::error_code error;
        std::filesystem::create_directories(directory, error); if (error) return false;
        if (std::filesystem::equivalent(source.parent_path(), directory, error) && !error) { delivered = source; return true; }
        const auto temporary = directory / (source.filename().wstring() + L".vyne-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++temporary_id) + L".tmp");
        bool copied = true;
        {
            File output(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (!output) return false;
            LARGE_INTEGER zero{};
            copied = SetFilePointerEx(input.handle, zero, nullptr, FILE_BEGIN) != FALSE;
            std::array<unsigned char, 256 * 1024> buffer{};
            DWORD read = 0;
            while (copied) {
                if (!ReadFile(input.handle, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) { copied = false; break; }
                if (!read) break;
                DWORD written = 0;
                if (!WriteFile(output.handle, buffer.data(), read, &written, nullptr) || written != read) copied = false;
            }
            if (copied) copied = FlushFileBuffers(output.handle) != FALSE;
        }
        if (copied) {
            for (unsigned suffix = 0; suffix < 1000; ++suffix) {
                auto target = directory / source.filename();
                if (suffix) target = directory / (source.stem().wstring() + L" (" + std::to_wstring(suffix) + L")" + source.extension().wstring());
                if (MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH)) { delivered = target; return true; }
                if (GetLastError() != ERROR_ALREADY_EXISTS && GetLastError() != ERROR_FILE_EXISTS) break;
            }
        }
        DeleteFileW(temporary.c_str()); // Only the temporary file created by this call.
    } catch (...) { return false; }
    return false;
}

void vyne::watch_native_export(const std::filesystem::path& output_directory) {
    PWSTR local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) return;
    const auto source = std::filesystem::path(local) / L"Rockstar Games/GTA V/videos/rendered";
    CoTaskMemFree(local);
    auto watch = std::make_unique<Watch>(Watch{++generation, source, output_directory, list(source)});
    set_status("Waiting for Rockstar's finished MP4; the gallery original will be preserved.");
    const auto thread = CreateThread(nullptr, 0, watch_thread, watch.get(), 0, nullptr);
    if (thread) { watch.release(); CloseHandle(thread); }
    else set_status("Could not start output delivery; the native export remains in Rockstar's Video Gallery.");
}
std::string vyne::native_output_status() { std::lock_guard guard(status_mutex); return status; }
