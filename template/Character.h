
#pragma once

#include <cmath>

class Character {
public:
    static constexpr int kVoices = 7;

    // Narrow spectral emphasis
    static constexpr float kWidth = 1.7f;

    struct Weights {
        float body = 0.f;
        float rattle = 0.f;
        float shimmer = 0.f;
    };

    Weights voices[kVoices]{};

    float position = 0.f;

    // ========================================================
    // UTILITIES
    // ========================================================

    static float Clamp(float x, float lo, float hi) {
        return x < lo ? lo : (x > hi ? hi : x);
    }

    // ========================================================
    // EMPHASIS
    // ========================================================

    static float Emphasis(
        float voice,
        float centre
    ) {
        const float distance =
            fabsf(voice - centre);

        const float taper = Clamp(
            1.f - distance / kWidth,
            0.f,
            1.f
        );

        // Narrow, smooth weighting
        return taper * taper;
    }

    // ========================================================
    // CONTROL
    // ========================================================

    void SetPosition(float value) {
        position = Clamp(value, 0.f, 1.f);
    }

    // ========================================================
    // UPDATE DISTRIBUTIONS
    // ========================================================

    void Update() {

        const float x = position;

        // Modulation depth reaches maximum
        // relatively early in the knob rotation.
        // The remaining travel moves the emphasis.
        const float depth =
            Clamp(x / 0.30f, 0.f, 1.f);

        // ----------------------------------------------------
        // CHARACTER POSITIONS
        // ----------------------------------------------------

        // BODY:
        // Travels from high-numbered voices
        // toward low-numbered voices.
        const float bodyCentre =
            6.f * (1.f - x);

        // RATTLE:
        // Moves outward and returns.
        const float rattleCentre =
            6.f * fabsf(
                sinf(3.14159265359f * x)
            );

        // SHIMMER:
        // Travels from low-numbered voices
        // toward high-numbered voices.
        const float shimmerCentre =
            6.f * x;

        // ----------------------------------------------------
        // DISTRIBUTE CHARACTER
        // ----------------------------------------------------

        for(int i = 0; i < kVoices; ++i) {

            const float voice = float(i);

            voices[i].body =
                depth *
                Emphasis(
                    voice,
                    bodyCentre
                );

            voices[i].rattle =
                depth *
                Emphasis(
                    voice,
                    rattleCentre
                );

            voices[i].shimmer =
                depth *
                Emphasis(
                    voice,
                    shimmerCentre
                );
        }

        // Fully CCW means absolutely no modulation.
        if(x <= 0.f) {

            for(auto& v : voices) {
                v.body = 0.f;
                v.rattle = 0.f;
                v.shimmer = 0.f;
            }
        }
    }

    // ========================================================
    // GET WEIGHTS
    // ========================================================

    const Weights& Get(int voice) const {
        return voices[voice];
    }
};
