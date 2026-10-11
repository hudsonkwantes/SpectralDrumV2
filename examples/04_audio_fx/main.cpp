#include "daisy_seed.h"
#include "daisysp.h"
#include "touch/touch.h"

using namespace daisy;
using namespace daisysp;
using namespace synthux;

static DaisySeed hw;
static Touch     touch;

static constexpr size_t kMaxDelay = 24000; // 0.5s @ 48kHz
static DelayLine<float, kMaxDelay> delay_l;
static DelayLine<float, kMaxDelay> delay_r;
static Svf filter_l;
static Svf filter_r;

static float smooth_cutoff = 1000.0f;
static float target_cutoff = 1000.0f;
static float smooth_delay  = 4800.0f;
static float target_delay  = 4800.0f;

static float delay_fb  = 0.3f;
static float delay_wet = 0.3f;
static bool  freeze_active = false;

void AudioCallback(AudioHandle::InputBuffer in, 
                   AudioHandle::OutputBuffer out, 
                   size_t size) {
    for (size_t i = 0; i < size; i++) {
        // Smooth parameters
        fonepole(smooth_cutoff, target_cutoff, 0.005f);
        fonepole(smooth_delay, target_delay, 0.001f);

        delay_l.SetDelay(smooth_delay);
        delay_r.SetDelay(smooth_delay * 0.75f); // Stereo offset

        // 1. Stereo SVF Filter on input audio
        filter_l.SetFreq(smooth_cutoff);
        filter_r.SetFreq(smooth_cutoff);

        filter_l.Process(in[0][i]);
        filter_r.Process(in[1][i]);

        float filt_l = filter_l.Low();
        float filt_r = filter_r.Low();

        // 2. Stereo Delay Line with feedback (or infinite freeze if pad held)
        float del_l = delay_l.Read();
        float del_r = delay_r.Read();

        float effective_fb = freeze_active ? 0.98f : delay_fb;

        delay_l.Write(SoftClip(filt_l + del_r * effective_fb));
        delay_r.Write(SoftClip(filt_r + del_l * effective_fb));

        // 3. Output mix
        out[0][i] = SoftClip(filt_l + del_l * delay_wet);
        out[1][i] = SoftClip(filt_r + del_r * delay_wet);
    }
}

int main(void) {
    hw.Init(true);
    hw.SetAudioBlockSize(16);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    touch.Init(hw);

    float sr = hw.AudioSampleRate();
    filter_l.Init(sr);
    filter_r.Init(sr);
    delay_l.Init();
    delay_r.Init();

    hw.StartAudio(AudioCallback);

    for (int i = 0; i < 40; i++) {
        touch.Process();
        System::Delay(3);
    }

    while (1) {
        touch.Process();

        // 1. Left Fader (S36): Filter Cutoff (60 Hz to 14,000 Hz)
        target_cutoff = 60.0f + touch.knobs().LeftFader() * 14000.0f;

        // S30: Filter Resonance
        float res = touch.knobs().s30() * 0.85f;
        filter_l.SetRes(res);
        filter_r.SetRes(res);

        // 2. Right Fader (S37): Delay Time (10ms to 500ms)
        target_delay = 480.0f + touch.knobs().RightFader() * (kMaxDelay - 480.0f);

        // S34: Delay Feedback, S35: Delay Wet Mix
        delay_fb  = touch.knobs().s34() * 0.85f;
        delay_wet = touch.knobs().s35() * 0.70f;

        // 3. Performance Touch: Holding any pad Freezes / Repeats the audio in an infinite loop!
        freeze_active = touch.pads().HasTouch();

        // LED lights up when Freeze is active
        hw.SetLed(freeze_active);

        System::Delay(3);
    }
}
