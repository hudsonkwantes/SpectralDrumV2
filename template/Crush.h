
#pragma once

#include <cmath>

class Crush {
public:

    // ========================================================
    // CONFIGURATION
    // ========================================================

    // Bit-depth range
    static constexpr int kMaxBits = 16;
    static constexpr int kMinBits = 9;

    // Sample-rate reduction
    static constexpr int kMaxHold = 18;

    // Maximum wet mix
    static constexpr float kMaxWet = 1.0f;

    // ========================================================
    // STATE
    // ========================================================

    float heldSample = 0.f;
    int holdCounter = 0;

    // ========================================================
    // UTILITIES
    // ========================================================

    static float Clamp(float x, float lo, float hi) {
        return x < lo ? lo : (x > hi ? hi : x);
    }

    // ========================================================
    // INITIALIZATION
    // ========================================================

    void Init() {

        heldSample = 0.f;
        holdCounter = 0;
    }

    // ========================================================
    // PROCESS
    // ========================================================

    float Process(float input, float depth) {

        depth = Clamp(depth, 0.f, 1.f);

        // Completely clean at zero
        if(depth <= 0.f) {

            heldSample = input;
            holdCounter = 0;

            return input;
        }

        // ----------------------------------------------------
        // CONTROL CURVE
        // ----------------------------------------------------

        // Gentle initial response, increasingly
        // aggressive toward the maximum.
        const float shaped = depth * kMaxWet;

        // ----------------------------------------------------
        // BIT DEPTH
        // ----------------------------------------------------

        const float bitValue =
            float(kMaxBits) -
            shaped *
            float(kMaxBits - kMinBits);

        int bits = int(bitValue + 0.5f);

        if(bits < kMinBits)
            bits = kMinBits;

        if(bits > kMaxBits)
            bits = kMaxBits;

        const int levels =
            1 << (bits - 1);

        // ----------------------------------------------------
        // SAMPLE-RATE REDUCTION
        // ----------------------------------------------------

        const float holdValue =
            1.f +
            shaped * float(kMaxHold - 1);

        int holdPeriod =
            int(holdValue + 0.5f);

        if(holdPeriod < 1)
            holdPeriod = 1;

        if(holdPeriod > kMaxHold)
            holdPeriod = kMaxHold;

        // ----------------------------------------------------
        // SAMPLE AND HOLD
        // ----------------------------------------------------

        if(holdCounter <= 0) {

            const float clipped =
                Clamp(input, -1.f, 1.f);

            heldSample =
                roundf(
                    clipped * float(levels)
                ) / float(levels);

            holdCounter = holdPeriod;
        }

        --holdCounter;

        // ----------------------------------------------------
        // DRY / WET
        // ----------------------------------------------------

        const float wet =
            depth * kMaxWet;

        return input * (1.f - wet) +
               heldSample * wet;
    }
};
