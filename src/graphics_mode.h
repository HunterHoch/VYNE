#pragma once
#include <cstdint>
#include <string>
#include <array>

namespace vyne {
struct GraphicsState {
    std::uint32_t width = 0, height = 0;
    std::uint32_t refresh_numerator = 0, refresh_denominator = 1;
    std::uint32_t adapter = 0, monitor = 0, window_mode = 0, vsync = 0;
    std::uint32_t scale_numerator = 1, scale_denominator = 1;
    std::array<std::uint32_t, 6> native_state{};
    bool operator==(const GraphicsState& other) const noexcept;
};

struct RenderDimensions {
    std::uint32_t width = 0, height = 0;
    std::uint64_t frame = 0;
};

// These callbacks must be supplied by a separately validated game adapter and
// called on its owning game/render thread. No guessed addresses are accepted.
struct GraphicsBackend {
    void* context = nullptr;
    bool (*snapshot)(void*, GraphicsState&) noexcept = nullptr;
    bool (*request)(void*, const GraphicsState&) noexcept = nullptr;
    bool (*dimensions)(void*, RenderDimensions&) noexcept = nullptr;
    bool validated = false;
};

enum class ModeStatus { idle, switching, ready, restoring, restored, failed, restore_failed };

class GraphicsModeSession {
public:
    explicit GraphicsModeSession(GraphicsBackend backend) noexcept : backend_(backend) {}
    // One session per export. Always call finish/cancel and keep ticking until
    // terminal; a destructor cannot safely issue game-thread graphics calls.
    bool begin(const GraphicsState& requested, std::uint64_t now_ms);
    void tick(std::uint64_t now_ms);
    void finish(std::uint64_t now_ms);
    void cancel(std::uint64_t now_ms);
    ModeStatus status() const noexcept { return status_; }
    const std::string& error() const noexcept { return error_; }
    const GraphicsState& original() const noexcept { return original_; }
    bool capture_allowed() const noexcept { return status_ == ModeStatus::ready; }
private:
    void restore(std::uint64_t now_ms, const char* reason);
    GraphicsBackend backend_;
    GraphicsState original_{}, requested_{};
    ModeStatus status_ = ModeStatus::idle;
    std::uint64_t deadline_ = 0;
    unsigned stable_samples_ = 0;
    std::uint64_t last_sample_frame_ = UINT64_MAX;
    std::string error_;
};
}
