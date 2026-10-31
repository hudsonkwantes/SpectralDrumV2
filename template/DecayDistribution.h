
#pragma once

#include <cmath>

class DecayDistribution {
public:
    static constexpr int kVoices = 7;

    // ========================================================
    // CONFIGURATION
    // ========================================================

    // Maximum reduction:
    // 0.97 = decay can be reduced to 3% of baseline.
    static constexpr float kMaxReduction = 0.97f;

    // Width of the moving emphasis region.
    static constexpr float kWidth = 3.0f;

    // Minimum decay multiplier
    static constexpr float kMinMultiplier = 0.03f;

    // ========================================================
    // UTILITIES
    // ========================================================

    static float Clamp(float x, float lo, float hi) {
        return x < lo ? lo : (x > hi ? hi : x);
    }

    // ========================================================
    // EMPHASIS FUNCTION
    // ========================================================

    static float Emphasis(float voice, float centre) {

        const float distance =
            fabsf(voice - centre);

        // Linear taper for stronger influence
        // across neighbouring voices.
        return Clamp(
            1.f - distance / kWidth,
            0.f,
            1.f
        );
    }

    // ========================================================
    // DECAY DISTRIBUTION
    // ========================================================

    // Slider range: -1 to +1
    //
    // CENTRE (0):
    // All voices share the baseline decay.
    //
    // UP (+):
    // Moving notch of SHORTER decay times.
    // Voices outside the notch retain baseline.
    //
    // DOWN (-):
    // Moving peak of LONGER decay times.
    // Voices outside the peak become shorter.
    //
    // The slider never increases decay beyond
    // the baseline established by S31.

    static void Calculate(
        float slider,
        float* output
    ) {

        slider = Clamp(slider, -1.f, 1.f);

        // ----------------------------------------------------
        // DEPTH
        // ----------------------------------------------------

        const float depth =
            fabsf(slider);

        // ----------------------------------------------------
        // CENTRE / NEUTRAL
        // ----------------------------------------------------

        if(depth < 0.001f) {

            for(int i = 0; i < kVoices; ++i) {
                output[i] = 1.f;
            }

            return;
        }

        // ----------------------------------------------------
        // MOVING EMPHASIS POSITION
        // ----------------------------------------------------

        // As the slider moves away from centre,
        // emphasis travels from Band 1 to Band 7.
        const float centre =
            6.f * depth;

        // ----------------------------------------------------
        // CALCULATE VOICE WEIGHTS
        // ----------------------------------------------------

        for(int i = 0; i < kVoices; ++i) {

            const float emphasis =
                Emphasis(
                    float(i),
                    centre
                );

            float reduction = 0.f;

            // ------------------------------------------------
            // UP: SHORT-DECAY NOTCH
            // ------------------------------------------------

            if(slider > 0.f) {

                reduction =
                    kMaxReduction *
                    depth *
                    emphasis;
            }

            // ------------------------------------------------
            // DOWN: LONG-DECAY PEAK
            // ------------------------------------------------

            else {

                reduction =
                    kMaxReduction *
                    depth *
                    (1.f - emphasis);
            }

            // ------------------------------------------------
            // FINAL DECAY MULTIPLIER
            // ------------------------------------------------

            output[i] = Clamp(
                1.f - reduction,
                kMinMultiplier,
                1.f
            );
        }
    }
};
