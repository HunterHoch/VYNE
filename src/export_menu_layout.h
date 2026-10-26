#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace vyne {
struct NativeExportOption {
    std::uint32_t text, context, block, link, jump, action, toggle, warning, dependent_action, unique;
};
template<class T> struct NativeArray {
    T* data;
    std::uint16_t count, capacity;
    std::uint32_t padding;
};
struct NativeEditorMenu {
    std::uint32_t id, column, type, data_type;
    std::uint64_t reserved;
    NativeArray<NativeExportOption> options;
    void* content;
    std::uint32_t selected, padding;
};
static_assert(sizeof(NativeExportOption) == 0x28);
static_assert(offsetof(NativeExportOption, context) == 0x04);
static_assert(offsetof(NativeExportOption, block) == 0x08);
static_assert(offsetof(NativeExportOption, action) == 0x14);
static_assert(offsetof(NativeExportOption, toggle) == 0x18);
static_assert(sizeof(NativeEditorMenu) == 0x38);
static_assert(offsetof(NativeEditorMenu, options) == 0x18);
static_assert(offsetof(NativeEditorMenu, content) == 0x28);
constexpr std::uint32_t export_option_key(unsigned row) { return 0x56594e40 + row; }
inline int export_option_row(std::uint32_t key) {
    return key >= export_option_key(0) && key < export_option_key(7) ? static_cast<int>(key - export_option_key(0)) : -1;
}
inline std::vector<NativeExportOption> compose_export_options(const NativeExportOption* stock, unsigned count) {
    if (!stock || !count || count + 7 > 16) return {};
    unsigned insertion = 0, example = 0;
    for (unsigned i = 0; i < count; ++i) {
        if (export_option_row(stock[i].toggle) >= 0) return {};
        if (stock[i].toggle) { insertion = i + 1; example = i; }
    }
    if (!insertion) return {}; // No verified stock toggle template: leave native data alone.
    std::vector<NativeExportOption> result;
    result.reserve(count + 7);
    result.insert(result.end(), stock, stock + insertion);
    for (unsigned row = 0; row < 7; ++row) {
        NativeExportOption option{};
        option.text = option.toggle = option.unique = export_option_key(row);
        option.block = stock[example].block;
        option.link = stock[example].link;
        result.push_back(option);
    }
    result.insert(result.end(), stock + insertion, stock + count);
    return result;
}
}
