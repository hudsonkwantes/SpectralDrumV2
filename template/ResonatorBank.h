#pragma once

#include "daisysp.h"
#include "FrequencyDistribution.h"
#include "DecayDistribution.h"
#include "Crush.h"

#include <cstdint>
#include <cmath>

class ResonatorBank {
public:

    static constexpr int kBands = 7;

    // ========================================================
    // CONFIGURATION
    // ========================================================

    static constexpr float kSampleRate = 48000.f;

    // Fixed SVF resonance
    static constexpr float kResonance = 0.999f;

    // Amplitude envelope
    static constexpr float kMinDecay = 0.03f;
    static constexpr float kMaxDecay = 3.00f;
    static constexpr float kAttackTime = 0.002f;

    // Internal excitation
    static constexpr float kExcitationTime = 0.001f;

    // Body
    static constexpr float kPitchSweepAmount = 1.2f;
    static constexpr float kPitchTimeConstant = 0.01f;
    static constexpr float kBodyDrive = 3.0f;

    // Rattle
    static constexpr float kRattleBandwidth = 5000.f;
    static constexpr float kRattleDepth = 1.60f;

    // Frequency smoothing
    static constexpr float kFrequencySmooth = 0.025f;

    // Output gain
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
        Crush crush;

        // Frequency
        float frequency = 100.f;
        float targetFrequency = 100.f;

        // Amplitude envelope
        float amplitude = 0.f;
        float decayCoefficient = 0.999f;

        // Attack
        float attackIncrement = 0.f;
        uint32_t attackRemaining = 0;
        float attackTarget = 1.f;

        // Internal excitation
        float excitation = 0.f;

        // Performance gate
        float gate = 0.f;

        // Body
        float bodyWeight = 0.f;
        float targetBodyWeight = 0.f;
        float pitchEnvelope = 0.f;

        // Rattle
        float noiseWeight = 0.f;
        float targetNoiseWeight = 0.f;
        float noiseModulation = 0.f;

        // Crush
        float crushWeight = 0.f;
        float targetCrushWeight = 0.f;
    };

    // ========================================================
    // STATE
    // ========================================================

    Band bands[kBands];

    float sampleRate = kSampleRate;

    uint32_t attackSamples = 96;

    float excitationCoefficient = 0.f;
    float pitchCoefficient = 0.f;
    float rattleFilterCoefficient = 0.f;

    float baselineDecay = 0.45f;
    float stepDecayDepth = 0.f;

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

        attackSamples = uint32_t(
            sampleRate * kAttackTime + 0.5f
        );

        if(attackSamples < 1)
            attackSamples = 1;

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

        rattleFilterCoefficient =
            1.f - expf(
                -2.f * 3.14159265359f *
                kRattleBandwidth / sampleRate
            );

        baselineDecay = 0.45f;
        stepDecayDepth = 0.f;

        const float initialSeconds =
            kMinDecay *
            powf(
                kMaxDecay / kMinDecay,
                baselineDecay
            );

        for(int i = 0; i < kBands; ++i) {

            Band& b = bands[i];

            b.filter.Init(sampleRate);
            b.filter.SetRes(kResonance);

            b.crush.Init();

            b.frequency = frequencies[i];
            b.targetFrequency = frequencies[i];

            b.filter.SetFreq(b.frequency);

            b.amplitude = 0.f;

            b.decayCoefficient =
                EnvelopeCoefficient(initialSeconds);

            b.attackIncrement = 0.f;
            b.attackRemaining = 0;
            b.attackTarget = 1.f;

            b.excitation = 0.f;
            b.gate = 0.f;

            b.bodyWeight = 0.f;
            b.targetBodyWeight = 0.f;
            b.pitchEnvelope = 0.f;

            b.noiseWeight = 0.f;
            b.targetNoiseWeight = 0.f;
            b.noiseModulation = 0.f;

            b.crushWeight = 0.f;
            b.targetCrushWeight = 0.f;
        }
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

    void SetDecay(float baseline, float depth) {

        baselineDecay = Clamp(
            baseline,
            0.f,
            1.f
        );

        stepDecayDepth = Clamp(
            depth,
            0.f,
            1.f
        );
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

    void SetCrushWeight(int voice, float weight) {

        if(voice < 0 || voice >= kBands)
            return;

        bands[voice].targetCrushWeight =
            Clamp(weight, 0.f, 1.f);
    }

    // ========================================================
    // EXCITATION
    // ========================================================

    void Excite(
        int index,
        float velocity,
        float strength,
        int step
    ) {

        if(index < 0 || index >= kBands)
            return;

        Band& b = bands[index];

        const float level = Clamp(
            velocity * strength,
            0.f,
            1.f
        );

        // ----------------------------------------------------
        // PER-STEP DECAY
        // ----------------------------------------------------

        const float baselineSeconds =
            kMinDecay *
            powf(
                kMaxDecay / kMinDecay,
                baselineDecay
            );

        const float multiplier =
            DecayDistribution::Calculate(
                index,
                step,
                stepDecayDepth
            );

        const float seconds =
            baselineSeconds * multiplier;

        b.decayCoefficient =
            EnvelopeCoefficient(seconds);

        // ----------------------------------------------------
        // ATTACK
        // ----------------------------------------------------

        b.attackRemaining = attackSamples;
        b.attackTarget = level;

        b.attackIncrement =
            (level - b.amplitude) /
            float(attackSamples);

        // ----------------------------------------------------
        // RETRIGGER
        // ----------------------------------------------------

        b.excitation = level;
        b.pitchEnvelope = 1.f;
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

    float Process(
        uint8_t audibleMask,
        float fadeSeconds = 0.002f,
        float externalAudio = 0.f,
        float internalMix = 1.f
    ) {

        float mix = 0.f;

        // ----------------------------------------------------
        // SOURCE MIX
        // ----------------------------------------------------

        const float internalGain =
            Clamp(internalMix, 0.f, 1.f);

        const float externalGain =
            1.f - internalGain;

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

            b.crushWeight += 0.002f *
                (b.targetCrushWeight - b.crushWeight);

            // ------------------------------------------------
            // FREQUENCY + BODY
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

            // ------------------------------------------------
            // BODY ENVELOPE
            // ------------------------------------------------

            b.pitchEnvelope *= pitchCoefficient;

            if(b.pitchEnvelope < 0.0001f)
                b.pitchEnvelope = 0.f;

            // ------------------------------------------------
            // INTERNAL EXCITATION
            // ------------------------------------------------

            const float internalExcitation =
                GenerateNoise() * b.excitation;

            b.excitation *= excitationCoefficient;

            if(b.excitation < 0.0001f)
                b.excitation = 0.f;

            // ------------------------------------------------
            // EXTERNAL EXCITATION
            // ------------------------------------------------

            // Continuously receives external audio.
            // Sequencer envelopes control audibility.

            const float externalExcitation =
                externalAudio;

            // ------------------------------------------------
            // INTERNAL / EXTERNAL CROSSFADE
            // ------------------------------------------------

            const float excitationSignal =
                internalExcitation * internalGain +
                externalExcitation * externalGain;

            // ------------------------------------------------
            // SVF BANDPASS
            // ------------------------------------------------

            b.filter.Process(excitationSignal);

            const float filtered =
                b.filter.Band();

            // ------------------------------------------------
            // BODY SOFT CLIPPING
            // ------------------------------------------------

            const float bodyAmount = Clamp(
                b.bodyWeight * b.pitchEnvelope,
                0.f,
                1.f
            );

            const float drive =
                1.f + kBodyDrive * bodyAmount;

            const float saturated =
                tanhf(filtered * drive);

            const float bodyProcessed =
                filtered +
                bodyAmount * (saturated - filtered);

            // ------------------------------------------------
            // RATTLE: NOISE AM
            // ------------------------------------------------

            const float rawNoise =
                GenerateNoise();

            b.noiseModulation +=
                rattleFilterCoefficient *
                (rawNoise - b.noiseModulation);

            const float amGain = Clamp(
                1.f +
                b.noiseWeight *
                kRattleDepth *
                b.noiseModulation,
                0.f,
                2.f
            );

            const float rattled =
                bodyProcessed * amGain;

            // ------------------------------------------------
            // CRUSH: BIT + RATE REDUCTION
            // ------------------------------------------------

            const float transformed =
                b.crush.Process(
                    rattled,
                    b.crushWeight
                );

            // ------------------------------------------------
            // AMPLITUDE ATTACK + DECAY
            // ------------------------------------------------

            if(b.attackRemaining > 0) {

                b.amplitude += b.attackIncrement;

                --b.attackRemaining;

                if(b.attackRemaining == 0)
                    b.amplitude = b.attackTarget;
            }
            else {

                b.amplitude *= b.decayCoefficient;
            }

            if(b.amplitude < 0.0001f &&
               b.attackRemaining == 0) {

                b.amplitude = 0.f;
            }

            // ------------------------------------------------
            // SHARED AMPLITUDE ENVELOPE
            // ------------------------------------------------

            const float voice =
                transformed * b.amplitude;

            // ------------------------------------------------
            // PERFORMANCE GATE
            // ------------------------------------------------

            const float target =
                (audibleMask & (1u << i))
                    ? 1.f
                    : 0.f;

            const float fadeTime = Clamp(
                fadeSeconds,
                0.002f,
                3.f
            );

            const float increment =
                1.f / (sampleRate * fadeTime);

            b.gate += Clamp(
                target - b.gate,
                -increment,
                increment
            );

            // ------------------------------------------------
            // OUTPUT MIX
            // ------------------------------------------------

            mix += voice *
                b.gate *
                kBandGain[i];
        }

        // ====================================================
        // MASTER OUTPUT
        // ====================================================

        return Soft(mix * kMasterGain);
    }
};