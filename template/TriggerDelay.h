#pragma once

#include "FrequencyDistribution.h"

#include <cstdint>
#include <cmath>

class TriggerDelay {
public:

    static constexpr int kVoices = 7;
    static constexpr int kEventsPerVoice = 16;

    static constexpr float kSampleRate = 48000.f;

    static constexpr int kMaxEchoes = 8;

    static constexpr float kPi =
        3.14159265358979323846f;

    // Echoes progressively weaken.
    static constexpr float kEchoAttenuation = 0.78f;

    struct Event {
        uint32_t remaining = 0;
        float velocity = 0.f;
        bool active = false;
    };

    Event events[kVoices][kEventsPerVoice]{};

    float delaySamples[kVoices]{};

    bool affected[kVoices]{};

    bool enabled = false;
    int echoCount = 1;

    // ========================================================
    // UTILITIES
    // ========================================================

    static float Clamp(float x, float lo, float hi) {
        return x < lo ? lo : (x > hi ? hi : x);
    }

    static float Lerp(float a, float b, float t) {
        return a + (b - a) * t;
    }

    // ========================================================
    // RESET
    // ========================================================

    void Clear() {

        for(int v = 0; v < kVoices; ++v) {

            for(int e = 0;
                e < kEventsPerVoice;
                ++e) {

                events[v][e] = Event{};
            }
        }
    }

    void Init() {

        Clear();

        enabled = false;
        echoCount = 1;

        for(int v = 0; v < kVoices; ++v) {

            affected[v] = false;
            delaySamples[v] = 6000.f;
        }
    }

    // ========================================================
    // CONFIGURATION
    // ========================================================

    void Configure(
        float distribution,
        float feedback,
        float bpm,
        float frequencyDistribution
    ) {

        distribution =
            Clamp(distribution, 0.f, 1.f);

        feedback =
            Clamp(feedback, 0.f, 1.f);

        bpm =
            Clamp(bpm, 30.f, 240.f);

        // A small tolerance for analog control noise.
        if(distribution <= 0.008f) {

            if(enabled)
                Clear();

            enabled = false;

            return;
        }

        enabled = true;

        // Feedback always begins at ONE echo.
        echoCount = 1 + int(
            feedback * float(kMaxEchoes - 1)
            + 0.5f
        );

        // One sixteenth-note reference interval.
        const float referenceSamples =
            kSampleRate * 60.f / (bpm * 4.f);

        // Current frequencies of the seven bands.
        float frequencies[kVoices];

        FrequencyDistribution::Calculate(
            frequencyDistribution,
            frequencies
        );

        // ----------------------------------------------------
        // MOVING SPECTRAL EMPHASIS
        // ----------------------------------------------------

        // As the slider increases, the centre
        // travels from lower to higher bands.
        //
        // A Gaussian distribution provides a
        // narrow emphasis, similar to Character.

        const float centreHz =
            80.f * powf(
                6000.f / 80.f,
                distribution
            );

        float weights[kVoices]{};

        for(int v = 0; v < kVoices; ++v) {

            const float frequency =
                Clamp(
                    frequencies[v],
                    20.f,
                    18000.f
                );

            const float distance =
                log2f(frequency / centreHz);

            const float width = 1.15f;

            const float normalized =
                distance / width;

            weights[v] = expf(
                -0.5f *
                normalized *
                normalized
            );
        }

        // ----------------------------------------------------
        // ISOLATE THREE STRONGEST VOICES
        // ----------------------------------------------------

        bool selected[kVoices]{};

        for(int n = 0; n < 3; ++n) {

            int best = -1;
            float bestWeight = -1.f;

            for(int v = 0; v < kVoices; ++v) {

                if(!selected[v] &&
                   weights[v] > bestWeight) {

                    best = v;
                    bestWeight = weights[v];
                }
            }

            if(best >= 0)
                selected[best] = true;
        }

        float maximum = 0.f;

        for(int v = 0; v < kVoices; ++v) {

            if(weights[v] > maximum)
                maximum = weights[v];
        }

        // ----------------------------------------------------
        // DISTRIBUTE DELAY TIMES
        // ----------------------------------------------------

        for(int v = 0; v < kVoices; ++v) {

            affected[v] = selected[v];

            if(!selected[v] || maximum <= 0.f)
                continue;

            const float emphasis =
                weights[v] / maximum;

            // High emphasis produces faster echoes.
            // Lower emphasis produces slower echoes.
            //
            // Slider movement increases the spread.
            const float ratio = Lerp(
                2.f,
                0.25f,
                emphasis * distribution
            );

            delaySamples[v] =
                referenceSamples * ratio;
        }
    }

    // ========================================================
    // SCHEDULE AN ECHO
    // ========================================================

    void Schedule(
        int voice,
        uint32_t delay,
        float velocity
    ) {

        if(voice < 0 || voice >= kVoices)
            return;

        for(int e = 0;
            e < kEventsPerVoice;
            ++e) {

            Event& event = events[voice][e];

            if(!event.active) {

                event.remaining = delay;
                event.velocity = velocity;
                event.active = true;

                return;
            }
        }

        // Queue full: discard additional echoes.
        // This prevents unbounded event accumulation.
    }

    // ========================================================
    // ORIGINAL TRIGGER
    // ========================================================

    void Trigger(
        int voice,
        float velocity
    ) {

        if(!enabled)
            return;

        if(voice < 0 || voice >= kVoices)
            return;

        if(!affected[voice])
            return;

        float echoVelocity = velocity;

        const uint32_t interval =
            uint32_t(
                delaySamples[voice] + 0.5f
            );

        for(int n = 1; n <= echoCount; ++n) {

            echoVelocity *= kEchoAttenuation;

            Schedule(
                voice,
                interval * uint32_t(n),
                echoVelocity
            );
        }
    }

    // ========================================================
    // PROCESS SCHEDULED TRIGGERS
    // ========================================================

    void Process(float* velocities) {

        for(int v = 0; v < kVoices; ++v)
            velocities[v] = 0.f;

        if(!enabled)
            return;

        for(int v = 0; v < kVoices; ++v) {

            for(int e = 0;
                e < kEventsPerVoice;
                ++e) {

                Event& event = events[v][e];

                if(!event.active)
                    continue;

                if(event.remaining > 0)
                    --event.remaining;

                if(event.remaining == 0) {

                    // If two echoes coincide, use the
                    // stronger velocity rather than
                    // retriggering twice in one sample.
                    if(event.velocity > velocities[v]) {

                        velocities[v] =
                            event.velocity;
                    }

                    event.active = false;
                }
            }
        }
    }
};