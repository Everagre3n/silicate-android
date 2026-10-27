// Themes and decoration.
// Rule that matters: solid decoration is only placed where the player can never reach.
// Block hitboxes ignore z-layer, so decorating playable space would kill or block the player.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "format.hpp"
#include "objects.hpp"
#include "rng.hpp"

namespace silicate {

inline const std::vector<int> CHANNEL_ORDER{O::CH_BG, O::CH_GROUND, O::CH_LINE, O::CH_OBJ, O::CH_FILL,
                                           O::CH_FILL2, O::CH_DEC1, O::CH_DEC2, O::CH_EDGE};

inline const std::map<std::string, std::vector<RGB>>& themes() {
    // bg, ground, line, obj, fill, fill2, dec1, dec2, edge
    static const std::map<std::string, std::vector<RGB>> t{
        {"neon", {{{10, 8, 40}}, {{20, 14, 70}}, {{0, 255, 255}}, {{30, 30, 90}}, {{20, 20, 70}}, {{28, 26, 96}}, {{60, 20, 140}}, {{0, 160, 200}}, {{0, 255, 220}}}},
        {"fire", {{{40, 6, 6}}, {{60, 14, 8}}, {{255, 160, 0}}, {{110, 24, 10}}, {{70, 14, 8}}, {{90, 20, 10}}, {{160, 40, 10}}, {{255, 110, 0}}, {{255, 210, 60}}}},
        {"ice", {{{10, 30, 60}}, {{20, 50, 90}}, {{200, 240, 255}}, {{50, 110, 170}}, {{30, 70, 120}}, {{40, 90, 140}}, {{90, 160, 220}}, {{160, 220, 255}}, {{240, 255, 255}}}},
        {"forest", {{{8, 30, 16}}, {{14, 46, 20}}, {{150, 255, 120}}, {{30, 90, 40}}, {{20, 60, 30}}, {{26, 76, 36}}, {{40, 120, 60}}, {{110, 200, 90}}, {{200, 255, 150}}}},
        {"void", {{{4, 4, 8}}, {{12, 12, 20}}, {{200, 200, 255}}, {{40, 40, 60}}, {{24, 24, 36}}, {{32, 32, 48}}, {{70, 40, 120}}, {{110, 110, 170}}, {{230, 230, 255}}}},
        {"sunset", {{{60, 20, 60}}, {{90, 30, 70}}, {{255, 140, 100}}, {{150, 50, 80}}, {{100, 30, 70}}, {{120, 40, 80}}, {{220, 80, 90}}, {{255, 170, 80}}, {{255, 230, 150}}}},
        {"candy", {{{60, 20, 80}}, {{90, 40, 110}}, {{255, 150, 230}}, {{200, 90, 190}}, {{140, 60, 150}}, {{170, 80, 170}}, {{255, 120, 200}}, {{120, 220, 255}}, {{255, 255, 255}}}},
    };
    return t;
}

inline RGB mixColor(const RGB& a, const RGB& b, double t) {
    RGB r;
    for (int i = 0; i < 3; ++i) r[i] = (int)std::lround(a[i] + (b[i] - a[i]) * t);
    return r;
}

inline std::map<int, RGB> palette(const std::string& theme, const std::string& mood) {
    auto it = themes().find(theme);
    const auto& base = (it != themes().end() ? it->second : themes().at("neon"));
    RGB edge = base[8];
    std::map<int, RGB> out;
    for (size_t i = 0; i < CHANNEL_ORDER.size(); ++i) {
        int ch = CHANNEL_ORDER[i];
        RGB c = base[i];
        if (mood == "calm") c = mixColor(c, {{0, 0, 0}}, 0.25);
        else if (mood == "build") { if (ch != O::CH_BG && ch != O::CH_GROUND) c = mixColor(c, edge, 0.12); }
        else if (mood == "drop") c = ch != O::CH_BG ? mixColor(c, edge, 0.3) : mixColor(c, {{0, 0, 0}}, 0.35);
        else if (mood == "break") { int g = (c[0] + c[1] + c[2]) / 3; c = mixColor(c, {{g, g, g}}, 0.5); }
        out[ch] = c;
    }
    return out;
}

struct Style { int id; std::map<int, double> props; };

enum class Role { Top, Edge, Fill };

inline Style blockStyle(Role role, int c, int r, int depth = 0) {
    if (role == Role::Top) return {O::BLOCK, {{O::K_MAIN_COLOR, O::CH_EDGE}, {O::K_ZLAYER, O::Z_B1}}};
    if (role == Role::Edge) return {O::BLOCK, {{O::K_MAIN_COLOR, O::CH_OBJ}, {O::K_ZLAYER, O::Z_B2}}};
    int ch = (c + r) % 2 == 0 ? O::CH_FILL : O::CH_FILL2;
    if (c % 4 == 0) ch = O::CH_DEC1;  // support-column look
    else if (depth && depth % 5 == 0) ch = O::CH_OBJ;
    return {O::BLOCK, {{O::K_MAIN_COLOR, ch}, {O::K_ZLAYER, O::Z_B3}}};
}

inline Obj makeDeco(int id, double x, double y, int ch, int z, double scale = 1, int rot = 0) {
    Obj o{id, x, y, {{O::K_MAIN_COLOR, (double)ch}, {O::K_ZLAYER, (double)z}, {O::K_HIGH_DETAIL, 1}, {O::K_NO_TOUCH, 1}}};
    if (scale != 1) o.props[O::K_SCALE] = scale;
    if (rot) o.props[O::K_ROT] = rot;
    return o;
}

// `W` must provide: int decoFloor(int col) (returns -1 when the column can't be decorated).
template <class W>
std::vector<Obj> decorateSky(const W& world, Rng& rng, int c0, int c1, const std::string& mood) {
    std::vector<Obj> out;
    int c = c0 + rng.randint(2, 5);
    while (c < c1) {
        int lo = world.decoFloor(c);
        if (lo < 0) { c += 3; continue; }
        double kind = rng.random();
        double cx = c * 30 + 15;
        if (kind < 0.45) {  // hanging chain, tapering diamonds
            int top = lo + rng.randint(6, 9), n = rng.randint(3, 6);
            for (int i = 0; i < n; ++i)
                out.push_back(makeDeco(O::BLOCK, cx, (top - i) * 30 + 15, O::CH_DEC1, O::Z_B4, 0.35 + 0.1 * i, 45));
            out.push_back(makeDeco(O::BLOCK, cx, (top - n) * 30 + 15, O::CH_EDGE, O::Z_B3, 0.9, 45));
        } else if (kind < 0.8) {  // floating twinkles
            int count = rng.randint(2, 4);
            for (int k = 0; k < count; ++k) {
                int cc = c + rng.randint(-2, 2);
                int rr = world.decoFloor(cc);
                if (rr < 0) continue;
                double y = (rr + rng.randint(1, 8)) * 30 + 15;
                static const double sizes[] = {0.3, 0.5, 0.7};
                out.push_back(makeDeco(O::BLOCK, cc * 30 + 15, y, rng.random() < 0.5 ? O::CH_DEC2 : O::CH_EDGE,
                                       O::Z_B3, sizes[rng.randint(0, 2)], 45));
            }
        } else {  // tall back pillar with cap
            int h = rng.randint(3, 5);
            for (int i = 0; i < h; ++i)
                out.push_back(makeDeco(O::BLOCK, cx, (lo + 5 + i) * 30 + 15, O::CH_DEC1, O::Z_B4, 0.6));
            out.push_back(makeDeco(O::BLOCK, cx, (lo + 5 + h) * 30 + 15, O::CH_DEC2, O::Z_B4, 0.9, 45));
        }
        c += mood != "calm" ? rng.randint(4, 9) : rng.randint(7, 13);
    }
    return out;
}

}  // namespace silicate
