
#pragma once
#include <cstdint>
#include <cmath>

class ResonatorBank {
public:
    static constexpr int kBands = 7;
    static constexpr float kPi = 3.14159265358979323846f;

    struct Band {
        float re=0.f, im=0.f, cosine=1.f, sine=0.f;
        float damping=0.999f, gate=0.f;
        float excitation=0.f;
    };

    Band bands[kBands];
    float sampleRate = 48000.f;
    float last[kBands]{};

    static float Clamp(float x,float lo,float hi) {
        return x < lo ? lo : (x > hi ? hi : x);
    }

    static float Soft(float x) {
        return x/(1.f+fabsf(x));
    }

    void Init(float sr) {
        sampleRate=sr;
        for(auto &b:bands) b=Band{};
        for(auto &x:last) x=0.f;
    }

    void SetSound(float decay,float shift,float spread) {
        constexpr float minHz=50.f,maxHz=15000.f;
        const float spanOct=log2f(maxHz/minHz);
        const float widthOct=(0.12f+0.88f*spread)*spanOct;
        const float centreOct=(spanOct-widthOct)*shift+widthOct*0.5f;
        const float decaySeconds=0.035f*powf(110.f,decay);
        const float damping=expf(-1.f/(sampleRate*decaySeconds));

        for(int b=0;b<kBands;++b) {
            const float offset=(float(b)-3.f)/6.f;
            const float freq=minHz*exp2f(centreOct+offset*widthOct);
            const float phase=2.f*kPi*freq/sampleRate;

            bands[b].cosine=cosf(phase);
            bands[b].sine=sinf(phase);
            bands[b].damping=damping;
        }
    }

    void Excite(int band,float velocity=1.f) {
        bands[band].im += 0.16f*velocity;
    }

    // Coupling uses previous-sample state.
    // mode: 0 independent, 1 serial, 2 feedback.
    float Process(uint8_t audibleMask,float fold,
                  uint8_t mode,uint8_t selectedMask) {
        float raw[kBands]{};

        for(int i=0;i<kBands;++i) {
            Band &b=bands[i];
            float injected=0.f;

            if(selectedMask & (1u<<i)) {
                if(mode==1) {
                    injected=0.015f*last[(i+6)%kBands];
                }
                else if(mode==2) {
                    injected=0.009f*
                        (last[(i+6)%kBands]+last[(i+1)%kBands]);
                }
            }

            b.im += injected;
            b.im=Clamp(b.im,-2.f,2.f);
            b.re=Clamp(b.re,-2.f,2.f);

            const float re=b.re*b.cosine-b.im*b.sine;
            const float im=b.re*b.sine+b.im*b.cosine;

            b.re=re*b.damping;
            b.im=im*b.damping;
            raw[i]=b.re;

            const float target=(audibleMask & (1u<<i))?1.f:0.f;
            const float inc=1.f/(sampleRate*0.002f);
            b.gate+=Clamp(target-b.gate,-inc,inc);
        }

        for(int i=0;i<kBands;++i)
            last[i]=raw[i];

        // Gentle per-band saturation.
        float mix=0.f;

        for(int i=0;i<kBands;++i) {
            const float x=raw[i];
            const float driven=Soft(x*(1.f+3.f*fold));
            const float blended=(1.f-fold)*x+fold*driven;

            mix+=blended*bands[i].gate;
        }

        return 0.30f*Soft(mix);
    }
};
