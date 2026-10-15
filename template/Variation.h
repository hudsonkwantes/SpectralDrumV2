
#pragma once
#include <cstdint>

// Deterministic initial effect distributions.
class Variation {
public:
    static constexpr uint8_t kSerialDefault   = 0b0101010;
    static constexpr uint8_t kFeedbackDefault = 0b0010101;
    static constexpr uint8_t kAccentDefault   = 0b1010010;
    static constexpr uint8_t kRatchetDefault  = 0b0101001;

    uint8_t serial   = kSerialDefault;
    uint8_t feedback = kFeedbackDefault;
    uint8_t accent   = kAccentDefault;
    uint8_t ratchet  = kRatchetDefault;

    // 0 = clean/normal
    // 1 = serial/accent
    // 2 = feedback/ratchet
    void Toggle(int band,uint8_t resonatorMode,
                uint8_t rhythmMode) {
        if(band < 0 || band >= 7) return;

        const uint8_t bit=uint8_t(1u<<band);

        if(resonatorMode==1) serial ^= bit;
        if(resonatorMode==2) feedback ^= bit;
        if(rhythmMode==1) accent ^= bit;
        if(rhythmMode==2) ratchet ^= bit;
    }
};
