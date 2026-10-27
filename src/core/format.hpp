// Level string assembly. Output is the *uncompressed* "header;obj;obj;" text;
// the Geode layer compresses it with GD's own routine (gzip + base64url).
#pragma once
#include <array>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace silicate {

using RGB = std::array<int, 3>;

struct Obj {
    int id = 0;
    double x = 0, y = 0;
    std::map<int, double> props;
};

inline std::string fmtNum(double v) {
    char b[40];
    std::snprintf(b, sizeof b, "%.3f", v);
    std::string s = b;
    if (s.find('.') != std::string::npos) {
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
    }
    return s;
}

inline std::string encodeObj(const Obj& o) {
    std::string s = "1," + std::to_string(o.id) + ",2," + fmtNum(o.x) + ",3," + fmtNum(o.y);
    for (auto& kv : o.props) s += "," + std::to_string(kv.first) + "," + fmtNum(kv.second);
    return s + ";";
}

inline std::string buildHeader(const std::map<int, RGB>& channels, int startSpeed, int bg, int ground) {
    std::string colors;
    for (auto& kv : channels) {
        colors += "1_" + std::to_string(kv.second[0]) + "_2_" + std::to_string(kv.second[1]) + "_3_" +
                  std::to_string(kv.second[2]) + "_11_255_12_255_13_255_4_-1_6_" + std::to_string(kv.first) +
                  "_7_1_15_1_18_0_8_1|";
    }
    return "kS38," + colors + ",kA13,0,kA15,0,kA16,0,kA14,,kA6," + std::to_string(bg) + ",kA7," +
           std::to_string(ground) + ",kA25,0,kA17,0,kA18,0,kS39,0,kA2,0,kA3,0,kA8,0,kA4," +
           std::to_string(startSpeed) +
           ",kA9,0,kA10,0,kA22,0,kA23,0,kA24,0,kA27,1,kA40,1,kA41,1,kA42,1,kA28,0,kA29,0,kA31,1,"
           "kA32,1,kA36,0,kA43,0,kA44,0,kA45,1,kA46,0,kA33,1,kA34,1,kA35,0,kA37,1,kA38,1,kA39,1,"
           "kA19,0,kA26,0,kA20,0,kA21,0,kA11,0;";
}

inline std::string rawLevelString(const std::string& header, const std::vector<Obj>& objs) {
    std::string s = header;
    for (auto& o : objs) s += encodeObj(o);
    return s;
}

}  // namespace silicate
