// Geode/GD object IDs and property keys. If an ID is wrong for your game version, fix it here only.
#pragma once
#include <string>

namespace silicate::O {

constexpr int BLOCK = 1, SPIKE = 8, SPIKE_SMALL = 39;
constexpr int PORTAL_CUBE = 12, PORTAL_SHIP = 13, PORTAL_BALL = 47, PORTAL_UFO = 111, PORTAL_WAVE = 660;
constexpr int PAD_YELLOW = 35, ORB_YELLOW = 36, COIN = 1329;
constexpr int COLOR_TRIGGER = 899, PULSE_TRIGGER = 1006, END_TRIGGER = 3600;

constexpr int K_ROT = 6, K_R = 7, K_G = 8, K_B = 9, K_DUR = 10;
constexpr int K_MAIN_COLOR = 21, K_TARGET_COLOR = 23, K_ZLAYER = 24, K_ZORDER = 25, K_SCALE = 32, K_OPACITY = 35;
constexpr int K_PULSE_IN = 45, K_PULSE_HOLD = 46, K_PULSE_OUT = 47, K_PULSE_MODE = 48;
constexpr int K_PULSE_TARGET = 51, K_PULSE_TARGET_TYPE = 52;
constexpr int K_HIGH_DETAIL = 103, K_NO_TOUCH = 121;

constexpr int Z_B4 = -3, Z_B3 = -1, Z_B2 = 1, Z_B1 = 3;

constexpr int CH_BG = 1000, CH_GROUND = 1001, CH_LINE = 1002, CH_OBJ = 1004;
constexpr int CH_FILL = 1, CH_FILL2 = 2, CH_DEC1 = 3, CH_DEC2 = 4, CH_EDGE = 5;

struct Speed { const char* name; int portal; double ups; int header; };
constexpr Speed SPEEDS[] = {
    {"0.5x", 200, 251.16, 1}, {"1x", 201, 311.58, 0}, {"2x", 202, 387.42, 2},
    {"3x", 203, 468.0, 3}, {"4x", 1334, 576.0, 4},
};
inline const Speed* findSpeed(const std::string& n) {
    for (auto& s : SPEEDS) if (n == s.name) return &s;
    return nullptr;
}
inline int portalFor(const std::string& mode) {
    if (mode == "ship") return PORTAL_SHIP;
    if (mode == "ball") return PORTAL_BALL;
    if (mode == "ufo") return PORTAL_UFO;
    if (mode == "wave") return PORTAL_WAVE;
    return PORTAL_CUBE;
}

}  // namespace silicate::O
