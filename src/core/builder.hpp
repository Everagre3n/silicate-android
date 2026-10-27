// Compile a sanitised Plan into Geometry Dash objects.
// Grid: column c / row r -> x = 30c+15, y = 30r+15 (row 0 sits on the ground).
#pragma once
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "decorate.hpp"
#include "format.hpp"
#include "objects.hpp"
#include "rng.hpp"
#include "spec.hpp"

namespace silicate {

constexpr int START_COL = 10;
constexpr int HEAD = 9;       // cube: rows above the floor treated as reachable
constexpr int CEIL_TOP = 10;  // arena modes: playable rows are 0..CEIL_TOP-1 at most

class World {
public:
    std::vector<Obj> objs;
    std::map<int, int> floorH, reach, ceil;
    int lastHazard = -99;

    void put(int id, int c, int r, std::map<int, double> props = {}) {
        objs.push_back(Obj{id, c * 30.0 + 15, r * 30.0 + 15, std::move(props)});
    }
    int floor(int c, int n, int h, int arenaReach = -1) {
        for (int col = c; col < c + n; ++col) {
            setFloor(col, h);
            int r = arenaReach >= 0 ? arenaReach : h + HEAD;
            auto it = reach.find(col);
            reach[col] = it == reach.end() ? r : std::max(it->second, r);
        }
        return c + n;
    }
    void setFloor(int col, int h) {
        floorH[col] = h;
        for (int r = 0; r < h; ++r) {
            Style s = blockStyle(r == h - 1 ? Role::Top : Role::Fill, col, r, h - 1 - r);
            put(s.id, col, r, s.props);
        }
    }
    int decoFloor(int col) const {  // first row where solid decoration is allowed; -1 = none
        auto it = reach.find(col);
        if (it == reach.end() || ceil.count(col)) return -1;
        return it->second + 1;
    }
};

// ------------------------------------------------------------------ cube
inline int padToFlat(World& w, int c, int h, const Cfg& cfg) {
    int gap = c - w.lastHazard;
    return gap < cfg.minFlat ? w.floor(c, cfg.minFlat - gap, h) : c;
}

inline void runCube(World& w, int& c, int& h, const std::vector<Element>& elems, const Cfg& cfg) {
    for (auto& e : elems) {
        const std::string& t = e.t;
        if (t == "flat") {
            c = w.floor(c, e.v, h);
        } else if (t == "spike") {
            int n = std::min(e.v, cfg.maxSpike);
            c = padToFlat(w, c, h, cfg);
            w.floor(c, n, h);
            for (int i = 0; i < n; ++i) w.put(O::SPIKE, c + i, h);
            c += n;
            w.lastHazard = c;
        } else if (t == "gap" || t == "pad_gap" || t == "orb_gap") {
            int extra = t == "gap" ? 0 : t == "pad_gap" ? 2 : 3;
            int n = std::min(e.v, cfg.maxGap + extra);
            c = padToFlat(w, c, h, cfg);
            if (h == 0) {  // no pits at ground level: use a spike run instead
                n = std::min(n, cfg.maxSpike + extra);
                if (t == "pad_gap") { w.put(O::PAD_YELLOW, c - 1, 0); n = std::min(n, 5); }
                int start = c;
                w.floor(c, n, 0, HEAD + (extra ? 4 : 0));
                for (int i = 0; i < n; ++i) w.put(O::SPIKE, c + i, 0);
                if (t == "orb_gap") w.put(O::ORB_YELLOW, start + n / 2, 2);
                c += n;
            } else {
                if (t == "pad_gap") w.put(O::PAD_YELLOW, c - 1, h);
                for (int i = 0; i < n; ++i) {
                    w.setFloor(c + i, 0);
                    w.reach[c + i] = h + HEAD + (extra ? 4 : 0);
                    w.put(O::SPIKE, c + i, 0);
                }
                if (t == "orb_gap") w.put(O::ORB_YELLOW, c + n / 2, h + 2);
                c += n;
            }
            w.lastHazard = c;
            c = w.floor(c, 2, h);
        } else if (t == "up") {
            c = w.floor(c, 2, h);
            h = std::min(6, h + std::min(e.v, cfg.maxRise));
        } else if (t == "down") {
            h = std::max(0, h - e.v);
        } else if (t == "platform") {
            c = w.floor(c, 2, h);
            h = std::min(6, h + 1);
            c = w.floor(c, e.v, h);
            h = std::max(0, h - 1);
        } else if (t == "stairs") {
            for (int i = 0; i < e.v; ++i) { c = w.floor(c, 3, h); h = std::min(6, h + 1); }
            c = w.floor(c, 3, h);
            h = std::max(0, h - e.v);
        } else if (t == "coin") {
            c = w.floor(c, 2, h);
            w.put(O::COIN, c - 1, h + 2);
        }
    }
}

inline std::vector<Element> genCube(const std::string& style, int cols, double density, const Cfg& cfg, Rng& rng) {
    std::vector<Element> out;
    int left = cols;
    auto add = [&](const char* t, int v, int cost) { out.push_back({t, v}); left -= cost; };
    int flatLo = 1 + (int)((1 - density) * 4);
    static const std::vector<std::string> families{"stairs", "towers", "spike_run", "pit_run", "orb_run", "pad_run"};
    while (left > 0) {
        std::string s = style != "mixed" ? style : rng.choice(families);
        add("flat", rng.randint(flatLo, flatLo + 3), 4);
        if (s == "stairs") {
            int n = rng.randint(2, 4);
            add("stairs", n, n * 3 + 3);
            add("spike", rng.randint(1, cfg.maxSpike), 4);
        } else if (s == "towers") {
            add("platform", rng.randint(3, 7), 9);
            if (rng.random() < density) add("gap", rng.randint(2, cfg.maxGap), 7);
        } else if (s == "spike_run") {
            int k = rng.randint(1, 2 + (int)(density * 3));
            for (int i = 0; i < k; ++i) add("spike", rng.randint(1, cfg.maxSpike), 6);
        } else if (s == "pit_run") {
            add("up", 1, 3);
            int k = rng.randint(1, 3);
            for (int i = 0; i < k; ++i) { add("flat", rng.randint(3, 5), 4); add("gap", rng.randint(2, cfg.maxGap), 7); }
            add("down", 1, 1);
        } else if (s == "orb_run") {
            add("orb_gap", rng.randint(3, cfg.maxGap + 3), 9);
        } else {
            add("pad_gap", rng.randint(2, cfg.maxGap + 2), 9);
        }
        if (rng.random() < 0.15) add("coin", 1, 2);
    }
    return out;
}

// ----------------------------------------------------------- arena modes
inline void ceilingFill(World& w, int col, int cs) {
    for (int r = cs; r < CEIL_TOP + 2; ++r) {
        Style s = blockStyle(r == cs ? Role::Top : Role::Fill, col, r);
        w.put(s.id, col, r, s.props);
    }
}

inline int corridor(World& w, int c, int cols, int gap, int slopeCols, int wobble, Rng& rng, bool pillars, bool spikes) {
    int top = CEIL_TOP - gap;
    std::vector<int> fs;
    int f = 0, target = 0;
    for (int i = 0; i < cols; ++i) {
        if (i % 6 == 0 || f == target) target = rng.randint(0, std::max(0, std::min(top, wobble)));
        if (i % slopeCols == 0) f += (target > f) - (target < f);
        fs.push_back(std::max(0, std::min(top, f)));
    }
    for (int i = 0; i < cols; ++i) fs[i] = std::min(fs[i], (cols - 1 - i) / slopeCols);  // glide to ground
    int nextPillar = c + 8;
    for (int i = 0; i < cols; ++i) {
        int col = c + i;
        f = fs[i];
        w.setFloor(col, f);
        int cs = f + gap;
        w.ceil[col] = cs;
        w.reach[col] = cs - 1;
        ceilingFill(w, col, cs);
        if (pillars && col >= nextPillar && cols - i > 10) {
            int ph = rng.randint(1, std::max(1, gap - 4));
            std::map<int, double> p{{O::K_MAIN_COLOR, O::CH_OBJ}, {O::K_ZLAYER, O::Z_B1}};
            if (rng.random() < 0.5) for (int r = f; r < f + ph; ++r) w.put(O::BLOCK, col, r, p);
            else for (int r = cs - ph; r < cs; ++r) w.put(O::BLOCK, col, r, p);
            nextPillar = col + rng.randint(6, 9);
        } else if (spikes && gap >= 5 && col % 7 == 0 && i > 3 && i < cols - 6) {
            if (rng.random() < 0.5) w.put(O::SPIKE_SMALL, col, f);
            else w.put(O::SPIKE_SMALL, col, cs - 1, {{O::K_ROT, 180}});
        }
    }
    return c + cols;
}

inline int ballArena(World& w, int c, int cols, double density, Rng& rng) {
    const int gap = 5;
    int lastSide = 0, hazardAt = c + 4;
    for (int i = 0; i < cols; ++i) {
        int col = c + i;
        w.setFloor(col, 0);
        w.ceil[col] = gap;
        w.reach[col] = gap - 1;
        ceilingFill(w, col, gap);
        if (col >= hazardAt && i < cols - 6) {
            int side = rng.random() < 0.7 ? rng.randint(0, 1) : lastSide;
            int n = rng.randint(1, 2);
            for (int k = 0; k < n; ++k) {
                if (side == 0) w.put(O::SPIKE, col + k, 0);
                else w.put(O::SPIKE, col + k, gap - 1, {{O::K_ROT, 180}});
            }
            hazardAt = col + n + (side != lastSide ? 7 : 6) + (int)((1 - density) * 4);
            lastSide = side;
        }
    }
    return c + cols;
}

inline int buildArena(World& w, const std::string& mode, int c, int cols, const Cfg& cfg, const Section& s, Rng& rng) {
    double d = s.density;
    if (mode == "ship") return corridor(w, c, cols, cfg.shipGap + (s.mood == "calm" ? 1 : 0), 2, 2 + (int)(d * 4), rng, d > 0.55, d > 0.4);
    if (mode == "wave") return corridor(w, c, cols, cfg.waveGap, 1, 2 + (int)(d * 5), rng, false, false);
    if (mode == "ufo") return corridor(w, c, cols, 7, 3, 2 + (int)(d * 3), rng, true, false);
    return ballArena(w, c, cols, d, rng);
}

// ------------------------------------------------------------- compiler
inline std::vector<Obj> moodTriggers(const std::string& theme, const std::string& mood, double x,
                                     std::map<int, RGB>& prev) {
    auto pal = palette(theme, mood);
    std::vector<Obj> out;
    for (auto& kv : pal) {
        auto it = prev.find(kv.first);
        if (it != prev.end() && it->second == kv.second) continue;
        out.push_back(Obj{O::COLOR_TRIGGER, x, 30.0 * 16 + 15,
                          {{O::K_R, (double)kv.second[0]}, {O::K_G, (double)kv.second[1]}, {O::K_B, (double)kv.second[2]},
                           {O::K_DUR, 1.0}, {O::K_TARGET_COLOR, (double)kv.first}, {O::K_OPACITY, 1}, {O::K_HIGH_DETAIL, 1}}});
    }
    prev = pal;
    return out;
}

inline std::vector<Obj> pulseTriggers(int c0, int c1, int bpm, double ups, int everyBars) {
    double barCols = (4 * 60.0 / bpm) * ups / 30.0;
    std::vector<Obj> out;
    for (double c = c0; c < c1; c += barCols * everyBars) {
        out.push_back(Obj{O::PULSE_TRIGGER, c * 30 + 15, 30.0 * 17 + 15,
                          {{O::K_R, 255}, {O::K_G, 255}, {O::K_B, 255}, {O::K_PULSE_IN, 0}, {O::K_PULSE_HOLD, 0.05},
                           {O::K_PULSE_OUT, 0.35}, {O::K_PULSE_MODE, 0}, {O::K_PULSE_TARGET, O::CH_EDGE},
                           {O::K_PULSE_TARGET_TYPE, 0}, {O::K_HIGH_DETAIL, 1}}});
    }
    return out;
}

struct Level {
    std::string header;
    std::vector<Obj> objs;
    World world;
    std::string raw() const { return rawLevelString(header, objs); }
};

inline Level compileLevel(const Plan& plan, uint64_t seed) {
    Rng rng(seed);
    Level lv;
    World& w = lv.world;
    int c = START_COL, h = 0;
    std::string mode = "cube", curSpeed;
    const std::string firstSpeed = plan.sections.front().speed;
    std::map<int, RGB> prevPal;
    std::vector<Obj> extras;
    struct Range { int a, b; std::string mood; };
    std::vector<Range> ranges;
    Cfg cfg = difficultyCfg(plan.difficulty, firstSpeed);

    c = w.floor(c, 8, 0);
    for (auto& sec : plan.sections) {
        cfg = difficultyCfg(plan.difficulty, sec.speed);
        const O::Speed* sp = O::findSpeed(sec.speed);
        int barCols = std::max(4, (int)((4 * 60.0 / plan.bpm) * sp->ups / 30.0));
        int cols = sec.bars * barCols, start = c;

        if (sec.speed != curSpeed && !(curSpeed.empty() && sec.speed == firstSpeed)) w.put(sp->portal, c, 2);
        curSpeed = sec.speed;

        if (sec.gamemode != mode) {
            if (h) runCube(w, c, h, {{"down", h}}, cfg);
            c = w.floor(c, 3, 0);
            w.put(O::portalFor(sec.gamemode), c, sec.gamemode == "cube" ? 2 : 3);
            c = w.floor(c, 2, 0);
            mode = sec.gamemode;
            start = c;
        }
        auto trig = moodTriggers(plan.theme, sec.mood, start * 30.0 + 15, prevPal);
        extras.insert(extras.end(), trig.begin(), trig.end());

        if (mode == "cube") {
            auto elems = sec.elements.empty() ? genCube(sec.style, cols, sec.density, cfg, rng) : sec.elements;
            runCube(w, c, h, elems, cfg);
            c = w.floor(c, cfg.minFlat, h);
        } else {
            c = w.floor(c, 3, 0, CEIL_TOP);
            c = buildArena(w, mode, c, cols, cfg, sec, rng);
            c = w.floor(c, 3, 0, CEIL_TOP);
        }
        auto pulses = pulseTriggers(start, c, plan.bpm, sp->ups, sec.mood == "drop" ? 1 : 2);
        extras.insert(extras.end(), pulses.begin(), pulses.end());
        ranges.push_back({start, c, sec.mood});
    }
    if (h) runCube(w, c, h, {{"down", h}}, cfg);
    if (mode != "cube") { w.put(O::PORTAL_CUBE, c, 2); c = w.floor(c, 3, 0); }
    c = w.floor(c, 8, 0);
    extras.push_back(Obj{O::END_TRIGGER, c * 30.0 + 15, 30.0 * 3 + 15, {}});

    std::vector<Obj> deco;
    for (auto& r : ranges) {
        auto d = decorateSky(w, rng, r.a, r.b, r.mood);
        deco.insert(deco.end(), d.begin(), d.end());
    }
    lv.header = buildHeader(palette(plan.theme, plan.sections.front().mood), O::findSpeed(firstSpeed)->header,
                            rng.randint(1, 20), rng.randint(1, 17));
    lv.objs = w.objs;
    lv.objs.insert(lv.objs.end(), deco.begin(), deco.end());
    lv.objs.insert(lv.objs.end(), extras.begin(), extras.end());
    return lv;
}

}  // namespace silicate
