// Plan schema: sanitise whatever the model returns into something buildable.
#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

#include "json.hpp"
#include "objects.hpp"
#include "rng.hpp"

namespace silicate {

inline const std::vector<std::string> GAMEMODES{"cube", "ship", "wave", "ball", "ufo"};
inline const std::vector<std::string> MOODS{"calm", "build", "drop", "break"};
inline const std::vector<std::string> STYLES{"mixed", "stairs", "towers", "spike_run", "pit_run", "orb_run", "pad_run"};
inline const std::vector<std::string> DIFFICULTIES{"easy", "normal", "hard", "insane"};
inline const std::vector<std::string> THEME_NAMES{"neon", "fire", "ice", "forest", "void", "sunset", "candy"};

struct ElementSpec { const char* name; const char* param; int lo, hi, def; };
inline const ElementSpec ELEMENTS[] = {
    {"flat", "n", 1, 12, 3}, {"spike", "n", 1, 3, 1}, {"gap", "w", 1, 6, 2}, {"up", "dh", 1, 2, 1},
    {"down", "dh", 1, 5, 1}, {"platform", "n", 2, 10, 4}, {"stairs", "n", 2, 5, 3},
    {"pad_gap", "w", 1, 6, 3}, {"orb_gap", "w", 1, 6, 4}, {"coin", "n", 1, 1, 1},
};

struct Element { std::string t; int v = 0; };

struct Section {
    std::string gamemode = "cube", speed = "1x", mood = "build", style = "mixed";
    int bars = 8;
    double density = 0.5;
    std::vector<Element> elements;
};

struct Plan {
    std::string name = "Untitled", theme = "neon", difficulty = "normal";
    int bpm = 128;
    std::vector<Section> sections;
};

inline bool oneOf(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}
inline int clampInt(double v, int lo, int hi, int def) {
    if (std::isnan(v)) return def;
    return std::max(lo, std::min(hi, (int)std::lround(v)));
}

inline Plan sanitize(const json::Value& j, Rng& rng) {
    if (j.type != json::Value::Obj) throw std::runtime_error("plan must be a JSON object");
    Plan p;
    p.name = j.getStr("name", "Untitled").substr(0, 20);
    if (p.name.empty()) p.name = "Untitled";
    std::string th = j.getStr("theme");
    p.theme = oneOf(THEME_NAMES, th) ? th : rng.choice(THEME_NAMES);
    std::string d = j.getStr("difficulty");
    p.difficulty = oneOf(DIFFICULTIES, d) ? d : "normal";
    p.bpm = clampInt(j.getNum("bpm", 128), 60, 220, 128);
    auto secs = j.get("sections");
    if (secs && secs->type == json::Value::Arr) {
        for (auto& s : secs->a) {
            if (s.type != json::Value::Obj) continue;
            Section sec;
            std::string m = s.getStr("gamemode");
            sec.gamemode = oneOf(GAMEMODES, m) ? m : "cube";
            std::string sp = s.getStr("speed");
            sec.speed = O::findSpeed(sp) ? sp : "1x";
            std::string mo = s.getStr("mood");
            sec.mood = oneOf(MOODS, mo) ? mo : "build";
            std::string st = s.getStr("style");
            sec.style = oneOf(STYLES, st) ? st : "mixed";
            sec.bars = clampInt(s.getNum("bars", 8), 2, 32, 8);
            sec.density = std::max(0.0, std::min(1.0, s.getNum("density", 0.5)));
            auto els = s.get("elements");
            if (els && els->type == json::Value::Arr && sec.gamemode == "cube") {
                for (auto& e : els->a) {
                    std::string name = e.getStr("t");
                    for (auto& spec : ELEMENTS) {
                        if (name == spec.name) {
                            sec.elements.push_back({name, clampInt(e.getNum(spec.param, spec.def), spec.lo, spec.hi, spec.def)});
                            break;
                        }
                    }
                }
            }
            p.sections.push_back(std::move(sec));
        }
    }
    if (p.sections.empty()) throw std::runtime_error("plan has no sections");
    return p;
}

// Jump-physics limits in blocks (1 block = 30 units). Cube jump ~4.05 long, ~2.2 high at 1x.
struct Cfg { int maxGap, maxSpike, maxRise, minFlat, shipGap, waveGap; double ratio; };

inline Cfg difficultyCfg(const std::string& diff, const std::string& speed) {
    double ratio = O::findSpeed(speed)->ups / 311.58;
    double jumpLen = 4.05 * ratio;
    double f = diff == "easy" ? 0.6 : diff == "normal" ? 0.7 : diff == "hard" ? 0.8 : 0.85;
    int minFlat = diff == "easy" ? 5 : diff == "normal" ? 4 : diff == "hard" ? 3 : 2;
    Cfg c;
    c.maxGap = std::max(2, (int)(jumpLen * f));
    c.maxSpike = (diff == "easy" || diff == "normal") ? 2 : 3;
    c.maxRise = diff == "easy" ? 1 : 2;
    c.minFlat = std::max(2, (int)std::lround(minFlat * ratio));
    c.shipGap = diff == "easy" ? 6 : diff == "normal" ? 5 : 4;
    c.waveGap = diff == "easy" ? 5 : diff == "insane" ? 3 : 4;
    c.ratio = ratio;
    return c;
}

}  // namespace silicate
