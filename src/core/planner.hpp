// Gemini request/response handling (no networking here) and the offline planner.
#pragma once
#include <cctype>
#include <string>

#include "json.hpp"
#include "rng.hpp"
#include "spec.hpp"

namespace silicate {

inline std::string join(const std::vector<std::string>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) s += (i ? ", " : "") + ("\"" + v[i] + "\"");
    return s;
}

inline std::string systemPrompt() {
    std::string elems;
    for (auto& e : ELEMENTS)
        elems += std::string(elems.empty() ? "" : "; ") + e.name + "(" + e.param + " " + std::to_string(e.lo) + "-" + std::to_string(e.hi) + ")";
    return
        "You are a Geometry Dash level designer. You do NOT write object data; you write a high-level PLAN as JSON "
        "and a compiler builds the level, enforcing jump physics and decorating it.\n\n"
        "Output ONE JSON object:\n{\n  \"name\": string (<=20 chars),\n  \"theme\": one of " + join(THEME_NAMES) +
        ",\n  \"difficulty\": one of " + join(DIFFICULTIES) + ",\n  \"bpm\": integer 60-220,\n  \"sections\": [ {\n"
        "      \"gamemode\": one of " + join(GAMEMODES) + ",\n      \"speed\": one of \"0.5x\",\"1x\",\"2x\",\"3x\",\"4x\",\n"
        "      \"mood\": one of " + join(MOODS) + ",\n      \"bars\": integer 2-32 (4 beats each),\n"
        "      \"style\": one of " + join(STYLES) + " (cube sections only; chooses the structure family),\n"
        "      \"density\": number 0-1 (obstacles per bar; ship/wave: how twisty and hazardous),\n"
        "      \"elements\": OPTIONAL list for cube sections only, each {\"t\": name, <param>: int}\n  } ]\n}\n"
        "Cube element vocabulary (name: param range): " + elems + ".\n"
        "\"stairs\"/\"platform\"/\"up\"/\"down\" change floor height; \"gap\" is a pit on raised floor; "
        "\"pad_gap\"/\"orb_gap\" are jumps using yellow pads/orbs.\n\nDesign rules:\n"
        "- Structure the level like music: calm intro -> build -> drop -> break -> bigger drop -> calm outro. Vary mood per "
        "section; \"drop\" is the visual/energy peak.\n"
        "- Do not switch gamemode more often than every 6 bars. Put ship/wave/ball/ufo in the middle, start and end on cube "
        "unless asked otherwise.\n"
        "- Fast speeds (3x,4x) only in drops on hard/insane. Easy levels stay on 1x with low density.\n"
        "- Total 5-9 sections unless the user asks for a length. Prefer omitting \"elements\" and picking a style; only give "
        "elements to craft a specific motif.\n"
        "- Be creative with the name and theme to match the user's request. Return JSON only.";
}

inline std::string userPrompt(const std::string& prompt, const std::string& difficulty, const std::string& theme, int bars) {
    std::string u = "Design a level: " + (prompt.empty() ? std::string("surprise me") : prompt);
    std::string hints;
    if (!difficulty.empty()) hints += "difficulty: " + difficulty;
    if (!theme.empty()) hints += std::string(hints.empty() ? "" : "; ") + "theme: " + theme;
    if (bars > 0) hints += std::string(hints.empty() ? "" : "; ") + "about " + std::to_string(bars) + " bars in total";
    if (!hints.empty()) u += "\nConstraints: " + hints;
    return u;
}

// JSON body for POST .../models/{model}:generateContent
inline std::string requestBody(const std::string& user) {
    using json::Value;
    Value sys = Value::object();
    sys.set("parts", Value::array().push(Value::object().set("text", Value::str(systemPrompt()))));
    Value content = Value::object();
    content.set("role", Value::str("user"));
    content.set("parts", Value::array().push(Value::object().set("text", Value::str(user))));
    Value gen = Value::object();
    gen.set("temperature", Value::num(0.9));
    gen.set("responseMimeType", Value::str("application/json"));
    Value body = Value::object();
    body.set("systemInstruction", sys);
    body.set("contents", Value::array().push(content));
    body.set("generationConfig", gen);
    return json::dump(body);
}

inline std::string endpointFor(const std::string& model) {
    std::string m;
    for (char c : model) if (std::isalnum((unsigned char)c) || c == '-' || c == '.' || c == '_') m += c;
    return "https://generativelanguage.googleapis.com/v1beta/models/" + m + ":generateContent";
}

// Human-readable message for an HTTP error from Gemini.
inline std::string describeError(int code, const std::string& body) {
    std::string msg;
    try {
        auto j = json::parse(body);
        if (auto e = j.get("error")) msg = e->getStr("message");
    } catch (...) {}
    const char* hint = code == 400 ? " (check the API key and model name)"
                     : code == 403 ? " (key rejected or not allowed for this model)"
                     : code == 404 ? " (unknown model name)"
                     : code == 429 ? " (free-tier rate limit; wait a minute or pick another model)" : "";
    return "Gemini HTTP " + std::to_string(code) + ": " + msg + hint;
}

// Response body -> sanitised plan. Throws std::runtime_error with a readable message.
inline Plan parseResponse(const std::string& body, Rng& rng) {
    auto j = json::parse(body);
    const json::Value* cands = j.get("candidates");
    if (!cands || cands->a.empty()) {
        auto fb = j.get("promptFeedback");
        throw std::runtime_error(fb ? "Gemini blocked the prompt" : "Gemini returned no candidates");
    }
    const json::Value* content = cands->a[0].get("content");
    const json::Value* parts = content ? content->get("parts") : nullptr;
    std::string text;
    if (parts) for (auto& p : parts->a) {
        auto th = p.get("thought");
        if (th && th->type == json::Value::Bool && th->b) continue;
        text += p.getStr("text");
    }
    if (text.empty()) throw std::runtime_error("Gemini returned an empty reply");
    return sanitize(json::parseLoose(text), rng);
}

inline Plan offlinePlan(uint64_t seed, const std::string& difficulty, const std::string& theme, int bars) {
    Rng rng(seed);
    struct Beat { const char* mood; const char* mode; };
    const Beat arc[] = {{"calm", "cube"}, {"build", "cube"}, {"drop", "cube"}, {"break", "ship"},
                        {"build", "cube"}, {"drop", "wave"}, {"drop", "cube"}, {"calm", "cube"}};
    int per = std::max(4, bars / 8);
    json::Value secs = json::Value::array();
    for (auto& b : arc) {
        std::string mood = b.mood;
        double dens = mood == "calm" ? 0.3 : mood == "build" ? 0.5 : mood == "drop" ? 0.8 : 0.4;
        secs.push(json::Value::object()
                      .set("gamemode", json::Value::str(b.mode))
                      .set("speed", json::Value::str(mood == "drop" && (difficulty == "hard" || difficulty == "insane") ? "2x" : "1x"))
                      .set("mood", json::Value::str(mood)).set("bars", json::Value::num(per))
                      .set("style", json::Value::str(rng.choice(STYLES))).set("density", json::Value::num(dens)));
    }
    static const int bpms[] = {110, 128, 140, 150};
    json::Value plan = json::Value::object();
    plan.set("name", json::Value::str("Offline " + std::to_string(seed)));
    plan.set("theme", json::Value::str(theme.empty() ? rng.choice(THEME_NAMES) : theme));
    plan.set("difficulty", json::Value::str(difficulty));
    plan.set("bpm", json::Value::num(bpms[rng.randint(0, 3)]));
    plan.set("sections", secs);
    return sanitize(plan, rng);
}

}  // namespace silicate
