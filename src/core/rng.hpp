#pragma once
#include <cstdint>
#include <vector>

namespace silicate {

// xorshift64*: small, deterministic across platforms (unlike std distributions).
class Rng {
public:
    explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull) : s_(seed ? seed * 0x2545F4914F6CDD1Dull + 1 : 1) { next(); next(); }
    uint64_t next() {
        s_ ^= s_ >> 12; s_ ^= s_ << 25; s_ ^= s_ >> 27;
        return s_ * 0x2545F4914F6CDD1Dull;
    }
    double random() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
    int randint(int lo, int hi) {  // inclusive
        if (hi <= lo) return lo;
        return lo + (int)(next() % (uint64_t)(hi - lo + 1));
    }
    template <class T> const T& choice(const std::vector<T>& v) { return v[(size_t)randint(0, (int)v.size() - 1)]; }
private:
    uint64_t s_;
};

}  // namespace silicate
