#include "graphics_mode.h"

bool vyne::GraphicsState::operator==(const GraphicsState& b) const noexcept {
    return width == b.width && height == b.height &&
        refresh_numerator == b.refresh_numerator && refresh_denominator == b.refresh_denominator &&
        adapter == b.adapter && monitor == b.monitor && window_mode == b.window_mode && vsync == b.vsync &&
        scale_numerator == b.scale_numerator && scale_denominator == b.scale_denominator && native_state == b.native_state;
}

bool vyne::GraphicsModeSession::begin(const GraphicsState& requested, std::uint64_t now_ms) {
    if (status_ != ModeStatus::idle) return false;
    if (!backend_.validated || !backend_.snapshot || !backend_.request || !backend_.dimensions) {
        status_ = ModeStatus::failed;
        error_ = "No validated game graphics adapter; mode change refused.";
        return false;
    }
    if (!requested.width || !requested.height || !requested.refresh_denominator ||
        !requested.scale_numerator || !requested.scale_denominator) {
        status_ = ModeStatus::failed;
        error_ = "Invalid graphics mode.";
        return false;
    }
    if (!backend_.snapshot(backend_.context, original_) || !original_.width || !original_.height) {
        status_ = ModeStatus::failed;
        error_ = "Original graphics state unavailable; mode change refused.";
        return false;
    }
    requested_ = requested;
    status_ = ModeStatus::switching;
    deadline_ = now_ms + 10000;
    if (!backend_.request(backend_.context, requested_)) {
        // A failed request can still have partially changed device state.
        restore(now_ms, "Game rejected the requested mode.");
        return false;
    }
    return true;
}

void vyne::GraphicsModeSession::restore(std::uint64_t now_ms, const char* reason) {
    if (reason && *reason) error_ = reason;
    status_ = ModeStatus::restoring;
    stable_samples_ = 0;
    last_sample_frame_ = UINT64_MAX;
    deadline_ = now_ms + 10000;
    if (!backend_.request(backend_.context, original_)) {
        status_ = ModeStatus::restore_failed;
        error_ += " Original mode restore request failed; recovery required.";
    }
}

void vyne::GraphicsModeSession::tick(std::uint64_t now_ms) {
    if (status_ != ModeStatus::switching && status_ != ModeStatus::ready &&
        status_ != ModeStatus::restoring) return;
    RenderDimensions measured{};
    const bool measured_ok = backend_.dimensions(backend_.context, measured);
    const auto& expected = status_ == ModeStatus::restoring ? original_ : requested_;
    const bool matches = measured_ok && measured.width == expected.width && measured.height == expected.height;
    if (status_ == ModeStatus::ready) {
        if (!matches) restore(now_ms, "Render dimensions changed during export; capture stopped.");
        return;
    }
    if (matches) {
        if (measured.frame != last_sample_frame_) ++stable_samples_;
    } else stable_samples_ = 0;
    last_sample_frame_ = measured.frame;
    if (stable_samples_ >= 3) {
        if (status_ == ModeStatus::switching) {
            status_ = ModeStatus::ready;
        } else {
            GraphicsState actual{};
            if (backend_.snapshot(backend_.context, actual) && actual == original_)
                status_ = ModeStatus::restored;
        }
    }
    if (now_ms < deadline_ || status_ == ModeStatus::ready || status_ == ModeStatus::restored) return;
    if (status_ == ModeStatus::switching) restore(now_ms, "Requested render dimensions did not stabilize.");
    else {
        status_ = ModeStatus::restore_failed;
        error_ += " Original graphics state did not restore before timeout; recovery required.";
    }
}

void vyne::GraphicsModeSession::finish(std::uint64_t now_ms) {
    if (status_ == ModeStatus::switching || status_ == ModeStatus::ready) restore(now_ms, "");
}

void vyne::GraphicsModeSession::cancel(std::uint64_t now_ms) {
    if (status_ == ModeStatus::switching || status_ == ModeStatus::ready) restore(now_ms, "Export cancelled.");
}
