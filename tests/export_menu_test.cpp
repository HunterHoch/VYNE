#include "../src/export_menu_layout.h"
#include "../src/export_settings.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <array>

using namespace vyne;
void require(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
int main() {
    std::array<NativeExportOption, 3> stock{};
    stock[0] = {1,2,3,4,5,6,100,8,9,10};
    stock[1] = {11,12,13,14,15,16,200,18,19,20};
    stock[2] = {21,22,23,24,25,26,0,28,29,30};
    const auto result = compose_export_options(stock.data(), 3);
    require(result.size() == 10, "seven native settings inserted");
    require(!std::memcmp(result.data(), stock.data(), 2 * sizeof(stock[0])), "original setting rows preserved");
    require(!std::memcmp(&result.back(), &stock[2], sizeof(stock[2])), "native Export action remains last and unchanged");
    for (unsigned row = 0; row < 7; ++row) {
        const auto& value = result[row + 2];
        require(value.text == export_option_key(row) && value.toggle == value.text && value.unique == value.text, "label and native input identify the same row");
        require(value.block == stock[1].block && value.link == stock[1].link, "native row group and help panel retained");
        require(!value.reserved && !value.jump && !value.context && !value.action && !value.warning, "new rows cannot inherit stock actions/navigation/warnings");
    }
    require(compose_export_options(result.data(), static_cast<unsigned>(result.size())).empty(), "reopening/rebuilding cannot double-inject");
    std::array<NativeExportOption, 10> full{}; full[0].toggle = 100;
    require(compose_export_options(full.data(), 10).empty(), "reject menu overflow before allocation/mutation");
    require(compose_export_options(nullptr, 1).empty(), "invalid native array refused");
    require(export_row_enabled(ExportRow::mode) && export_row_enabled(ExportRow::resolution) && export_row_enabled(ExportRow::directory), "implemented controls enabled");
    require(!export_row_enabled(ExportRow::format) && !export_row_enabled(ExportRow::color), "unimplemented formats/color cannot be offered as working");
    const auto directory = std::filesystem::absolute("build") / (L"settings-test-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(directory);
    const std::wstring seed = L"\xfeff[Export]\r\nResolutionCheck=0\r\nResolution=99\r\nDirectory=D:\\VYNE test é\\影像\r\n";
    std::ofstream file(directory / L"Export.ini", std::ios::binary);
    file.write(reinterpret_cast<const char*>(seed.data()), static_cast<std::streamsize>(seed.size() * sizeof(wchar_t))); file.close();
    initialize_export_settings(directory);
    require(export_settings().resolution == 2, "invalid stored resolution normalized");
    require(export_settings().output == std::filesystem::path(L"D:\\VYNE test é\\影像"), "Unicode output path survives Windows INI loading");
    step_export_setting(ExportRow::mode, 1); step_export_setting(ExportRow::resolution, 1);
    initialize_export_settings(directory);
    require(export_settings().resolution_check && export_settings().resolution == 0, "menu settings persist and reload through the Windows profile API");
    require(export_settings().output == std::filesystem::path(L"D:\\VYNE test é\\影像"), "atomic settings updates preserve Unicode path");
    std::cout << "PASS: native row layout, unchanged Export, no inherited actions, row limits, capability gating and Unicode persistence.\n";
}
