#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

// This model is deliberately not the Geometry Dash collision engine.
// It models single, axis-aligned, constant-speed Wave in a static world.
namespace frameguard {
struct Rect { double left, bottom, right, top; };
struct State {
    double x = 0, y = 0;
    double dx = 0, rise = 0; // displacement per OBSERVED physics update
    double halfWidth = 2, halfHeight = 2;
    bool held = false, inverted = false;
};
struct World {
    std::vector<Rect> obstacles;
    // Barriers are unsupported objects/portals; crossing means "unknown", not safe.
    std::vector<Rect> barriers;
};
inline bool finite(State const& s) {
    return std::isfinite(s.x) && std::isfinite(s.y) &&
        std::isfinite(s.dx) && std::isfinite(s.rise) &&
        std::isfinite(s.halfWidth) && std::isfinite(s.halfHeight) &&
        s.dx > 0 && s.rise > 0 && s.halfWidth >= 0 && s.halfHeight >= 0;
}
inline State step(State s) {
    s.x += s.dx;
    s.y += (s.held != s.inverted ? 1.0 : -1.0) * s.rise;
    return s;
}
inline Rect expanded(Rect r, double x, double y) {
    return {r.left-x, r.bottom-y, r.right+x, r.top+y};
}
// Slab test: continuous segment vs AABB, including its boundary.
// This prevents tunneling between discrete sample points.
inline bool intersects(double x0, double y0, double x1, double y1, Rect r) {
    if (r.left > r.right || r.bottom > r.top) return false;
    double lo = 0.0, hi = 1.0;
    auto axis = [&](double p, double d, double a, double b) {
        if (std::abs(d) < 1e-12) return p >= a && p <= b;
        double u = (a-p)/d, v = (b-p)/d;
        if (u > v) std::swap(u,v);
        lo = std::max(lo,u); hi = std::min(hi,v);
        return lo <= hi;
    };
    return axis(x0,x1-x0,r.left,r.right) && axis(y0,y1-y0,r.bottom,r.top);
}
inline bool clear(State const& a, State const& b, World const& world) {
    auto hit = [&](std::vector<Rect> const& list) {
        for (auto const& r : list)
            if (intersects(a.x,a.y,b.x,b.y,expanded(r,a.halfWidth,a.halfHeight)))
                return true;
        return false;
    };
    return !hit(world.obstacles) && !hit(world.barriers);
}
inline std::optional<State> simulate(State s, bool desired, int delay,
                                      int ticks, World const& world) {
    if (!finite(s) || delay < 0 || ticks < 0) return std::nullopt;
    for (int i=0; i<ticks; ++i) {
        if (i == delay) s.held = desired;
        State next = step(s);
        if (!clear(s,next,world)) return std::nullopt;
        s = next;
    }
    return s;
}
inline int boundedWindow(int n) { return std::clamp(n,0,10); }
// Return the nearest predicted safe NON-NEGATIVE offset, or no proposal.
// Always preserve immediate input if it is already predicted safe.
inline std::optional<int> future(State const& s, bool desired, int window,
                                 int horizon, World const& world) {
    window = boundedWindow(window);
    horizon = std::max(horizon,window+1);
    for (int d=0; d<=window; ++d)
        if (simulate(s,desired,d,horizon,world)) return d;
    return std::nullopt;
}
struct PastChoice { int age; State present; };
// History entry is the state BEFORE age ticks; the new edge is placed there.
// Caller owns event-order/epoch validation and actual checkpoint storage.
inline std::optional<PastChoice> past(State const& historical, bool desired,
    int age, int window, int futureHorizon, World const& world) {
    if (age <= 0 || age > boundedWindow(window)) return std::nullopt;
    if (!simulate(historical,desired,0,age+futureHorizon,world)) return std::nullopt;
    auto now = simulate(historical,desired,0,age,world);
    if (!now) return std::nullopt;
    return PastChoice{age,*now};
}
// One collision cannot keep restarting its grace timer forever.
struct Grace {
    bool active = false;
    int elapsed = 0;
    void begin() { if (!active) { active=true; elapsed=0; } }
    bool advance(int budget) { return active && ++elapsed >= boundedWindow(budget); }
    void clear() { active=false; elapsed=0; }
};
} // namespace frameguard
