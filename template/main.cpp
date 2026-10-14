#include "daisy_seed.h"
#include "touch/touch.h"
#include "Sequencer.h"
#include "ResonatorBank.h"
#include <cmath>

using namespace daisy;
using namespace synthux;

static DaisySeed hw;
static Touch touch;
static Sequencer sequencer;
static ResonatorBank resonators;

// Control snapshot: written in main thread, read by audio callback.
static volatile float ctlDecay=0.45f, ctlShift=0.5f, ctlSpread=0.65f, ctlFold=0.25f;
static volatile float ctlTilt=0.f;
static volatile int ctlLength=16, ctlX=0, ctlY=0;
static volatile float ctlBpm=120.f;
static volatile uint8_t ctlAudible=0;
static volatile bool ctlRunning=false;
static volatile bool ctlRestart=false;

static float Clamp(float x,float a,float b) { return x<a?a:(x>b?b:x); }
static int Round(float x) { return int(x>=0.f?x+0.5f:x-0.5f); }
static float Bipolar(float position) {
    const float v=2.f*position-1.f;
    return fabsf(v)<0.035f ? 0.f : Clamp(v,-1.f,1.f);
}

static void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    (void)in;
    static uint32_t samplesIntoStep=0;
    static bool previousRunning=false;
    const bool running=ctlRunning;
    const bool restart=ctlRestart;
    if(restart) { samplesIntoStep=0; ctlRestart=false; }
    const float bpm=Clamp(ctlBpm,30.f,240.f);
    // 4 sequencer steps per quarter note; global 16th-note quantization.
    const uint32_t samplesPerStep = uint32_t(48000.f*60.f/(bpm*4.f)+0.5f);
    const int len=ctlLength, x=ctlX, y=ctlY;
    const float tilt=ctlTilt, fold=ctlFold;
    const uint8_t audible=ctlAudible;

    // Recompute DSP coefficients once per audio block, not once per sample.
    resonators.SetSound(ctlDecay,ctlShift,ctlSpread);
    if(running && !previousRunning && !restart) {
        // Resume at saved phase; do not force any new trigger.
    }
    previousRunning=running;
    for(size_t i=0;i<size;++i) {
        if(running) {
            if(samplesIntoStep == 0) {
                const uint8_t triggers=sequencer.Advance(len,tilt,x,y);
                for(int b=0;b<7;++b) if(triggers & (1u<<b)) resonators.Excite(b);
            }
            if(++samplesIntoStep >= samplesPerStep) samplesIntoStep=0;
        }
        const float output=resonators.Process(audible,fold);
        out[0][i]=output;
        out[1][i]=output;
    }
}

int main(void) {
    hw.Init(true);
    hw.SetAudioBlockSize(16);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    touch.Init(hw);
    sequencer.Init();
    resonators.Init(48000.f);
    // Allow MPR121 touch baselines to settle.
    for(int i=0;i<40;++i) { touch.Process(); System::Delay(3); }
    hw.StartAudio(AudioCallback);

    bool wasTap=false,wasStop=false,wasStart=false;
    uint32_t lastTap=0;
    float filteredBpm=120.f;
    while(1) {
        touch.Process();
        // Sound: S31 resonance, S32 shift, S33 spread, S34 folding.
        ctlDecay=touch.knobs().s31();
        ctlShift=touch.knobs().s32();
        ctlSpread=touch.knobs().s33();
        ctlFold=touch.knobs().s34();
        // Sequencer: S30 global length, S35 tilt, S36/37 matrix translation.
        ctlLength=Round(Clamp(touch.knobs().s30(),0.f,1.f)*16.f);
        ctlTilt=Bipolar(touch.knobs().s35());
        ctlX=Round(Bipolar(touch.knobs().LeftFader())*15.f);
        ctlY=Round(Bipolar(touch.knobs().RightFader())*6.f);

        uint8_t pads=0;
        for(int b=0;b<7;++b) if(touch.pads().IsTouched(b+3)) pads|=uint8_t(1u<<b);
        ctlAudible=pads;

        // Hardware P10 = T (stop), P11 = H (start), P01 = tap tempo.
        const bool stop=touch.pads().IsTouched(10);
        const bool start=touch.pads().IsTouched(11);
        const bool tap=touch.pads().IsTouched(1);
        if(stop && !wasStop) ctlRunning=false;
        if(start && !wasStart) ctlRunning=true;
        if(tap && !wasTap) {
            const uint32_t now=System::GetNow();
            const uint32_t elapsed=now-lastTap;
            if(lastTap && elapsed>=250u && elapsed<=2000u) {
                const float instantaneous=60000.f/float(elapsed);
                filteredBpm=0.6f*filteredBpm+0.4f*instantaneous;
                ctlBpm=Clamp(filteredBpm,30.f,240.f);
            } else {
                filteredBpm=ctlBpm;
            }
            lastTap=now;
        }
        wasStop=stop; wasStart=start; wasTap=tap;
        hw.SetLed(ctlRunning);
        System::Delay(3);
    }
}
