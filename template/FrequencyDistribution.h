
#pragma once

#include <cmath>

class FrequencyDistribution {
public:
    static constexpr int kBands = 7;
    static constexpr int kPresets = 6;

    // ========================================================
    // FREQUENCY CONFIGURATIONS
    // ========================================================

    // Each row represents one position along the knob.
    // Frequencies are in Hz.
    //
    // Resonators are deliberately allowed to cross.
    // A voice's identity remains tied to its pad,
    // not its position in the frequency spectrum.

    inline static constexpr float configurations
        [kPresets][kBands] = {

        // 0: LOW CLUSTER
        {
            55.f, 68.f, 82.f, 98.f,
            120.f, 155.f, 200.f
        },

        // 1: HARMONIC SERIES
        {
            65.f, 130.f, 195.f, 260.f,
            325.f, 390.f, 455.f
        },

        // 2: INHARMONIC / METALLIC
        {
            82.f, 231.f, 517.f, 136.f,
            1180.f, 349.f, 2150.f
        },

        // 3: MIDRANGE CLUSTER
        {
            410.f, 680.f, 520.f, 950.f,
            590.f, 780.f, 1150.f
        },

        // 4: SCATTERED SPECTRUM
        {
            3200.f, 85.f, 1700.f, 440.f,
            6100.f, 190.f, 920.f
        },

        // 5: FULL SPECTRUM
        {
            55.f, 1800.f, 240.f, 7900.f,
            920.f, 125.f, 13500.f
        }
    };

    // ========================================================
    // UTILITIES
    // ========================================================

    static float Clamp(float x, float lo, float hi) {
        return x < lo ? lo : (x > hi ? hi : x);
    }

    // ========================================================
    // FREQUENCY CALCULATION
    // ========================================================

    static void Calculate(float position, float* output) {

        position = Clamp(position, 0.f, 1.f);

        const float scaled =
            position * float(kPresets - 1);

        int first = int(scaled);

        if(first >= kPresets - 1) {
            first = kPresets - 2;
        }

        const int second = first + 1;

        const float fraction =
            scaled - float(first);

        // Smoothstep interpolation.
        // Creates smooth arrivals at configurations.
        const float blend =
            fraction * fraction *
            (3.f - 2.f * fraction);

        for(int i = 0; i < kBands; ++i) {

            const float a =
                configurations[first][i];

            const float b =
                configurations[second][i];

            // Logarithmic frequency interpolation.
            // Equal ratios produce equal perceived
            // pitch movement.
            const float logA = logf(a);
            const float logB = logf(b);

            output[i] = expf(
                logA + blend * (logB - logA)
            );
        }
    }
};
