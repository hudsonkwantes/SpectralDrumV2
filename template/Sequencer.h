#pragma once
#include <cstdint>
#include <cmath>

// Immutable 7 x 16 source pattern. Seed is fixed for repeatability on every boot.
class Sequencer {
public:
    static constexpr int kBands = 7;
    static constexpr int kSteps = 16;
    uint16_t source[kBands]{};
    uint32_t tick = 0;

    void Init() {
        uint32_t rng = 0x5EED2026u;
        for(int b = 0; b < kBands; ++b) {
            for(int s = 0; s < kSteps; ++s) {
                rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
                if((rng % 100u) < 28u) source[b] |= uint16_t(1u << s);
            }
        }
        tick = 0;
    }
    static int Wrap(int n, int mod) { n %= mod; return n < 0 ? n + mod : n; }
    // Tilt: -1 = band 1 short, +1 = band 7 short.
    static int LengthForBand(int b, int globalLength, float tilt) {
        if(globalLength <= 0) return 0;
        const float amount = fabsf(tilt);
        const float normalized = tilt < 0.f ? float(b) / 6.f : float(6-b) / 6.f;
        const float len = float(globalLength) - (float(globalLength)-1.f) * amount * normalized;
        int rounded = int(len + 0.5f);
        return rounded < 1 ? 1 : (rounded > 16 ? 16 : rounded);
    }
    // Called once per global 16th-note tick; this does NOT change source[]
    uint8_t Advance(int globalLength, float tilt, int xShift, int yShift) {
        uint8_t fired = 0;
        for(int target = 0; target < kBands; ++target) {
            const int len = LengthForBand(target, globalLength, tilt);
            if(len == 0) continue;
            const int sourceBand = Wrap(target - yShift, kBands);
            const int localStep = int(tick % uint32_t(len));
            const int sourceStep = Wrap(localStep - xShift, kSteps);
            if(source[sourceBand] & uint16_t(1u << sourceStep)) fired |= uint8_t(1u << target);
        }
        ++tick;
        return fired;
    }
};
