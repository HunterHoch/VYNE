#include "../src/graphics_mode.h"
#include <cstdlib>
#include <iostream>

using namespace vyne;
namespace {
void require(bool condition, const char* text) {
    if (!condition) { std::cerr << "FAIL: " << text << '\n'; std::exit(1); }
}
struct Game {
    GraphicsState state{2560, 1440, 120, 1, 0, 0, 2, 1, 1, 1};
    RenderDimensions measured{2560, 1440, 0};
    unsigned requests = 0;
    bool reject_next = false;
    bool reject_restore = false;
    static bool snapshot(void* p, GraphicsState& out) noexcept {
        out = static_cast<Game*>(p)->state; return true;
    }
    static bool request(void* p, const GraphicsState& value) noexcept {
        auto& g = *static_cast<Game*>(p); ++g.requests;
        g.state = value; // Simulate a partially applied failing request too.
        if (g.reject_next) { g.reject_next = false; return false; }
        return !(g.reject_restore && value.width == 2560);
    }
    static bool dimensions(void* p, RenderDimensions& out) noexcept {
        out = static_cast<Game*>(p)->measured; return true;
    }
    GraphicsBackend backend() { return {this, snapshot, request, dimensions, true}; }
    GraphicsState target() const { auto mode = state; mode.width = 3840; mode.height = 2160; return mode; }
    void frame(GraphicsModeSession& session, std::uint64_t time) {
        ++measured.frame; session.tick(time);
    }
};
void stabilize(Game& game, GraphicsModeSession& session, unsigned w, unsigned h) {
    game.measured.width = w; game.measured.height = h;
    game.frame(session, 100); game.frame(session, 101); game.frame(session, 102);
}
}

int main() {
    {
        Game g; auto backend = g.backend(); backend.validated = false;
        GraphicsModeSession s(backend);
        require(!s.begin(g.target(), 0) && g.requests == 0, "unsupported adapter must not change state");
    }
    {
        Game g; GraphicsModeSession s(g.backend());
        const auto original = g.state;
        require(s.begin(g.target(), 0), "mode request accepted");
        require(!s.capture_allowed(), "capture blocked until target dimensions stabilize");
        g.measured = {3840, 2160, 1};
        s.tick(1); s.tick(2); s.tick(3);
        require(!s.capture_allowed(), "repeated observation of one frame is not stabilization");
        g.frame(s, 4); g.frame(s, 5);
        require(s.capture_allowed(), "three distinct matching frames permit capture");
        s.finish(6);
        require(!s.capture_allowed(), "finish closes capture before restoring");
        stabilize(g, s, 2560, 1440);
        require(s.status() == ModeStatus::restored && g.state == original, "success restores every saved field");
    }
    {
        Game g; GraphicsModeSession s(g.backend());
        s.begin(g.target(), 0); s.cancel(1);
        stabilize(g, s, 2560, 1440);
        require(s.status() == ModeStatus::restored && g.requests == 2, "cancel during asynchronous switch restores once");
    }
    {
        Game g; g.reject_next = true; GraphicsModeSession s(g.backend());
        require(!s.begin(g.target(), 0) && g.requests == 2, "failed mode request rolls back partial changes");
        stabilize(g, s, 2560, 1440);
        require(s.status() == ModeStatus::restored, "rejected target recovers original mode");
    }
    {
        Game g; GraphicsModeSession s(g.backend());
        s.begin(g.target(), 0); s.tick(10000);
        require(s.status() == ModeStatus::restoring && !s.capture_allowed(), "wrong dimensions time out and restore");
        stabilize(g, s, 2560, 1440);
        require(s.status() == ModeStatus::restored, "timeout recovery completes");
    }
    {
        Game g; GraphicsModeSession s(g.backend());
        s.begin(g.target(), 0); stabilize(g, s, 3840, 2160);
        g.measured.width = 1920; g.frame(s, 103);
        require(s.status() == ModeStatus::restoring && !s.capture_allowed(), "resize during capture aborts and restores");
    }
    {
        Game g; g.reject_restore = true; GraphicsModeSession s(g.backend());
        s.begin(g.target(), 0); s.cancel(1);
        require(s.status() == ModeStatus::restore_failed, "failed restore requires explicit recovery");
    }
    {
        Game g; GraphicsModeSession s(g.backend());
        s.begin(g.target(), 0); stabilize(g, s, 3840, 2160); s.finish(103);
        g.state.vsync = 0; stabilize(g, s, 2560, 1440); s.tick(10103);
        require(s.status() == ModeStatus::restore_failed, "matching dimensions alone do not prove full restoration");
    }
    std::cout << "PASS: graphics session success, distinct frames, cancel, partial failure, timeout, resize and restore failure.\n";
}
