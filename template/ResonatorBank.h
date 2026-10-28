
#pragma once
#include <cstdint>
#include <cmath>

class ResonatorBank {
public:
    static constexpr int kBands = 7;
    static constexpr float PI = 3.14159265358979323846f;

    struct Band {
        float re = 0.f, im = 0.f;
        float cosine = 1.f, sine = 0.f;
        float damping = .999f;
        float gate = 0.f;

        // External audio bandpass filter
        float ic1 = 0.f, ic2 = 0.f;
        float a1 = 0.f, a2 = 0.f, a3 = 0.f;

        // Envelope derived from resonator output
        float follower = 0.f;
        float envelope = 0.f;
    };

    Band bands[kBands];
    float previous[kBands]{};
    float sampleRate = 48000.f;

    static float Clamp(float v, float lo, float hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    static float Soft(float x) {
        return x / (1.f + fabsf(x));
    }

    void Init(float sr) {
        sampleRate = sr;
        for(auto &b : bands) b = Band{};
        for(auto &x : previous) x = 0.f;
    }

    void SetSound(float decay, float shift, float spread) {
        const float minHz = 50.f;
        const float maxHz = 15000.f;

        const float span = log2f(maxHz / minHz);
        const float width =
            (.12f + .88f * spread) * span;

        const float centre =
            (span - width) * shift + width * .5f;

        const float decaySeconds =
            .035f * powf(110.f, Clamp(decay, 0.f, 1.f));

        const float damping =
            expf(-1.f / (sampleRate * decaySeconds));

        constexpr float Q = 3.f;
        constexpr float k = 1.f / Q;

        for(int i = 0; i < kBands; ++i) {
            const float offset = (float(i) - 3.f) / 6.f;
            const float freq =
                minHz * exp2f(centre + offset * width);

            const float phase = 2.f * PI * freq / sampleRate;

            Band &b = bands[i];
            b.cosine = cosf(phase);
            b.sine = sinf(phase);
            b.damping = damping;

            // TPT state-variable bandpass
            const float g = tanf(PI * freq / sampleRate);
            b.a1 = 1.f / (1.f + g * (g + k));
            b.a2 = g * b.a1;
            b.a3 = g * b.a2;
        }
    }

    void Excite(int index, float velocity, float strength) {
        if(index < 0 || index >= kBands) return;

        Band &b = bands[index];
        b.im += .16f * velocity * strength;

        // Short envelope ceiling, independent of
        // the much longer resonator decay.
        b.envelope = 1.f;
    }

    float Process(uint8_t audibleMask,
                  float fold,
                  uint8_t couplingMode,
                  uint8_t couplingMask,
                  float externalInput,
                  float externalMix) {

        float raw[kBands]{};
        float externalBands[kBands]{};

        externalInput = Clamp(externalInput, -1.f, 1.f);
        externalMix = Clamp(externalMix, 0.f, 1.f);

        // Approximately 180 ms envelope release.
        const float envDecay =
            expf(-1.f / (sampleRate * .18f));

        for(int i = 0; i < kBands; ++i) {
            Band &b = bands[i];

            // Filter the incoming audio continuously.
            const float v3 = externalInput - b.ic2;
            const float v1 = b.a1 * b.ic1 + b.a2 * v3;
            const float v2 =
                b.ic2 + b.a2 * b.ic1 + b.a3 * v3;

            b.ic1 = 2.f * v1 - b.ic1;
            b.ic2 = 2.f * v2 - b.ic2;

            // v1 is the bandpass output.
            const float spectralAudio = v1;

            // Existing cross-resonator coupling.
            float injection = 0.f;

            if(couplingMask & (1u << i)) {
                if(couplingMode == 1)
                    injection =
                        .015f * previous[(i + 6) % kBands];
                else if(couplingMode == 2)
                    injection = .009f *
                        (previous[(i + 6) % kBands] +
                         previous[(i + 1) % kBands]);
            }

            b.im = Clamp(b.im + injection, -2.f, 2.f);
            b.re = Clamp(b.re, -2.f, 2.f);

            const float nextRe =
                b.re * b.cosine - b.im * b.sine;
            const float nextIm =
                b.re * b.sine + b.im * b.cosine;

            b.re = nextRe * b.damping;
            b.im = nextIm * b.damping;

            raw[i] = b.re;

            // Follow the absolute amplitude of the ping.
            // Fast attack, slower release.
            const float magnitude = fabsf(b.re);
            const float coefficient =
                magnitude > b.follower ? .012f : .0015f;

            b.follower +=
                (magnitude - b.follower) * coefficient;

            b.envelope *= envDecay;

            // The ping's amplitude shapes the gate,
            // while the 180 ms ceiling prevents long tails.
            const float pingShape =
                Clamp(b.follower * 18.f, 0.f, 1.f);

            const float spectralEnvelope =
                b.envelope * pingShape;

            externalBands[i] =
                spectralAudio * spectralEnvelope;

            // Existing touch-controlled inverted mute.
            const float target =
                (audibleMask & (1u << i)) ? 1.f : 0.f;

            const float inc = 1.f / (sampleRate * .002f);
            b.gate += Clamp(target - b.gate, -inc, inc);
        }

        for(int i = 0; i < kBands; ++i)
            previous[i] = raw[i];

        float mix = 0.f;

        for(int i = 0; i < kBands; ++i) {
            const float x = raw[i];

            // Gentle internal saturation.
            const float saturated =
                Soft(x * (1.f + 3.f * fold));

            const float internal =
                (1.f - fold) * x + fold * saturated;

            // External gain compensation.
            // This is deliberately conservative.
            const float external =
                Soft(externalBands[i] * 2.5f);

            const float blended =
                (1.f - externalMix) * internal +
                externalMix * external;

            mix += blended * bands[i].gate;
        }

        return .30f * Soft(mix);
    }
};
