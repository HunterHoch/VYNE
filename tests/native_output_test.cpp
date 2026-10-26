#include "../src/native_output.h"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <vector>

void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void box(std::ofstream& file, const char* name, unsigned payload) {
    const unsigned size = payload + 8;
    const unsigned char header[] = {static_cast<unsigned char>(size >> 24), static_cast<unsigned char>(size >> 16), static_cast<unsigned char>(size >> 8), static_cast<unsigned char>(size)};
    file.write(reinterpret_cast<const char*>(header), 4); file.write(name, 4);
    for (unsigned i = 0; i < payload; ++i) file.put(0);
}
std::vector<char> bytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
int wmain(int argc, wchar_t** argv) {
    const auto root = std::filesystem::absolute("build") / (L"output-test-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    const auto source = root / L"native.mp4";
    const auto destination = root / L"output folder é 影像";
    {
        std::ofstream file(source, std::ios::binary); box(file, "ftyp", 8); box(file, "mdat", 8);
    }
    require(!vyne::finalized_native_mp4(source), "unfinished MP4 without movie metadata rejected");
    {
        std::ofstream file(source, std::ios::binary | std::ios::app); box(file, "moov", 8);
    }
    require(vyne::finalized_native_mp4(source), "closed finalized container accepted");
    const auto writer = CreateFileW(source.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(writer != INVALID_HANDLE_VALUE, "test writer opened");
    require(!vyne::finalized_native_mp4(source), "an active writer prevents premature delivery");
    CloseHandle(writer);
    std::filesystem::path delivered;
    require(vyne::copy_native_mp4(source, destination, delivered), "completed native output delivered to Unicode folder");
    require(bytes(source) == bytes(delivered), "delivered file is byte-for-byte native output");
    const auto first = delivered;
    require(vyne::copy_native_mp4(source, destination, delivered) && first != delivered, "existing destination never overwritten");
    require(bytes(source) == bytes(first) && bytes(source) == bytes(delivered), "both existing and numbered output remain intact");
    require(vyne::copy_native_mp4(source, root, delivered) && delivered == source, "selecting native folder does not duplicate or overwrite source");
    if (argc > 1) {
        const auto actual = std::filesystem::path(argv[1]);
        require(vyne::copy_native_mp4(actual, destination, delivered), "real native export delivered");
        require(bytes(actual) == bytes(delivered), "real native export preserved exactly");
        require(!vyne::copy_native_mp4(actual, destination, delivered, 3840, 2160), "1080p fixture cannot be delivered as a 4K export");
        require(vyne::copy_native_mp4(actual, destination, delivered, 1920, 1080), "matching native dimensions accepted from MP4 track header");
    }
    std::cout << "PASS: unfinished/writing-file rejection, finalized output delivery, Unicode folder and collision-safe byte preservation.\n";
}
