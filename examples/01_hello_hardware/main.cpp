#include "daisy_seed.h"
#include "touch/touch.h"

using namespace daisy;
using namespace synthux;

static DaisySeed hw;
static Touch     touch;

static float master_gain = 0.5f;
static float stereo_pan  = 0.5f;

void AudioCallback(AudioHandle::InputBuffer in, 
                   AudioHandle::OutputBuffer out, 
                   size_t size) {
    for (size_t i = 0; i < size; i++) {
        // Pass-through audio input scaled by Left Fader (gain) and Right Fader (pan)
        float in_mono = (in[0][i] + in[1][i]) * 0.5f * master_gain;
        out[0][i] = in_mono * (1.0f - stereo_pan);
        out[1][i] = in_mono * stereo_pan;
    }
}

int main(void) {
    hw.Init(true);
    hw.SetAudioBlockSize(16);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    touch.Init(hw);
    hw.StartAudio(AudioCallback);

    // Warm-up settle loop
    for (int i = 0; i < 40; i++) {
        touch.Process();
        System::Delay(3);
    }

    while (1) {
        touch.Process();

        // 1. Faders: S36 (Left) = Master Gain, S37 (Right) = Pan
        master_gain = touch.knobs().LeftFader();
        stereo_pan  = touch.knobs().RightFader();

        // 2. Visual feedback: LED turns ON when any pad is touched
        bool is_touched = touch.pads().HasTouch();
        hw.SetLed(is_touched);

        System::Delay(3);
    }
}
