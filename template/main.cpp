
#include "daisy_seed.h"
#include "touch/touch.h"
#include "Sequencer.h"
#include "ResonatorBank.h"
#include "Variation.h"
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
static Variation variation;

// ============================================================
// CONTROLS
// ============================================================

static volatile float ctlEnergy = .45f;
static volatile float ctlShift = .5f;
static volatile float ctlSpread = .65f;
static volatile float ctlFold = .25f;

static volatile float ctlTilt = 0.f;
static volatile float ctlBpm = 120.f;

static volatile int ctlLength = 16;
static volatile int ctlX = 0;
static volatile int ctlY = 0;

static volatile uint8_t ctlAudible = 0;
static volatile uint8_t ctlResMode = 0;
static volatile uint8_t ctlRhythmMode = 0;

static volatile uint8_t ctlSerial =
    Variation::kSerialDefault;
static volatile uint8_t ctlFeedback =
    Variation::kFeedbackDefault;
static volatile uint8_t ctlAccent =
    Variation::kAccentDefault;
static volatile uint8_t ctlRatchet =
    Variation::kRatchetDefault;

static volatile bool ctlRunning = false;

static float Clamp(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

static int Round(float x) {
    return int(x >= 0.f ? x + .5f : x - .5f);
}

static float Bipolar(float p) {
    const float x = 2.f * p - 1.f;
    return fabsf(x) < .035f
        ? 0.f : Clamp(x, -1.f, 1.f);
}

// ============================================================
// AUDIO CALLBACK
// ============================================================

static void AudioCallback(
    AudioHandle::InputBuffer in,
    AudioHandle::OutputBuffer out,
    size_t size)
{
    static uint32_t samplesIntoStep = 0;
    static uint8_t pendingRatchets = 0;
    static uint32_t ratchetOffset = 0;

    // External signal detector
    static float inputPower = 0.f;
    static uint32_t signalHold = 0;
    static bool inputActive = false;

    constexpr float detectOn = .008f;
    constexpr float detectOff = .004f;
    constexpr uint32_t holdSamples = 48000u * 3u;

    // Smoothed macro parameters
    static float externalMix = 0.f;
    static float decay = .45f;
    static float strength = 1.f;

    const float macro = Clamp(ctlEnergy, 0.f, 1.f);

    const float bpm = Clamp(ctlBpm, 30.f, 240.f);
    const uint32_t samplesPerStep = uint32_t(
        48000.f * 60.f / (bpm * 4.f) + .5f
    );

    const uint32_t halfStep = samplesPerStep / 2;

    const uint8_t rhythmMode = ctlRhythmMode;
    const uint8_t rhythmMask =
        rhythmMode == 1 ? ctlAccent :
        rhythmMode == 2 ? ctlRatchet : 0;

    const uint8_t resMode = ctlResMode;
    const uint8_t couplingMask =
        resMode == 1 ? ctlSerial :
        resMode == 2 ? ctlFeedback : 0;

    const uint8_t audible = ctlAudible;
    const bool running = ctlRunning;

    for(size_t i = 0; i < size; ++i) {

        const float input = in[0][i];

        // ----------------------------------------------------
        // DETECT EXTERNAL AUDIO
        // ----------------------------------------------------

        inputPower =
            .9995f * inputPower +
            .0005f * input * input;

        const float threshold =
            inputActive ? detectOff : detectOn;

        if(inputPower > threshold * threshold) {
            signalHold = holdSamples;
            inputActive = true;
        }
        else if(signalHold > 0) {
            --signalHold;
        }
        else {
            inputActive = false;
        }

        // ----------------------------------------------------
        // S31: ENERGY / EXTERNAL CROSSFADE
        // ----------------------------------------------------

        float targetMix = 0.f;
        float targetDecay = decay;
        float targetStrength = strength;

        if(inputActive) {
            // External signal detected:
            // S31 crossfades from internal ping
            // to envelope-gated external spectral audio.
            targetMix = macro;

            // Retain last internal decay and strength,
            // instead of forcing maximum resonance.
        }
        else {
            // Without input, S31 controls both
            // internal impulse strength and decay.
            targetMix = 0.f;
            targetDecay = macro;
            targetStrength = .35f + 1.65f * macro;
        }

        // Approximately 120 ms smoothing.
        constexpr float smooth = .000174f;

        externalMix +=
            (targetMix - externalMix) * smooth;
        decay +=
            (targetDecay - decay) * smooth;
        strength +=
            (targetStrength - strength) * smooth;

        // ----------------------------------------------------
        // GLOBAL SEQUENCER
        // ----------------------------------------------------

        if(running) {
            if(samplesIntoStep == 0) {
                const uint8_t hits = sequencer.Advance(
                    ctlLength, ctlTilt, ctlX, ctlY
                );

                pendingRatchets =
                    rhythmMode == 2
                    ? uint8_t(hits & rhythmMask)
                    : 0;

                ratchetOffset = halfStep;

                for(int b = 0; b < 7; ++b) {
                    if(hits & (1u << b)) {
                        const float velocity =
                            (rhythmMode == 1 &&
                             (rhythmMask & (1u << b)))
                            ? 1.5f : 1.f;

                        resonators.Excite(
                            b, velocity, strength
                        );
                    }
                }
            }

            if(pendingRatchets &&
               samplesIntoStep == ratchetOffset) {
                for(int b = 0; b < 7; ++b) {
                    if(pendingRatchets & (1u << b))
                        resonators.Excite(
                            b, .65f, strength
                        );
                }
                pendingRatchets = 0;
            }

            if(++samplesIntoStep >= samplesPerStep)
                samplesIntoStep = 0;
        }

        // ----------------------------------------------------
        // OUTPUT
        // ----------------------------------------------------

        const float output = resonators.Process(
            audible,
            ctlFold,
            resMode,
            couplingMask,
            input,
            externalMix
        );

        out[0][i] = output;
        out[1][i] = output;
    }

    resonators.SetSound(
        decay, ctlShift, ctlSpread
    );
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

    for(int i = 0; i < 40; ++i) {
        touch.Process();
        System::Delay(3);
    }

    hw.StartAudio(AudioCallback);

    bool wasTap = false;
    bool wasStop = false;
    bool wasStart = false;
    bool wasBand[7]{};

    uint32_t lastTap = 0;
    float filteredBpm = 120.f;

    while(1) {
        touch.Process();

        // Sound parameters
        ctlEnergy = touch.knobs().s31();
        ctlShift = touch.knobs().s32();
        ctlSpread = touch.knobs().s33();
        ctlFold = touch.knobs().s34();

        // Sequencer parameters
        ctlLength = Round(
            Clamp(touch.knobs().s30(), 0.f, 1.f) * 16.f
        );
        ctlTilt = Bipolar(touch.knobs().s35());

        ctlX = Round(
            Bipolar(touch.knobs().LeftFader()) * 15.f
        );
        ctlY = Round(
            Bipolar(touch.knobs().RightFader()) * 6.f
        );

        // Switch A: Clean / Serial / Feedback
        const uint8_t rm =
            touch.switches().IsAUp() ? 1 :
            touch.switches().IsADown() ? 2 : 0;

        // Switch B: Normal / Accent / Ratchet
        const uint8_t tm =
            touch.switches().IsBUp() ? 1 :
            touch.switches().IsBDown() ? 2 : 0;

        ctlResMode = rm;
        ctlRhythmMode = tm;

        // P02: effect assignment modifier
        const bool editing =
            touch.pads().IsTouched(2);

        uint8_t pads = 0;

        for(int b = 0; b < 7; ++b) {
            const bool pressed =
                touch.pads().IsTouched(b + 3);

            if(editing && pressed && !wasBand[b])
                variation.Toggle(b, rm, tm);

            if(pressed && !editing)
                pads |= uint8_t(1u << b);

            wasBand[b] = pressed;
        }

        ctlAudible = pads;

        ctlSerial = variation.serial;
        ctlFeedback = variation.feedback;
        ctlAccent = variation.accent;
        ctlRatchet = variation.ratchet;

        // Transport
        const bool stop = touch.pads().IsTouched(10);
        const bool start = touch.pads().IsTouched(11);
        const bool tap = touch.pads().IsTouched(1);

        if(stop && !wasStop)
            ctlRunning = false;

        if(start && !wasStart)
            ctlRunning = true;

        // Tap BPM
        if(tap && !wasTap) {
            const uint32_t now = System::GetNow();
            const uint32_t elapsed = now - lastTap;

            if(lastTap &&
               elapsed >= 250u &&
               elapsed <= 2000u) {
                const float instant =
                    60000.f / float(elapsed);

                filteredBpm =
                    .6f * filteredBpm +
                    .4f * instant;

                ctlBpm = Clamp(
                    filteredBpm, 30.f, 240.f
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

        hw.SetLed(ctlRunning);
        System::Delay(3);
    }
}
