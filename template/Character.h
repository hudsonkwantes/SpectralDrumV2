
#pragma once

#include "FrequencyDistribution.h"
#include <cmath>

class Character {
public:
    static constexpr int kVoices = 7;
    static constexpr int kScenes = 6;

    struct Weights {
        float body = 0.f;
        float rattle = 0.f;
        float crush = 0.f;
    };

    struct Scene {
        float body;
        float rattle;
        float crush;

        float bodyHz;
        float rattleHz;
        float crushHz;
    };

    // ========================================================
    // CHARACTER SCENES
    // ========================================================

    inline static constexpr Scene scenes[kScenes] = {

        // CLEAN
        {
            0.f, 0.f, 0.f,
            160.f, 1000.f, 3500.f
        },

        // BODY / WOOD
        {
            1.0f, 0.08f, 0.f,
            135.f, 850.f, 3000.f
        },

        // RATTLE
        {
            0.25f, 1.0f, 0.08f,
            200.f, 950.f, 3000.f
        },

        // INDUSTRIAL
        {
            0.10f, 0.65f, 0.75f,
            300.f, 1400.f, 2800.f
        },

        // CRUSH
        {
            0.f, 0.15f, 1.0f,
            250.f, 1800.f, 4800.f
        },

        // COMPLEX
        {
            0.65f, 0.75f, 0.90f,
            180.f, 1100.f, 4000.f
        }
    };

    // ========================================================
    // CONFIGURATION
    // ========================================================

    static constexpr float kBodyWidth = 1.50f;
    static constexpr float kRattleWidth = 1.25f;
    static constexpr float kCrushWidth = 1.50f;

    static constexpr int kMaxAffected = 3;

    Weights voices[kVoices]{};

    float position = 0.f;

    // ========================================================
    // UTILITIES
    // ========================================================

    static float Clamp(float x, float lo, float hi) {
        return x < lo ? lo : (x > hi ? hi : x);
    }

    static float Lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    static float LogLerp(float a, float b, float t) {
        return expf(
            Lerp(logf(a), logf(b), t)
        );
    }

    // ========================================================
    // SPECTRAL EMPHASIS
    // ========================================================

    static float SpectralWeight(
        float frequency,
        float centre,
        float width
    ) {

        frequency = Clamp(
            frequency,
            20.f,
            18000.f
        );

        centre = Clamp(
            centre,
            20.f,
            18000.f
        );

        const float octaveDistance =
            log2f(frequency / centre);

        const float normalized =
            octaveDistance / width;

        return expf(
            -0.5f * normalized * normalized
        );
    }

    // ========================================================
    // NARROW DISTRIBUTION
    // ========================================================

    static void Isolate(float* weights) {

        float original[kVoices];

        for(int i = 0; i < kVoices; ++i)
            original[i] = weights[i];

        float maximum = 0.f;

        for(int i = 0; i < kVoices; ++i) {

            if(original[i] > maximum)
                maximum = original[i];
        }

        if(maximum <= 0.f) {

            for(int i = 0; i < kVoices; ++i)
                weights[i] = 0.f;

            return;
        }

        // Select three strongest voices
        bool selected[kVoices]{};

        for(int n = 0; n < kMaxAffected; ++n) {

            int best = -1;
            float bestValue = -1.f;

            for(int i = 0; i < kVoices; ++i) {

                if(!selected[i] &&
                   original[i] > bestValue) {

                    bestValue = original[i];
                    best = i;
                }
            }

            if(best >= 0)
                selected[best] = true;
        }

        // Normalize selected weights
        for(int i = 0; i < kVoices; ++i) {

            weights[i] = selected[i]
                ? original[i] / maximum
                : 0.f;
        }
    }

    // ========================================================
    // CONTROL
    // ========================================================

    void SetPosition(float value) {
        position = Clamp(value, 0.f, 1.f);
    }

    // ========================================================
    // UPDATE
    // ========================================================

    void Update(float distributionPosition) {

        const float x =
            Clamp(position, 0.f, 1.f);

        // ----------------------------------------------------
        // CLEAN
        // ----------------------------------------------------

        if(x <= 0.f) {

            for(auto& voice : voices)
                voice = Weights{};

            return;
        }

        // ----------------------------------------------------
        // SCENE INTERPOLATION
        // ----------------------------------------------------

        const float scaled =
            x * float(kScenes - 1);

        int first = int(scaled);

        if(first >= kScenes - 1)
            first = kScenes - 2;

        const int second = first + 1;

        const float fraction =
            scaled - float(first);

        const float blend =
            fraction * fraction *
            (3.f - 2.f * fraction);

        const Scene& a = scenes[first];
        const Scene& b = scenes[second];

        const float bodyDepth =
            Lerp(a.body, b.body, blend);

        const float rattleDepth =
            Lerp(a.rattle, b.rattle, blend);

        const float crushDepth =
            Lerp(a.crush, b.crush, blend);

        const float bodyCentre =
            LogLerp(a.bodyHz, b.bodyHz, blend);

        const float rattleCentre =
            LogLerp(a.rattleHz, b.rattleHz, blend);

        const float crushCentre =
            LogLerp(a.crushHz, b.crushHz, blend);

        // ----------------------------------------------------
        // FREQUENCY DISTRIBUTION
        // ----------------------------------------------------

        float frequencies[kVoices];

        FrequencyDistribution::Calculate(
            distributionPosition,
            frequencies
        );

        // ----------------------------------------------------
        // RAW WEIGHTS
        // ----------------------------------------------------

        float body[kVoices];
        float rattle[kVoices];
        float crush[kVoices];

        for(int i = 0; i < kVoices; ++i) {

            body[i] = SpectralWeight(
                frequencies[i],
                bodyCentre,
                kBodyWidth
            );

            rattle[i] = SpectralWeight(
                frequencies[i],
                rattleCentre,
                kRattleWidth
            );

            crush[i] = SpectralWeight(
                frequencies[i],
                crushCentre,
                kCrushWidth
            );
        }

        // ----------------------------------------------------
        // ISOLATE EFFECT REGIONS
        // ----------------------------------------------------

        Isolate(body);
        Isolate(rattle);
        Isolate(crush);

        // ----------------------------------------------------
        // APPLY DEPTHS
        // ----------------------------------------------------

        for(int i = 0; i < kVoices; ++i) {

            voices[i].body = Clamp(
                bodyDepth * body[i],
                0.f,
                1.f
            );

            voices[i].rattle = Clamp(
                rattleDepth * rattle[i],
                0.f,
                1.f
            );

            voices[i].crush = Clamp(
                crushDepth * crush[i],
                0.f,
                1.f
            );
        }
    }

    // ========================================================
    // GET WEIGHTS
    // ========================================================

    const Weights& Get(int voice) const {
        return voices[voice];
    }
};
