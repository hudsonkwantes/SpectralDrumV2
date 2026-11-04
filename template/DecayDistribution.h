
#pragma once

#include <cstdint>

class DecayDistribution {
public:
    static constexpr int kVoices = 7;
    static constexpr int kSteps = 16;

    // ========================================================
    // CONFIGURATION
    // ========================================================

    // Minimum decay relative to baseline.
    //
    // 0.06 = shortest hits can have 6%
    // of the baseline decay time.
    static constexpr float kMinMultiplier = 0.06f;

    // ========================================================
    // UTILITIES
    // ========================================================

    static float Clamp(float x, float lo, float hi) {
        return x < lo ? lo : (x > hi ? hi : x);
    }

    // ========================================================
    // DETERMINISTIC STEP HASH
    // ========================================================

    // Produces a repeatable pseudo-random value
    // for each voice and step combination.
    //
    // No changing random values between loops.

    static uint32_t Hash(int voice, int step) {

        uint32_t x =
            0x5EED2026u ^
            (uint32_t(voice + 1) * 0x9E3779B9u) ^
            (uint32_t(step + 1) * 0x85EBCA6Bu);

        x ^= x >> 16;
        x *= 0x7FEB352Du;
        x ^= x >> 15;
        x *= 0x846CA68Bu;
        x ^= x >> 16;

        return x;
    }

    // ========================================================
    // DECAY VARIATION
    // ========================================================

    // voice: 0-6
    // step:  0-15
    // depth: 0-1
    //
    // DEPTH = 0:
    // Every triggered step uses the baseline decay.
    //
    // DEPTH = 1:
    // Individual steps receive different decay times.
    //
    // Each voice has an independent variation pattern.
    //
    // The patterns are deterministic and repeatable.
    //
    // No decay can exceed the baseline set by S31.

    static float Calculate(
        int voice,
        int step,
        float depth
    ) {

        depth = Clamp(depth, 0.f, 1.f);

        // ----------------------------------------------------
        // NO MODULATION
        // ----------------------------------------------------

        if(depth <= 0.f)
            return 1.f;

        // ----------------------------------------------------
        // VALIDATE INPUTS
        // ----------------------------------------------------

        if(voice < 0 || voice >= kVoices)
            return 1.f;

        if(step < 0)
            return 1.f;

        step %= kSteps;

        // ----------------------------------------------------
        // PRESERVED ACCENTS
        // ----------------------------------------------------

        // Each voice retains one full-length decay
        // every four steps.
        //
        // Accent positions differ between voices.

        const int accentStep =
            (voice * 3) % 4;

        if(step % 4 == accentStep)
            return 1.f;

        // ----------------------------------------------------
        // STEP VARIATION
        // ----------------------------------------------------

        const uint32_t hash =
            Hash(voice, step);

        const float randomValue =
            float(hash & 0xFFFFu) / 65535.f;

        // Squared distribution creates more
        // pronounced differences in decay.
        const float shaped =
            randomValue * randomValue;

        // Minimum possible multiplier is 0.06
        // at maximum modulation depth.
        const float minimum =
            kMinMultiplier +
            (1.f - kMinMultiplier) * shaped;

        // ----------------------------------------------------
        // DEPTH CONTROL
        // ----------------------------------------------------

        // Interpolate from uniform decay
        // toward the step's individual multiplier.
        const float multiplier =
            1.f + depth * (minimum - 1.f);

        return Clamp(
            multiplier,
            kMinMultiplier,
            1.f
        );
    }
};
