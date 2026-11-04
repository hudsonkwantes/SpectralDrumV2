
#pragma once

#include <cstdint>
#include <cmath>

// ============================================================
// SEQUENCER
// ============================================================

class Sequencer {
public:

    static constexpr int kBands = 7;
    static constexpr int kSteps = 16;

    // Stored source patterns
    uint16_t source[kBands]{};

    // Global sequencer position
    uint32_t tick = 0;

    // ========================================================
    // INITIALIZATION
    // ========================================================

    void Init() {

        uint32_t rng = 0x5EED2026u;

        for(int b = 0; b < kBands; ++b) {

            source[b] = 0;

            for(int s = 0; s < kSteps; ++s) {

                rng ^= rng << 13;
                rng ^= rng >> 17;
                rng ^= rng << 5;

                if((rng % 100u) < 28u) {

                    source[b] |=
                        uint16_t(1u << s);
                }
            }
        }

        tick = 0;
    }

    // ========================================================
    // UTILITIES
    // ========================================================

    static int Wrap(int n, int mod) {

        n %= mod;

        return n < 0 ? n + mod : n;
    }

    // ========================================================
    // TRANSPORT
    // ========================================================

    void Reset() {
        tick = 0;
    }

    // ========================================================
    // CLEAR
    // ========================================================

    // Completely erase one voice's stored pattern.
    // This does not change the global clock,
    // other voices, or sequencer transformations.

    void ClearVoice(int voice) {

        if(voice < 0 || voice >= kBands)
            return;

        source[voice] = 0;
    }

    // ========================================================
    // LOOP LENGTH
    // ========================================================

    static int LengthForBand(
        int band,
        int globalLength,
        float tilt
    ) {

        if(globalLength <= 0)
            return 0;

        const float amount = fabsf(tilt);

        const float normalized =
            tilt < 0.f
                ? float(band) / 6.f
                : float(6 - band) / 6.f;

        const float len =
            float(globalLength) -
            (float(globalLength) - 1.f) *
            amount * normalized;

        int rounded = int(len + 0.5f);

        if(rounded < 1)
            rounded = 1;

        if(rounded > kSteps)
            rounded = kSteps;

        return rounded;
    }

    // ========================================================
    // RECORD
    // ========================================================

    // Add hits to stored source patterns.
    // Recording respects the current loop-length
    // and sequencing transformations.

    void Record(
        uint8_t padEvents,
        int globalLength,
        float tilt,
        int xShift,
        int yShift
    ) {

        for(int target = 0;
            target < kBands;
            ++target) {

            if(!(padEvents & (1u << target)))
                continue;

            const int length =
                LengthForBand(
                    target,
                    globalLength,
                    tilt
                );

            if(length <= 0)
                continue;

            const int sourceBand =
                Wrap(target - yShift, kBands);

            const int localStep =
                int(tick % uint32_t(length));

            const int sourceStep =
                Wrap(localStep - xShift, kSteps);

            source[sourceBand] |=
                uint16_t(1u << sourceStep);
        }
    }

    // ========================================================
    // ADVANCE
    // ========================================================

    uint8_t Advance(
        int globalLength,
        float tilt,
        int xShift,
        int yShift
    ) {

        uint8_t fired = 0;

        for(int target = 0;
            target < kBands;
            ++target) {

            const int length =
                LengthForBand(
                    target,
                    globalLength,
                    tilt
                );

            if(length <= 0)
                continue;

            const int sourceBand =
                Wrap(target - yShift, kBands);

            const int localStep =
                int(tick % uint32_t(length));

            const int sourceStep =
                Wrap(localStep - xShift, kSteps);

            if(source[sourceBand] &
               uint16_t(1u << sourceStep)) {

                fired |=
                    uint8_t(1u << target);
            }
        }

        ++tick;

        return fired;
    }
};
