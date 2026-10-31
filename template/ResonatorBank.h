
#pragma once

#include "daisysp.h"
#include "FrequencyDistribution.h"
#include "DecayDistribution.h"
#include "Shimmer.h"
#include <cstdint>
#include <cmath>

class ResonatorBank {
public:
    static constexpr int kBands = 7;

    // ========================================================
    // CONFIGURATION
    // ========================================================

    static constexpr float kSampleRate = 48000.f;

    static constexpr float kResonance = 0.999f;

    static constexpr float kMinDecay = 0.03f;
    static constexpr float kMaxDecay = 2.00f;

    static constexpr float kExcitationTime = 0.004f;

    // Original pitch envelope
    static constexpr float kPitchSweepAmount = 1.5f;
    static constexpr float kPitchTimeConstant = 0.017f;

    // Snare character
    static constexpr float kSnareBurstTime = 0.11f;
    static constexpr float kSnareHighpassHz = 700.f;
    static constexpr float kSnareLevel = 0.25f;

    static constexpr float kFrequencySmooth = 0.025f;

    static constexpr float kMasterGain = 2.0f;

    static constexpr float kBandGain[kBands] = {
        3.0f,
        2.5f,
        2.0f,
        1.6f,
        1.35f,
        1.15f,
        1.0f
    };

    // ========================================================
    // BAND
    // ========================================================

    struct Band {

        daisysp::Svf filter;
        Shimmer shimmer;

        float frequency = 100.f;
        float targetFrequency = 100.f;

        float amplitude = 0.f;
        float decayCoefficient = 0.999f;

        float excitation = 0.f;
        float gate = 0.f;

        // Body
        float bodyWeight = 0.f;
        float targetBodyWeight = 0.f;
        float pitchEnvelope = 0.f;

        // Rattle
        float noiseWeight = 0.f;
        float targetNoiseWeight = 0.f;
        float noiseEnvelope = 0.f;
        float noiseLowpass = 0.f;

        // Shimmer
        float shimmerWeight = 0.f;
        float targetShimmerWeight = 0.f;
    };

    // ========================================================
    // STATE
    // ========================================================

    Band bands[kBands];

    float sampleRate = kSampleRate;

    float excitationCoefficient = 0.f;
    float pitchCoefficient = 0.f;

    float snareBurstCoefficient = 0.f;
    float snareHighpassCoefficient = 0.f;

    uint32_t noiseState = 0xA341316Cu;
    uint32_t modulationCounter = 0;

    // ========================================================
    // UTILITIES
    // ========================================================

    static float Clamp(float x, float lo, float hi) {
        return x < lo ? lo : (x > hi ? hi : x);
    }

    static float Soft(float x) {
        return x / (1.f + fabsf(x));
    }

    float EnvelopeCoefficient(float seconds) const {
        return expf(
            -6.907755f / (sampleRate * seconds)
        );
    }

    // ========================================================
    // INITIALIZATION
    // ========================================================

    void Init(float sr) {

        sampleRate = sr;

        noiseState = 0xA341316Cu;
        modulationCounter = 0;

        static constexpr float frequencies[kBands] = {
            65.f,
            110.f,
            185.f,
            310.f,
            520.f,
            875.f,
            1470.f
        };

        excitationCoefficient =
            EnvelopeCoefficient(kExcitationTime);

        pitchCoefficient = expf(
            -1.f /
            (sampleRate * kPitchTimeConstant)
        );

        snareBurstCoefficient =
            EnvelopeCoefficient(kSnareBurstTime);

        snareHighpassCoefficient =
            1.f - expf(
                -2.f * 3.14159265359f *
                kSnareHighpassHz / sampleRate
            );

        for(int i = 0; i < kBands; ++i) {

            Band& b = bands[i];

            b.filter.Init(sampleRate);
            b.filter.SetRes(kResonance);

            b.shimmer.Init(sampleRate);

            b.frequency = frequencies[i];
            b.targetFrequency = frequencies[i];

            b.filter.SetFreq(b.frequency);

            b.amplitude = 0.f;
            b.excitation = 0.f;
            b.gate = 0.f;

            b.bodyWeight = 0.f;
            b.targetBodyWeight = 0.f;
            b.pitchEnvelope = 0.f;

            b.noiseWeight = 0.f;
            b.targetNoiseWeight = 0.f;
            b.noiseEnvelope = 0.f;
            b.noiseLowpass = 0.f;

            b.shimmerWeight = 0.f;
            b.targetShimmerWeight = 0.f;
        }

        SetDecay(0.45f, 0.f);
    }

    // ========================================================
    // FREQUENCY DISTRIBUTION
    // ========================================================

    void SetFrequencyDistribution(float position) {

        float frequencies[kBands];

        FrequencyDistribution::Calculate(
            position,
            frequencies
        );

        for(int i = 0; i < kBands; ++i) {

            bands[i].targetFrequency =
                frequencies[i];
        }
    }

    // ========================================================
    // DECAY
    // ========================================================

    void SetDecay(float baseline, float weight) {

        baseline = Clamp(baseline, 0.f, 1.f);

        const float baselineSeconds =
            kMinDecay *
            powf(
                kMaxDecay / kMinDecay,
                baseline
            );

        float multipliers[kBands];

        DecayDistribution::Calculate(
            weight,
            multipliers
        );

        for(int i = 0; i < kBands; ++i) {

            const float seconds =
                baselineSeconds * multipliers[i];

            bands[i].decayCoefficient =
                EnvelopeCoefficient(seconds);
        }
    }

    // ========================================================
    // CHARACTER WEIGHTS
    // ========================================================

    void SetBodyWeight(int voice, float weight) {

        if(voice < 0 || voice >= kBands)
            return;

        bands[voice].targetBodyWeight =
            Clamp(weight, 0.f, 1.f);
    }

    void SetRattleWeight(int voice, float weight) {

        if(voice < 0 || voice >= kBands)
            return;

        bands[voice].targetNoiseWeight =
            Clamp(weight, 0.f, 1.f);
    }

    void SetShimmerWeight(int voice, float weight) {

        if(voice < 0 || voice >= kBands)
            return;

        bands[voice].targetShimmerWeight =
            Clamp(weight, 0.f, 1.f);
    }

    // ========================================================
    // EXCITATION
    // ========================================================

    void Excite(
        int index,
        float velocity,
        float strength
    ) {

        if(index < 0 || index >= kBands)
            return;

        Band& b = bands[index];

        const float level = Clamp(
            velocity * strength,
            0.f,
            1.f
        );

        b.excitation = level;
        b.amplitude = level;
        b.pitchEnvelope = 1.f;
        b.noiseEnvelope = 1.f;
    }

    // ========================================================
    // WHITE NOISE
    // ========================================================

    float GenerateNoise() {

        noiseState ^= noiseState << 13;
        noiseState ^= noiseState >> 17;
        noiseState ^= noiseState << 5;

        return float(noiseState >> 8) *
            (1.f / 8388607.5f) - 1.f;
    }

    // ========================================================
    // AUDIO PROCESSING
    // ========================================================

    float Process(uint8_t audibleMask) {

        float mix = 0.f;

        const bool updateFrequency =
            (modulationCounter++ & 15u) == 0u;

        for(int i = 0; i < kBands; ++i) {

            Band& b = bands[i];

            // ------------------------------------------------
            // SMOOTH CHARACTER WEIGHTS
            // ------------------------------------------------

            b.bodyWeight += 0.002f *
                (b.targetBodyWeight - b.bodyWeight);

            b.noiseWeight += 0.002f *
                (b.targetNoiseWeight - b.noiseWeight);

            b.shimmerWeight += 0.002f *
                (b.targetShimmerWeight - b.shimmerWeight);

            // ------------------------------------------------
            // FREQUENCY + PITCH ENVELOPE
            // ------------------------------------------------

            if(updateFrequency) {

                b.frequency += kFrequencySmooth *
                    (b.targetFrequency - b.frequency);

                const float pitchExcursion =
                    b.pitchEnvelope *
                    b.bodyWeight *
                    kPitchSweepAmount;

                const float frequency = Clamp(
                    b.frequency * (1.f + pitchExcursion),
                    20.f,
                    18000.f
                );

                b.filter.SetFreq(frequency);
            }

            b.pitchEnvelope *= pitchCoefficient;

            if(b.pitchEnvelope < 0.0001f)
                b.pitchEnvelope = 0.f;

            // ------------------------------------------------
            // INTERNAL EXCITATION
            // ------------------------------------------------

            const float excitationSignal =
                GenerateNoise() * b.excitation;

            b.excitation *= excitationCoefficient;

            if(b.excitation < 0.0001f)
                b.excitation = 0.f;

            // ------------------------------------------------
            // SVF BANDPASS
            // ------------------------------------------------

            b.filter.Process(excitationSignal);

            const float filtered =
                b.filter.Band();

            // ------------------------------------------------
            // RATTLE / SNARE
            // ------------------------------------------------

            const float whiteNoise =
                GenerateNoise();

            b.noiseLowpass +=
                snareHighpassCoefficient *
                (whiteNoise - b.noiseLowpass);

            const float highpassedNoise =
                whiteNoise - b.noiseLowpass;

            const float noiseTexture =
                highpassedNoise *
                b.noiseEnvelope *
                b.noiseWeight *
                kSnareLevel;

            b.noiseEnvelope *= snareBurstCoefficient;

            if(b.noiseEnvelope < 0.0001f)
                b.noiseEnvelope = 0.f;

            // ------------------------------------------------
            // SHARED AMPLITUDE ENVELOPE
            // ------------------------------------------------

            const float dry =
                (filtered + noiseTexture) *
                b.amplitude;

            b.amplitude *= b.decayCoefficient;

            if(b.amplitude < 0.0001f)
                b.amplitude = 0.f;

            // ------------------------------------------------
            // PAD GATE
            // ------------------------------------------------

            const float target =
                (audibleMask & (1u << i))
                    ? 1.f
                    : 0.f;

            const float increment =
                1.f / (sampleRate * 0.002f);

            b.gate += Clamp(
                target - b.gate,
                -increment,
                increment
            );

            // ------------------------------------------------
            // SHIMMER
            // ------------------------------------------------

            // Feed the already enveloped and gated
            // voice into the spatial effect.
            const float gatedDry =
                dry * b.gate;

            const float shimmerWet =
                b.shimmer.Process(
                    gatedDry,
                    b.shimmerWeight
                );

            // Shimmer tails are allowed to fade
            // naturally after pad release.
            const float voice =
                gatedDry + shimmerWet;

            // ------------------------------------------------
            // OUTPUT MIX
            // ------------------------------------------------

            mix += voice * kBandGain[i];
        }

        // ====================================================
        // MASTER OUTPUT
        // ====================================================

        return Soft(mix * kMasterGain);
    }
};
