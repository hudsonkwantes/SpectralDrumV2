
#include "daisy_seed.h"
#include "touch/touch.h"
#include "Sequencer.h"
#include "ResonatorBank.h"
#include "Character.h"
#include <cmath>

using namespace daisy;
using namespace synthux;

// ============================================================
// HARDWARE
// ============================================================

static DaisySeed hw;
static Touch touch;

static Sequencer sequencer;
static ResonatorBank resonators;
static Character character;

// ============================================================
// SOUND CONTROLS
// ============================================================

static volatile float ctlDecay = 0.45f;
static volatile float ctlDecayWeight = 0.f;

static volatile float ctlFrequencyDistribution = 0.f;

static volatile float ctlCharacter = 0.f;

// ============================================================
// SEQUENCER CONTROLS
// ============================================================

static volatile float ctlTilt = 0.f;
static volatile float ctlBpm = 120.f;

static volatile int ctlLength = 16;
static volatile int ctlY = 0;

static volatile uint8_t ctlAudible = 0;
static volatile bool ctlRunning = false;

// ============================================================
// UTILITIES
// ============================================================

static float Clamp(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

static int Round(float x) {
    return int(x >= 0.f ? x + 0.5f : x - 0.5f);
}

static float Bipolar(float p) {

    const float x = 2.f * p - 1.f;

    return fabsf(x) < 0.035f
        ? 0.f
        : Clamp(x, -1.f, 1.f);
}

// ============================================================
// AUDIO CALLBACK
// ============================================================

static void AudioCallback(
    AudioHandle::InputBuffer in,
    AudioHandle::OutputBuffer out,
    size_t size)
{
    (void)in;

    static uint32_t samplesIntoStep = 0;

    static float decay = 0.45f;
    static float decayWeight = 0.f;

    const float bpm =
        Clamp(ctlBpm, 30.f, 240.f);

    const uint32_t samplesPerStep = uint32_t(
        48000.f * 60.f / (bpm * 4.f) + 0.5f
    );

    const uint8_t audible = ctlAudible;
    const bool running = ctlRunning;

    constexpr float smooth = 0.000174f;

    // ========================================================
    // AUDIO PROCESSING
    // ========================================================

    for(size_t i = 0; i < size; ++i) {

        // ----------------------------------------------------
        // SMOOTH DECAY PARAMETERS
        // ----------------------------------------------------

        decay +=
            (Clamp(ctlDecay, 0.f, 1.f) - decay)
            * smooth;

        decayWeight +=
            (Clamp(ctlDecayWeight, -1.f, 1.f)
             - decayWeight) * smooth;

        // ----------------------------------------------------
        // SEQUENCER
        // ----------------------------------------------------

        if(running) {

            if(samplesIntoStep == 0) {

                const uint8_t hits =
                    sequencer.Advance(
                        ctlLength,
                        ctlTilt,
                        0,
                        ctlY
                    );

                for(int b = 0; b < 7; ++b) {

                    if(hits & (1u << b)) {

                        resonators.Excite(
                            b,
                            1.f,
                            1.f
                        );
                    }
                }
            }

            if(++samplesIntoStep >= samplesPerStep)
                samplesIntoStep = 0;
        }

        // ----------------------------------------------------
        // AUDIO OUTPUT
        // ----------------------------------------------------

        const float output =
            resonators.Process(audible);

        out[0][i] = output;
        out[1][i] = output;
    }

    // ========================================================
    // UPDATE SOUND PARAMETERS
    // ========================================================

    resonators.SetDecay(
        decay,
        decayWeight
    );

    resonators.SetFrequencyDistribution(
        ctlFrequencyDistribution
    );

    // ========================================================
    // CHARACTER
    // ========================================================

    character.SetPosition(ctlCharacter);
    character.Update();

    for(int voice = 0;
        voice < Character::kVoices;
        ++voice) {

        const auto& weights =
            character.Get(voice);

        resonators.SetBodyWeight(
            voice,
            weights.body
        );

        resonators.SetRattleWeight(
            voice,
            weights.rattle
        );

        resonators.SetShimmerWeight(
            voice,
            weights.shimmer
        );
    }
}

// ============================================================
// MAIN
// ============================================================

int main(void)
{
    hw.Init(true);
    hw.SetAudioBlockSize(16);

    hw.SetAudioSampleRate(
        SaiHandle::Config::SampleRate::SAI_48KHZ
    );

    touch.Init(hw);

    sequencer.Init();
    resonators.Init(48000.f);

    // ========================================================
    // TOUCH STABILIZATION
    // ========================================================

    for(int i = 0; i < 40; ++i) {
        touch.Process();
        System::Delay(3);
    }

    hw.StartAudio(AudioCallback);

    bool wasTap = false;
    bool wasStop = false;
    bool wasStart = false;

    uint32_t lastTap = 0;
    float filteredBpm = 120.f;

    // ========================================================
    // MAIN LOOP
    // ========================================================

    while(1) {

        touch.Process();

        // ====================================================
        // SOUND CONTROLS
        // ====================================================

        // S31: Baseline Decay
        ctlDecay = touch.knobs().s31();

        // S32: Frequency Distribution
        ctlFrequencyDistribution =
            touch.knobs().s32();

        // S33: Combined Character
        ctlCharacter = touch.knobs().s33();

        // S34: Available

        // ====================================================
        // SEQUENCER CONTROLS
        // ====================================================

        // S30: Global Sequence Length
        ctlLength = Round(
            Clamp(
                touch.knobs().s30(),
                0.f,
                1.f
            ) * 16.f
        );

        // S35: Tilt
        ctlTilt = Bipolar(
            touch.knobs().s35()
        );

        // ====================================================
        // WEIGHTED DECAY
        // ====================================================

        ctlDecayWeight = Bipolar(
            touch.knobs().LeftFader()
        );

        // ====================================================
        // TEMPORARY Y SHIFT
        // ====================================================

        ctlY = Round(
            Bipolar(
                touch.knobs().RightFader()
            ) * 6.f
        );

        // ====================================================
        // VOICE PADS
        // ====================================================

        uint8_t pads = 0;

        for(int b = 0; b < 7; ++b) {

            if(touch.pads().IsTouched(b + 3))
                pads |= uint8_t(1u << b);
        }

        ctlAudible = pads;

        // ====================================================
        // TRANSPORT
        // ====================================================

        const bool stop =
            touch.pads().IsTouched(10);

        const bool start =
            touch.pads().IsTouched(11);

        const bool tap =
            touch.pads().IsTouched(1);

        if(stop && !wasStop)
            ctlRunning = false;

        if(start && !wasStart)
            ctlRunning = true;

        // ====================================================
        // TAP TEMPO
        // ====================================================

        if(tap && !wasTap) {

            const uint32_t now =
                System::GetNow();

            const uint32_t elapsed =
                now - lastTap;

            if(lastTap &&
               elapsed >= 250u &&
               elapsed <= 2000u) {

                const float instant =
                    60000.f / float(elapsed);

                filteredBpm =
                    0.6f * filteredBpm +
                    0.4f * instant;

                ctlBpm = Clamp(
                    filteredBpm,
                    30.f,
                    240.f
                );
            }
            else {
                filteredBpm = ctlBpm;
            }

            lastTap = now;
        }

        wasStop = stop;
        wasStart = start;
        wasTap = tap;

        // ====================================================
        // LED
        // ====================================================

        hw.SetLed(ctlRunning);

        System::Delay(3);
    }
}
