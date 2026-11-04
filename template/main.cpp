#include "daisy_seed.h"
#include "touch/touch.h"

#include "Sequencer.h"
#include "Transport.h"
#include "ResonatorBank.h"
#include "Character.h"
#include "TriggerDelay.h"

#include <cmath>
#include <atomic>

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
static TriggerDelay triggerDelay;

// ============================================================
// SOUND CONTROLS
// ============================================================

static volatile float ctlDecay = 0.45f;
static volatile float ctlStepDecay = 0.f;

static volatile float ctlFrequencyDistribution = 0.f;
static volatile float ctlCharacter = 0.f;

// S32: 0 = external, 1 = internal
static volatile float ctlInputMix = 1.f;

// ============================================================
// SEQUENCER CONTROLS
// ============================================================

static volatile float ctlTilt = 0.f;
static volatile float ctlBpm = 120.f;
static volatile int ctlLength = 16;

// ============================================================
// TRIGGER DELAY CONTROLS
// ============================================================

static volatile float ctlFeedback = 0.f;
static volatile float ctlDelayDistribution = 0.f;

// ============================================================
// TRANSPORT & PERFORMANCE
// ============================================================

static std::atomic<int> ctlTransport{
    Transport::STOP
};

static std::atomic<int> ctlPerformance{0};
static std::atomic<uint8_t> ctlPads{0};

// ============================================================
// PENDING EVENTS
// ============================================================

static std::atomic<uint8_t> pendingRecordEvents{0};
static std::atomic<uint8_t> pendingClearEvents{0};
static std::atomic<uint8_t> pendingManualEvents{0};

static std::atomic<bool> pendingReset{false};

// ============================================================
// CLICK TRACK
// ============================================================

static std::atomic<bool> ctlClickEnabled{false};

static constexpr float kClickGain = 0.12f;
static constexpr float kClickFrequency = 850.f;
static constexpr float kAccentFrequency = 1300.f;
static constexpr float kClickDecay = 0.9974f;

static constexpr float kTwoPi =
    6.28318530717958647692f;

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
    size_t size
)
{
    static uint32_t samplesIntoStep = 0;
    static float decay = 0.45f;

    static int previousTransport =
        Transport::STOP;

    static float clickPhase = 0.f;
    static float clickEnvelope = 0.f;
    static float clickFrequency = kClickFrequency;

    // ========================================================
    // CLOCK
    // ========================================================

    const float bpm =
        Clamp(ctlBpm, 30.f, 240.f);

    const uint32_t samplesPerStep =
        uint32_t(
            48000.f * 60.f /
            (bpm * 4.f) + 0.5f
        );

    const int transportMode =
        ctlTransport.load();

    const int performanceMode =
        ctlPerformance.load();

    const uint8_t heldPads =
        ctlPads.load();

    const bool clickEnabled =
        ctlClickEnabled.load();

    // S32 input crossfade
    const float inputMix =
        Clamp(ctlInputMix, 0.f, 1.f);

    constexpr float smooth = 0.000174f;

    const bool running =
        transportMode != Transport::STOP;

    const bool recording =
        transportMode == Transport::RECORD;

    // ========================================================
    // RESET
    // ========================================================

    if(pendingReset.exchange(false)) {

        for(int b = 0; b < 7; ++b)
            sequencer.ClearVoice(b);

        sequencer.Reset();
        samplesIntoStep = 0;

        pendingRecordEvents.store(0);
        pendingClearEvents.store(0);
        pendingManualEvents.store(0);

        triggerDelay.Clear();
        clickEnvelope = 0.f;
    }

    // ========================================================
    // TRANSPORT
    // ========================================================

    if(transportMode == Transport::STOP &&
       previousTransport != Transport::STOP) {

        sequencer.Reset();
        samplesIntoStep = 0;

        pendingRecordEvents.store(0);
        pendingManualEvents.store(0);

        triggerDelay.Clear();
        clickEnvelope = 0.f;
    }

    previousTransport = transportMode;

    // ========================================================
    // CLEAR
    // ========================================================

    const uint8_t clearEvents =
        pendingClearEvents.exchange(0);

    if(clearEvents != 0) {

        for(int b = 0; b < 7; ++b) {

            if(clearEvents & (1u << b))
                sequencer.ClearVoice(b);
        }

        pendingRecordEvents.fetch_and(
            uint8_t(~clearEvents)
        );
    }

    // ========================================================
    // TRIGGER DELAY CONFIGURATION
    // ========================================================

    triggerDelay.Configure(
        ctlDelayDistribution,
        ctlFeedback,
        bpm,
        ctlFrequencyDistribution
    );

    // ========================================================
    // PERFORMANCE AUDIBILITY
    // ========================================================

    uint8_t audible = 0;

    // Fixed short fades for momentary modes
    const float fadeSeconds = 0.025f;

    if(recording) {

        // All voices audible in RECORD
        audible = 0x7F;
    }
    else if(running) {

        if(performanceMode == 1) {

            // Switch B UP:
            // Momentary unmute
            audible = heldPads;
        }
        else if(performanceMode == 2) {

            // Switch B DOWN:
            // Momentary mute
            audible =
                uint8_t(~heldPads) & 0x7F;
        }
        else {

            // Switch B CENTER:
            // All voices audible
            audible = 0x7F;
        }
    }

    // ========================================================
    // MANUAL TRIGGERS
    // ========================================================

    if(running && !recording &&
       performanceMode == 0) {

        const uint8_t manual =
            pendingManualEvents.exchange(0);

        for(int b = 0; b < 7; ++b) {

            if(!(manual & (1u << b)))
                continue;

            resonators.Excite(
                b,
                1.f,
                1.f,
                0
            );

            triggerDelay.Trigger(b, 1.f);
        }
    }
    else {

        pendingManualEvents.store(0);
    }

    // ========================================================
    // AUDIO PROCESSING
    // ========================================================

    for(size_t i = 0; i < size; ++i) {

        // ----------------------------------------------------
        // DECAY SMOOTHING
        // ----------------------------------------------------

        decay +=
            (
                Clamp(ctlDecay, 0.f, 1.f)
                - decay
            ) * smooth;

        // ----------------------------------------------------
        // SEQUENCER
        // ----------------------------------------------------

        if(running) {

            if(samplesIntoStep == 0) {

                // --------------------------------------------
                // RECORD OVERDUB
                // --------------------------------------------

                if(recording) {

                    const uint8_t events =
                        pendingRecordEvents.exchange(0);

                    if(events != 0) {

                        sequencer.Record(
                            events,
                            ctlLength,
                            ctlTilt,
                            0,
                            0
                        );
                    }
                }
                else {

                    pendingRecordEvents.store(0);
                }

                // --------------------------------------------
                // CURRENT STEP
                // --------------------------------------------

                const uint32_t currentTick =
                    sequencer.tick;

                // --------------------------------------------
                // CLICK
                // --------------------------------------------

                if(clickEnabled &&
                   (currentTick % 4u) == 0u) {

                    const bool downbeat =
                        (currentTick % 16u) == 0u;

                    clickFrequency =
                        downbeat
                            ? kAccentFrequency
                            : kClickFrequency;

                    clickEnvelope = 1.f;
                    clickPhase = 0.f;
                }

                // --------------------------------------------
                // ADVANCE
                // --------------------------------------------

                const uint8_t hits =
                    sequencer.Advance(
                        ctlLength,
                        ctlTilt,
                        0,
                        0
                    );

                // --------------------------------------------
                // ORIGINAL TRIGGERS
                // --------------------------------------------

                for(int b = 0; b < 7; ++b) {

                    if(!(hits & (1u << b)))
                        continue;

                    const int length =
                        Sequencer::LengthForBand(
                            b,
                            ctlLength,
                            ctlTilt
                        );

                    if(length <= 0)
                        continue;

                    const int localStep = int(
                        currentTick %
                        uint32_t(length)
                    );

                    // Trigger the resonator envelope
                    resonators.Excite(
                        b,
                        1.f,
                        1.f,
                        localStep
                    );

                    // Schedule trigger echoes
                    triggerDelay.Trigger(
                        b,
                        1.f
                    );
                }
            }

            if(++samplesIntoStep >= samplesPerStep)
                samplesIntoStep = 0;
        }

        // ----------------------------------------------------
        // DELAYED TRIGGER EVENTS
        // ----------------------------------------------------

        if(running) {

            float echoVelocities[7];

            triggerDelay.Process(
                echoVelocities
            );

            for(int b = 0; b < 7; ++b) {

                if(echoVelocities[b] <= 0.f)
                    continue;

                const int length =
                    Sequencer::LengthForBand(
                        b,
                        ctlLength,
                        ctlTilt
                    );

                if(length <= 0)
                    continue;

                const int localStep = int(
                    sequencer.tick %
                    uint32_t(length)
                );

                // Fresh excitation from each echo
                resonators.Excite(
                    b,
                    echoVelocities[b],
                    1.f,
                    localStep
                );
            }
        }

        // ====================================================
        // EXTERNAL AUDIO INPUT
        // ====================================================

        // Read the incoming audio from channel 0.
        // The signal is distributed across all 7 SVFs.

        const float externalAudio =
            in[0][i];

        // ====================================================
        // RESONATOR PROCESSING
        // ====================================================

        // S32 determines the source mixture:
        //
        // 0   = 100% external
        // 0.5 = 50% internal / 50% external
        // 1   = 100% internal

        float output =
            resonators.Process(
                audible,
                fadeSeconds,
                externalAudio,
                inputMix
            );

        // ----------------------------------------------------
        // CLICK AUDIO
        // ----------------------------------------------------

        if(running && clickEnabled) {

            if(clickEnvelope > 0.0001f) {

                output +=
                    sinf(clickPhase) *
                    clickEnvelope *
                    kClickGain;

                clickPhase +=
                    kTwoPi *
                    clickFrequency /
                    48000.f;

                if(clickPhase >= kTwoPi)
                    clickPhase -= kTwoPi;

                clickEnvelope *=
                    kClickDecay;
            }
            else {

                clickEnvelope = 0.f;
            }
        }
        else {

            clickEnvelope = 0.f;
        }

        // ----------------------------------------------------
        // AUDIO OUTPUT
        // ----------------------------------------------------

        output = Clamp(
            output,
            -1.f,
            1.f
        );

        out[0][i] = output;
        out[1][i] = output;
    }

    // ========================================================
    // UPDATE SOUND PARAMETERS
    // ========================================================

    resonators.SetDecay(
        decay,
        ctlStepDecay
    );

    resonators.SetFrequencyDistribution(
        ctlFrequencyDistribution
    );

    // ========================================================
    // CHARACTER DISTRIBUTION
    // ========================================================

    character.SetPosition(
        ctlCharacter
    );

    character.Update(
        ctlFrequencyDistribution
    );

    for(int voice = 0; voice < 7; ++voice) {

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

        resonators.SetCrushWeight(
            voice,
            weights.crush
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
    triggerDelay.Init();

    // ========================================================
    // TOUCH STABILIZATION
    // ========================================================

    for(int i = 0; i < 40; ++i) {

        touch.Process();
        System::Delay(3);
    }

    hw.StartAudio(AudioCallback);

    // ========================================================
    // PAD STATES
    // ========================================================

    uint8_t previousVoicePads = 0;

    bool wasClick = false;
    bool wasTap = false;

    uint32_t lastTap = 0;
    float filteredBpm = 120.f;

    // ========================================================
    // RESET STATE
    // ========================================================

    bool resetWasHeld = false;
    bool resetTriggered = false;

    uint32_t resetStart = 0;

    // ========================================================
    // MAIN LOOP
    // ========================================================

    while(1) {

        touch.Process();

        // ====================================================
        // SOUND CONTROLS
        // ====================================================

        // S30: Frequency Distribution
        ctlFrequencyDistribution =
            touch.knobs().s30();

        // S31: Global Decay
        ctlDecay =
            touch.knobs().s31();

        // S32: Internal / External Mix
        //
        // CCW = External
        // CW  = Internal

        ctlInputMix =
            touch.knobs().s32();

        // Left slider: Character
        ctlCharacter =
            touch.knobs().LeftFader();

        // ====================================================
        // SEQUENCER CONTROLS
        // ====================================================

        // S34: Global Sequence Length
        ctlLength = Round(
            Clamp(
                touch.knobs().s34(),
                0.f,
                1.f
            ) * 16.f
        );

        // S35: Tilt
        ctlTilt = Bipolar(
            touch.knobs().s35()
        );

        // ====================================================
        // TRIGGER DELAY CONTROLS
        // ====================================================

        // S33: Feedback (1–8 echoes)
        ctlFeedback =
            touch.knobs().s33();

        // Right slider: Delay-time distribution
        // 0 = no delay
        ctlDelayDistribution =
            touch.knobs().RightFader();

        // ====================================================
        // TRANSPORT SWITCH A
        // ====================================================

        int newTransport =
            Transport::STOP;

        if(touch.switches().IsAUp()) {

            newTransport =
                Transport::RECORD;
        }
        else if(touch.switches().IsACenter()) {

            newTransport =
                Transport::PLAY;
        }

        ctlTransport.store(
            newTransport
        );

        // ====================================================
        // PERFORMANCE SWITCH B
        // ====================================================

        int performanceMode = 0;

        if(touch.switches().IsBUp()) {

            // Momentary unmute
            performanceMode = 1;
        }
        else if(touch.switches().IsBDown()) {

            // Momentary mute
            performanceMode = 2;
        }

        ctlPerformance.store(
            performanceMode
        );

        // ====================================================
        // VOICE PADS
        // ====================================================

        uint8_t voicePads = 0;

        for(int b = 0; b < 7; ++b) {

            if(touch.pads().IsTouched(b + 3)) {

                voicePads |=
                    uint8_t(1u << b);
            }
        }

        ctlPads.store(
            voicePads
        );

        const uint8_t newPresses =
            voicePads &
            uint8_t(~previousVoicePads);

        previousVoicePads = voicePads;

        // ====================================================
        // CLEAR - PAD 0
        // ====================================================

        const bool clearHeld =
            touch.pads().IsTouched(0);

        if(clearHeld) {

            if(newPresses != 0) {

                pendingClearEvents.fetch_or(
                    newPresses
                );

                pendingRecordEvents.fetch_and(
                    uint8_t(~newPresses)
                );

                pendingManualEvents.fetch_and(
                    uint8_t(~newPresses)
                );
            }
        }

        // ====================================================
        // RECORDING
        // ====================================================

        else if(newTransport ==
                Transport::RECORD) {

            if(newPresses != 0) {

                pendingRecordEvents.fetch_or(
                    newPresses
                );
            }
        }

        // ====================================================
        // MANUAL TRIGGERING
        // ====================================================

        else if(newTransport ==
                    Transport::PLAY &&
                performanceMode == 0) {

            if(newPresses != 0) {

                pendingManualEvents.fetch_or(
                    newPresses
                );
            }
        }

        // ====================================================
        // FULL RESET - PAD 1
        // ====================================================

        const bool resetHeld =
            touch.pads().IsTouched(1);

        if(resetHeld && !resetWasHeld) {

            resetStart = System::GetNow();
            resetTriggered = false;
        }

        if(resetHeld && !resetTriggered) {

            if(System::GetNow() - resetStart >= 2000u) {

                resetTriggered = true;

                pendingRecordEvents.store(0);
                pendingManualEvents.store(0);
                pendingClearEvents.store(0);

                ctlClickEnabled.store(false);
                pendingReset.store(true);
            }
        }

        if(!resetHeld)
            resetTriggered = false;

        resetWasHeld = resetHeld;

        // ====================================================
        // CLICK TRACK - PAD 10
        // ====================================================

        const bool clickPad =
            touch.pads().IsTouched(10);

        if(clickPad && !wasClick) {

            ctlClickEnabled.store(
                !ctlClickEnabled.load()
            );
        }

        wasClick = clickPad;

        // ====================================================
        // TAP TEMPO - PAD 11
        // ====================================================

        const bool tap =
            touch.pads().IsTouched(11);

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

        wasTap = tap;

        // ====================================================
        // LED
        // ====================================================

        hw.SetLed(
            newTransport != Transport::STOP
        );

        System::Delay(3);
    }
}