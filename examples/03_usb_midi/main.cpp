#include "daisy_seed.h"
#include "touch/touch.h"
#include <cmath>
#include <array>

using namespace daisy;
using namespace synthux;

static DaisySeed      hw;
static Touch          touch;
static MidiUsbHandler midi;

// MIDI Note assignments for pads P00–P11 (C3..E4, C2, D2)
static const uint8_t kNotes[12] = {
    48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 36, 38
};

// MIDI CC mappings for 8 analog controls (S30–S35 rotary knobs + S36, S37 faders)
static const uint8_t kKnobCC[8] = { 14, 15, 16, 17, 18, 19, 1, 21 };

// Dedicated CC numbers for per-pad continuous pressure (CC 30 to CC 41)
static constexpr uint8_t kPadPressureCCBase = 30;

// Switch CC mappings (Down=0, Center=64, Up=127)
static constexpr uint8_t kSwitchACC = 80;
static constexpr uint8_t kSwitchBCC = 81;

static uint8_t last_knob_val[8] = { 255, 255, 255, 255, 255, 255, 255, 255 };
static uint8_t last_poly_p[12] = { 0 };
static uint8_t last_channel_p = 0;
static int     last_switch_a = -1;
static int     last_switch_b = -1;

static void SendMidi3(uint8_t status, uint8_t d1, uint8_t d2) {
    uint8_t msg[3] = { status, d1, d2 };
    midi.SendMessage(msg, 3);
}

static void SendMidi2(uint8_t status, uint8_t d1) {
    uint8_t msg[2] = { status, d1 };
    midi.SendMessage(msg, 2);
}

static void DumpAllControls() {
    for (size_t k = 0; k < 8; k++) {
        uint8_t val = static_cast<uint8_t>(touch.knobs()[k] * 127.0f + 0.5f);
        if (val > 127) val = 127;
        last_knob_val[k] = val;
        SendMidi3(0xB0, kKnobCC[k], val);
        System::DelayUs(250);
    }

    int sw_a = touch.switches().A();
    last_switch_a = sw_a;
    uint8_t val_a = (sw_a == Switch3::POS_UP) ? 127 :
                    (sw_a == Switch3::POS_CENTER) ? 64 : 0;
    SendMidi3(0xB0, kSwitchACC, val_a);
    System::DelayUs(250);

    int sw_b = touch.switches().B();
    last_switch_b = sw_b;
    uint8_t val_b = (sw_b == Switch3::POS_UP) ? 127 :
                    (sw_b == Switch3::POS_CENTER) ? 64 : 0;
    SendMidi3(0xB0, kSwitchBCC, val_b);
    System::DelayUs(250);
}

int main(void) {
    hw.Init(true);

    // Initialize USB MIDI transport
    MidiUsbHandler::Config midi_cfg;
    midi_cfg.transport_config.periph = MidiUsbTransport::Config::INTERNAL;
    midi.Init(midi_cfg);
    midi.StartReceive();

    touch.Init(hw);

    // Note On & initial pressure when pad is touched
    touch.pads().SetOnTouch([](uint16_t pad) {
        if (pad < 12) {
            float p = touch.pads()[pad];
            uint8_t vel = static_cast<uint8_t>(35.0f + p * 92.0f + 0.5f);
            if (vel < 1) vel = 1;
            if (vel > 127) vel = 127;
            SendMidi3(0x90, kNotes[pad], vel);

            uint8_t press = static_cast<uint8_t>(p * 127.0f + 0.5f);
            if (press > 127) press = 127;
            SendMidi3(0xA0, kNotes[pad], press);
            SendMidi3(0xB0, kPadPressureCCBase + pad, press);
            last_poly_p[pad] = press;
        }
    });

    // Note Off & clear pressure when pad is released
    touch.pads().SetOnRelease([](uint16_t pad) {
        if (pad < 12) {
            SendMidi3(0x80, kNotes[pad], 0);
            SendMidi3(0xA0, kNotes[pad], 0);
            SendMidi3(0xB0, kPadPressureCCBase + pad, 0);
            last_poly_p[pad] = 0;
        }
    });

    // Baseline capacitance settling delay
    for (int i = 0; i < 40; i++) {
        touch.Process();
        System::Delay(3);
    }

    // Broadcast all current control positions
    DumpAllControls();

    while (1) {
        touch.Process();

        // 0. Process incoming MIDI commands from host (WebUI / DAW)
        midi.Listen();
        while (midi.HasEvents()) {
            MidiEvent ev = midi.PopEvent();
            if (ev.type == ControlChange) {
                ControlChangeEvent cc = ev.AsControlChange();
                if (cc.control_number == 112) {
                    // Re-zero / re-calibrate capacitive touch baseline
                    touch.pads().Recalibrate();
                } else if (cc.control_number == 120) {
                    // Host requested full control dump
                    DumpAllControls();
                }
            }
        }

        // 1. Continuous Pad Pressure: Poly Aftertouch + Dedicated CC (30–41) + Channel Aftertouch
        uint8_t max_active_p = 0;
        for (uint16_t p = 0; p < 12; p++) {
            if (touch.pads().IsTouched(p)) {
                uint8_t cur_p = static_cast<uint8_t>(touch.pads()[p] * 127.0f + 0.5f);
                if (cur_p > 127) cur_p = 127;
                if (cur_p > max_active_p) {
                    max_active_p = cur_p;
                }
                if (cur_p != last_poly_p[p]) {
                    last_poly_p[p] = cur_p;
                    SendMidi3(0xA0, kNotes[p], cur_p);
                    SendMidi3(0xB0, kPadPressureCCBase + p, cur_p);
                }
            }
        }

        if (max_active_p != last_channel_p) {
            last_channel_p = max_active_p;
            SendMidi2(0xD0, last_channel_p);
        }

        // 2. Control Change (CC) on knob & fader change
        for (size_t k = 0; k < 8; k++) {
            uint8_t val = static_cast<uint8_t>(touch.knobs()[k] * 127.0f + 0.5f);
            if (val > 127) val = 127;
            if (val != last_knob_val[k]) {
                last_knob_val[k] = val;
                SendMidi3(0xB0, kKnobCC[k], val);
            }
        }

        // 3. Toggle switches
        int sw_a = touch.switches().A();
        if (sw_a != last_switch_a) {
            last_switch_a = sw_a;
            uint8_t val = (sw_a == Switch3::POS_UP) ? 127 :
                          (sw_a == Switch3::POS_CENTER) ? 64 : 0;
            SendMidi3(0xB0, kSwitchACC, val);
        }

        int sw_b = touch.switches().B();
        if (sw_b != last_switch_b) {
            last_switch_b = sw_b;
            uint8_t val = (sw_b == Switch3::POS_UP) ? 127 :
                          (sw_b == Switch3::POS_CENTER) ? 64 : 0;
            SendMidi3(0xB0, kSwitchBCC, val);
        }

        // 4. LED feedback
        hw.SetLed(touch.pads().HasTouch());

        System::Delay(3);
    }
}
