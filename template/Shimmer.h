
#pragma once

#include <cmath>
#include <cstdint>

class Shimmer {
public:
    static constexpr int kDelayA = 1087;
    static constexpr int kDelayB = 1233;

    // Feedback level
    static constexpr float kFeedback = 0.58f;

    // Cross-coupling between delay lines
    static constexpr float kCrossfeed = 0.14f;

    // Wet output level
    static constexpr float kWetLevel = 0.32f;

    // Input high-frequency emphasis
    static constexpr float kBrightness = 0.65f;

    float delayA[kDelayA]{};
    float delayB[kDelayB]{};

    int indexA = 0;
    int indexB = 0;

    float inputLowpass = 0.f;
    float lowpassCoefficient = 0.15f;

    // ========================================================
    // UTILITIES
    // ========================================================

    static float Clamp(float x, float lo, float hi) {
        return x < lo ? lo : (x > hi ? hi : x);
    }

    static float Soft(float x) {
        return x / (1.f + fabsf(x));
    }

    // ========================================================
    // INITIALIZATION
    // ========================================================

    void Init(float sampleRate) {

        indexA = 0;
        indexB = 0;

        inputLowpass = 0.f;

        // Approximate 1.5 kHz one-pole low-pass
        const float cutoff = 1500.f;

        lowpassCoefficient =
            1.f - expf(
                -2.f * 3.14159265359f *
                cutoff / sampleRate
            );

        for(int i = 0; i < kDelayA; ++i)
            delayA[i] = 0.f;

        for(int i = 0; i < kDelayB; ++i)
            delayB[i] = 0.f;
    }

    // ========================================================
    // PROCESS
    // ========================================================

    // input: signal from resonator
    // weight: 0-1 character modulation depth

    float Process(float input, float weight) {

        weight = Clamp(weight, 0.f, 1.f);

        // ----------------------------------------------------
        // BRIGHTNESS
        // ----------------------------------------------------

        inputLowpass +=
            lowpassCoefficient *
            (input - inputLowpass);

        const float highpass =
            input - inputLowpass;

        // Input becomes brighter as shimmer increases
        const float brightInput =
            input +
            highpass * kBrightness;

        const float drive =
            brightInput * weight;

        // ----------------------------------------------------
        // DELAY READ
        // ----------------------------------------------------

        const float a = delayA[indexA];
        const float b = delayB[indexB];

        // ----------------------------------------------------
        // FEEDBACK NETWORK
        // ----------------------------------------------------

        const float nextA =
            drive +
            a * kFeedback +
            b * kCrossfeed;

        const float nextB =
            drive +
            b * kFeedback -
            a * kCrossfeed;

        // Soft limiting inside feedback network
        delayA[indexA] = Soft(nextA);
        delayB[indexB] = Soft(nextB);

        // ----------------------------------------------------
        // ADVANCE DELAYS
        // ----------------------------------------------------

        ++indexA;
        ++indexB;

        if(indexA >= kDelayA)
            indexA = 0;

        if(indexB >= kDelayB)
            indexB = 0;

        // ----------------------------------------------------
        // OUTPUT
        // ----------------------------------------------------

        const float wet =
            0.5f * (a + b);

        return wet * kWetLevel;
    }
};
