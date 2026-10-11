# TouchTemplate

Hardware library and starter template for the **Synthux Touch 2** instrument platform (Daisy Seed).

---

## 1. Hardware Layout

<img src="assets/touch.jpeg" width="350"/>

```
                         Synthux Touch 2
  =========================================================================
  [ S31: Upper Left ]      [ S32: Center Left ]   [ S33: Center Right ]  [ S34: Upper Right ]
  Potentiometer A1         Potentiometer A2       Potentiometer A3       Potentiometer A4

  [ S30: Lower Left ]                                                    [ S35: Lower Right ]
  Potentiometer A0                                                       Potentiometer A5

  [ S36: Left Fader ]                                                    [ S37: Right Fader ]
  Linear Fader A6                                                        Linear Fader A7
  -------------------------------------------------------------------------
  [ Switch A: Left (D09 / D08) ]                    [ Switch B: Right (D07 / D06) ]
  3-Position Toggle (Up / Center / Down)            3-Position Toggle (Up / Center / Down)
  -------------------------------------------------------------------------
  [ P10: Top Left ]                                 [ P11: Top Right ]
  Capacitive Pad 10                                 Capacitive Pad 11

  [ P00: Peak Left ]       [ P01: Peak Center ]     [ P02: Peak Right ]
  Capacitive Pad 0         Capacitive Pad 1         Capacitive Pad 2

        [ P03 ]    [ P04 ]    [ P05 ]    [ P06 ]    [ P07 ]    [ P08 ]    [ P09 ]
        Pad 03     Pad 04     Pad 05     Pad 06     Pad 07     Pad 08     Pad 09
  =========================================================================
```

---

## 2. Quickstart

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/vincentltm/TouchTemplate.git
cd TouchTemplate

# Build libraries and starter project
make libs -j4
make -j4

# Flash to Daisy Seed (hold BOOT, press RESET, release BOOT)
make program-dfu
```

---

## 3. Library API

```cpp
#include "daisy_seed.h"
#include "touch/touch.h"

using namespace daisy;
using namespace synthux;

static DaisySeed hw;
static Touch     touch;

int main(void) {
    hw.Init(true);
    touch.Init(hw);
    ...
```

### Pads (P00–P11)

```cpp
// Continuous pressure: 0.0 to 1.0 (quadratic finger-pulp response)
float p0 = touch.pads()[0];
float p_mid = touch.pads().Pressure(6);

// State queries
bool is_pressed = touch.pads().IsTouched(3);
bool any_active = touch.pads().HasTouch();

// Callbacks
touch.pads().SetOnTouch([](uint16_t pad) {
    // Pad pressed (0..11)
});

touch.pads().SetOnRelease([](uint16_t pad) {
    // Pad released (0..11)
});
```

### Knobs & Faders (0.0 to 1.0)

```cpp
// Rotary potentiometers (S30–S35)
float k0 = touch.knobs().s30();
float k1 = touch.knobs().s31();
float k_idx = touch.knobs().Knob(2); // S32

// Linear faders (S36 & S37)
float fader_l = touch.knobs().LeftFader();  // S36
float fader_r = touch.knobs().RightFader(); // S37

// Direct index access (0..7)
float raw_val = touch.knobs()[0];
```

### 3-Position Toggle Switches

```cpp
// Position queries
bool is_up   = touch.switches().IsAUp();
bool is_down = touch.switches().IsADown();
bool is_mid  = touch.switches().IsACenter();

// Enum values: Switch3::POS_UP, POS_CENTER, POS_DOWN
int pos_a = touch.switches().A();
int pos_b = touch.switches().B();
```

### Soft-Takeover (`MValue`)

Handles multi-function controls (e.g. shift layers) by latching values and requiring physical pickup before updating:

```cpp
static MValue cutoff(0.5f);
static MValue volume(0.85f);

bool is_shift = touch.pads().IsTouched(10);
float raw_fader = touch.knobs().LeftFader();

if (is_shift) {
    float v = volume.Process(raw_fader, true);
    cutoff.Process(raw_fader, false);
} else {
    float c = cutoff.Process(raw_fader, true);
    volume.Process(raw_fader, false);
}
```

### Chord Latch (`Latch`)

Handles chord latching / sustain pedal behavior:

```cpp
static Latch<12> latch;

latch.SetOnNoteOn([](uint8_t note) { ... });
latch.SetOnNoteOff([](uint8_t note) { ... });

// Toggle latch on/off
latch.Toggle();
```

---

## 4. Directory Structure & Examples

```
TouchTemplate/
├── Makefile                   # Top-level build file (delegates to template/)
├── README.md
├── LICENSE
├── assets/
│   └── touch.jpeg             # Hardware PCB diagram
├── template/                  # Standalone starter project directory
│   ├── Makefile
│   └── main.cpp
├── touch/                     # Hardware driver & utilities
│   ├── touch.h                # Single-include header
│   ├── pads.h / pads.cpp      # MPR121 12-pad continuous capacitive driver
│   ├── knobs.h / knobs.cpp    # 8-channel analog input wrapper
│   ├── switches.h / switches.cpp # Dual 3-position toggle switch handler
│   ├── mvalue.h               # Soft takeover utility
│   ├── latch.h                # Chord latch manager
│   └── log.h                  # Serial debug logger
├── examples/
│   ├── 01_hello_hardware/     # Audio I/O passthrough, LED, gain and pan faders
│   ├── 02_simple_synth/       # Basic oscillator, filter cutoff, touch pressure
│   ├── 03_usb_midi/           # Expressive USB MIDI controller (CCs, Aftertouch) + Web MIDI debugger
│   │   └── webui/             # Interactive browser-based MIDI schematic & event logger
│   └── 04_audio_fx/           # Stereo filter and delay with touch freeze/stutter
└── lib/                       # Submodules (libDaisy & DaisySP)
```

To build and flash an example:
```bash
cd examples/03_usb_midi
make -j4
make program-dfu
# Open examples/03_usb_midi/webui/index.html in Chrome/Edge to visualize & debug!
```

---

## 5. License

MIT
