// Native tests for the dependency-free generator core. Build: see tests/run.sh
#include <cassert>
#include <cstdio>
#include <set>
#include <tuple>

#include "../src/core/builder.hpp"
#include "../src/core/planner.hpp"

using namespace silicate;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static Level build(uint64_t seed, const char* diff) {
    return compileLevel(offlinePlan(seed, diff, "neon", 96), seed);
}

int main() {
    // json
    auto j = json::parse(R"({"a":[1,2.5,"x\né"],"b":{"c":true}})");
    CHECK(j.get("a")->a[1].n == 2.5);
    CHECK(j.get("a")->a[2].s == "x\n\xc3\xa9");
    CHECK(json::parseLoose("```json\n{\"k\":1}\n```").getNum("k") == 1);
    CHECK(json::parse(json::dump(j)).get("b")->get("c")->b);
    bool threw = false;
    try { json::parse("{\"a\":"); } catch (...) { threw = true; }
    CHECK(threw);

    // determinism + header
    CHECK(build(5, "hard").raw() == build(5, "hard").raw());
    CHECK(build(5, "hard").raw() != build(6, "hard").raw());
    CHECK(build(5, "hard").raw().rfind("kS38,", 0) == 0);

    // solid decoration never inside the playable envelope; no overlapping solids
    for (uint64_t seed = 0; seed < 20; ++seed) {
        for (const char* d : {"easy", "hard", "insane"}) {
            Level lv = build(seed, d);
            std::set<std::tuple<double, double, bool>> seen;
            for (auto& o : lv.objs) {
                int col = (int)(o.x / 30), row = (int)(o.y / 30);
                if (o.props.count(O::K_NO_TOUCH)) {
                    CHECK(lv.world.reach.count(col) && row > lv.world.reach.at(col));
                } else if ((o.id == O::BLOCK && !o.props.count(O::K_SCALE)) || o.id == O::SPIKE || o.id == O::SPIKE_SMALL) {
                    bool dup = !seen.insert({o.x, o.y, o.id == O::BLOCK}).second;
                    CHECK(!dup);
                }
            }
        }
    }

    // every gamemode builds and the level returns to the ground
    json::Value sec = json::Value::array();
    for (const char* m : {"cube", "ship", "wave", "ball", "ufo", "cube"})
        sec.push(json::Value::object().set("gamemode", json::Value::str(m)).set("bars", json::Value::num(6)).set("density", json::Value::num(0.9)));
    Rng rng(1);
    Plan p = sanitize(json::Value::object().set("difficulty", json::Value::str("insane")).set("sections", sec), rng);
    Level lv = compileLevel(p, 2);
    std::set<int> ids;
    for (auto& o : lv.objs) ids.insert(o.id);
    for (int id : {13, 660, 47, 111}) CHECK(ids.count(id));
    CHECK(lv.world.floorH.rbegin()->second == 0);

    // sanitize clamps garbage
    auto bad = json::parse(R"({"bpm":9999,"theme":"x","sections":[{"gamemode":"robot","speed":"9x","bars":999,
        "elements":[{"t":"gap","w":99},{"t":"nuke"}]}]})");
    Plan q = sanitize(bad, rng);
    CHECK(q.bpm == 220 && q.sections[0].gamemode == "cube" && q.sections[0].speed == "1x" && q.sections[0].bars == 32);
    CHECK(q.sections[0].elements.size() == 1 && q.sections[0].elements[0].v == 6);

    // gaps never exceed the jump limit
    Plan g = sanitize(json::parse(R"({"difficulty":"normal","sections":[{"bars":4,"elements":[{"t":"up","dh":2},{"t":"gap","w":6}]}]})"), rng);
    Level gl = compileLevel(g, 1);
    int pit = 0;
    for (auto& o : gl.objs) if (o.id == O::SPIKE && o.y == 15) ++pit;
    CHECK(pit <= difficultyCfg("normal", "1x").maxGap);

    // gemini request/response
    CHECK(json::parse(requestBody("hi")).get("generationConfig")->getStr("responseMimeType") == "application/json");
    std::string reply = R"({"candidates":[{"content":{"parts":[{"text":"```json\n{\"name\":\"Mock\",\"theme\":\"ice\",\"sections\":[{\"mood\":\"calm\"}]}\n```"}]}}]})";
    Plan m = parseResponse(reply, rng);
    CHECK(m.name == "Mock" && m.theme == "ice");
    threw = false;
    try { parseResponse(R"({"candidates":[]})", rng); } catch (const std::exception&) { threw = true; }
    CHECK(threw);
    CHECK(describeError(429, R"({"error":{"message":"quota"}})").find("rate limit") != std::string::npos);
    CHECK(endpointFor("a/../b:c").find('/', 50) == std::string::npos || endpointFor("a/b").find("models/ab:") != std::string::npos);

    std::printf(failures ? "%d FAILED\n" : "all ok\n", failures);
    return failures != 0;
}
