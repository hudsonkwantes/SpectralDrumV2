#pragma once
#include <cmath>

class ResonatorBank {
public:
    static constexpr int kBands = 7;
    static constexpr float kPi = 3.14159265358979323846f;
    struct Band { float re=0.f, im=0.f, cosine=1.f, sine=0.f, damping=0.999f, gate=0.f; };
    Band bands[kBands];
    float sampleRate = 48000.f;

    void Init(float sr) { sampleRate = sr; for(auto &b: bands) b = Band{}; }
    void SetSound(float decay, float shift, float spread) {
        // Equal-tempered semitone distances, logarithmically clamped to 50..15000 Hz.
        constexpr float minHz = 50.f, maxHz = 15000.f;
        const float spanOct = log2f(maxHz / minHz);
        const float widthOct = (0.12f + 0.88f * spread) * spanOct;
        const float centreOct = (spanOct - widthOct) * shift + widthOct * 0.5f;
        // Decay time is the e-folding time of resonator amplitude.
        const float decaySeconds = 0.035f * powf(110.f, decay);
        const float damping = expf(-1.f / (sampleRate * decaySeconds));
        for(int b=0;b<kBands;++b) {
            const float offset = (float(b)-3.f) / 6.f;
            const float freq = minHz * exp2f(centreOct + offset*widthOct);
            const float phase = 2.f*kPi*freq/sampleRate;
            bands[b].cosine = cosf(phase);
            bands[b].sine = sinf(phase);
            bands[b].damping = damping;
        }
    }
    void Excite(int band) {
        // Single trigger impulse. May be revised when tuning the resonant response.
        bands[band].im += 0.22f;
    }
    float Process(uint8_t audibleMask, float fold) {
        float mix = 0.f;
        for(int i=0;i<kBands;++i) {
            Band &b=bands[i];
            const float re = b.re*b.cosine - b.im*b.sine;
            const float im = b.re*b.sine + b.im*b.cosine;
            b.re = re*b.damping;
            b.im = im*b.damping;
            // Very short click-suppressing mute ramps (~2 ms).
            const float target = (audibleMask & (1u<<i)) ? 1.f : 0.f;
            const float inc = 1.f/(sampleRate*0.002f);
            b.gate += fmaxf(-inc, fminf(inc, target-b.gate));
            mix += b.re*b.gate;
        }
        mix *= 0.40f;
        // Simple sine-like folding across multiple lobes as the amount increases.
        const float drive = 1.f + 11.f*fold;
        const float folded = sinf(mix*drive);
        return (1.f-fold)*mix + fold*folded*0.75f;
    }
};