#include "daisy_seed.h"
#include "daisysp.h"
#include "touch/touch.h"

using namespace daisy;
using namespace daisysp;
using namespace synthux;

static DaisySeed  hw;
static Touch      touch;
static Oscillator osc;
static Svf         filter;

// Major Scale (C Major: C3 to D5)
static const uint8_t kScale[12] = {
    48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67
};

static float active_pressure = 0.0f;
static float note_freq       = 220.0f;
static float smooth_freq     = 220.0f;
static int   octave_shift    = 0;

void AudioCallback(AudioHandle::InputBuffer in, 
                   AudioHandle::OutputBuffer out, 
                   size_t size) {
    for (size_t i = 0; i < size; i++) {
        // Portamento pitch smoothing
        fonepole(smooth_freq, note_freq, 0.008f);
        osc.SetFreq(smooth_freq);

        // Synthesize oscillator modulated by finger touch pressure
        float sig = osc.Process() * active_pressure;

        // Process through SVF filter
        filter.Process(sig);
        float snd = SoftClip(filter.Low());

        out[0][i] = snd;
        out[1][i] = snd;
    }
}

int main(void) {
    hw.Init(true);
    hw.SetAudioBlockSize(16);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    touch.Init(hw);

    float sr = hw.AudioSampleRate();
    osc.Init(sr);
    osc.SetAmp(0.5f);
    osc.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);

    filter.Init(sr);

    // Pad strike callbacks
    touch.pads().SetOnTouch([](uint16_t pad) {
        if (pad < 12) {
            uint8_t midi_note = kScale[pad] + (octave_shift * 12);
            note_freq = mtof(midi_note);
        }
    });

    hw.StartAudio(AudioCallback);

    for (int i = 0; i < 40; i++) {
        touch.Process();
        System::Delay(3);
    }

    while (1) {
        touch.Process();

        // 1. Continuous pressure: track highest active pad pressure
        float peak = 0.0f;
        for (size_t p = 0; p < 12; p++) {
            if (touch.pads().IsTouched(p)) {
                if (touch.pads()[p] > peak) peak = touch.pads()[p];
            }
        }
        active_pressure = peak;

        // 2. Left Fader (S36) = Base Cutoff + pressure modulation
        float base_cutoff = 80.0f + touch.knobs().LeftFader() * 6000.0f;
        float dyn_cutoff  = base_cutoff + (active_pressure * 5000.0f);
        filter.SetFreq(fclamp(dyn_cutoff, 40.0f, 16000.0f));

        // S30 = Resonance
        filter.SetRes(touch.knobs().s30() * 0.85f);

        // 3. Switch A: Waveform (Up: Saw, Center: Triangle, Down: Square)
        if (touch.switches().IsAUp()) {
            osc.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);
        } else if (touch.switches().IsADown()) {
            osc.SetWaveform(Oscillator::WAVE_POLYBLEP_SQUARE);
        } else {
            osc.SetWaveform(Oscillator::WAVE_POLYBLEP_TRI);
        }

        // 4. Switch B: Octave Shift (Up: +1, Center: 0, Down: -1)
        if (touch.switches().IsBUp()) {
            octave_shift = 1;
        } else if (touch.switches().IsBDown()) {
            octave_shift = -1;
        } else {
            octave_shift = 0;
        }

        // 5. LED indicates note activity
        hw.SetLed(active_pressure > 0.01f);

        System::Delay(3);
    }
}
