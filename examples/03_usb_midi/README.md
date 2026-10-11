# USB MIDI Controller with Web Debugger

Turns **Synthux Touch 2** into an expressive, class-compliant USB MIDI controller with continuous per-pad pressure, rotary CCs, fader CCs, 3-position toggle switches, and a companion Web MIDI debugger.

---

## Hardware MIDI Mapping

All messages are transmitted on **MIDI Channel 1**.

### Touch Pads (P00–P11)

| Pad | Physical Location | Note | Note # | Pressure Message | Dedicated CC |
|---|---|---|---|---|---|
| **P00** | Upper Mountain (Left) | C3 | 48 | Poly Pressure (`0xA0`) | CC 30 (0–127) |
| **P01** | Upper Mountain (Center) | D3 | 50 | Poly Pressure (`0xA0`) | CC 31 (0–127) |
| **P02** | Upper Mountain (Right) | E3 | 52 | Poly Pressure (`0xA0`) | CC 32 (0–127) |
| **P03** | Mid Mountain (Far Left) | F3 | 53 | Poly Pressure (`0xA0`) | CC 33 (0–127) |
| **P04** | Mid Mountain (Inner Left) | G3 | 55 | Poly Pressure (`0xA0`) | CC 34 (0–127) |
| **P05** | Mid Mountain (Center Valley) | A3 | 57 | Poly Pressure (`0xA0`) | CC 35 (0–127) |
| **P06** | Mid Mountain (Inner Right) | B3 | 59 | Poly Pressure (`0xA0`) | CC 36 (0–127) |
| **P07** | Mid Mountain (Far Right) | C4 | 60 | Poly Pressure (`0xA0`) | CC 37 (0–127) |
| **P08** | Lower Mountain (Left) | D4 | 62 | Poly Pressure (`0xA0`) | CC 38 (0–127) |
| **P09** | Lower Mountain (Right) | E4 | 64 | Poly Pressure (`0xA0`) | CC 39 (0–127) |
| **P10** | Top Left Utility Pad | C2 | 36 | Poly Pressure (`0xA0`) | CC 40 (0–127) |
| **P11** | Top Right Utility Pad | D2 | 38 | Poly Pressure (`0xA0`) | CC 41 (0–127) |

*Note: In addition to Poly Pressure (`0xA0`) and dedicated CCs (30–41), Channel Pressure (`0xD0`) is also broadcast matching the highest active pad pressure for universal DAW compatibility.*

### Knobs & Faders (S30–S37)

| Control | Description | Hardware Pin | MIDI CC | Range |
|---|---|---|---|---|
| **S30** | Rotary Potentiometer | ADC A0 | **CC 14** | 0–127 |
| **S31** | Rotary Potentiometer | ADC A1 | **CC 15** | 0–127 |
| **S32** | Rotary Potentiometer | ADC A2 | **CC 16** | 0–127 |
| **S33** | Rotary Potentiometer | ADC A3 | **CC 17** | 0–127 |
| **S34** | Rotary Potentiometer | ADC A4 | **CC 18** | 0–127 |
| **S35** | Rotary Potentiometer | ADC A5 | **CC 19** | 0–127 |
| **S36** | Left Linear Fader | ADC A6 | **CC 1** (Mod Wheel) | 0–127 |
| **S37** | Right Linear Fader | ADC A7 | **CC 21** | 0–127 |

### 3-Position Toggle Switches

| Switch | Location | Hardware Pins | MIDI CC | Positions |
|---|---|---|---|---|
| **Switch A** | Left Toggle (S09 / S10) | D9 (Up), D8 (Down) | **CC 80** | Down = 0, Center = 64, Up = 127 |
| **Switch B** | Right Toggle (S07 / S08) | D7 (Up), D6 (Down) | **CC 81** | Down = 0, Center = 64, Up = 127 |

### Host Synchronization & Calibration

- **CC 120**: Host requests full state dump (re-sends all current knob, fader, and switch values).
- **CC 112**: Host requests MPR121 capacitance baseline recalibration (re-zeroing).

---

## Web MIDI Debugger

Open `webui/index.html` in any browser supporting the Web MIDI API (Google Chrome, Microsoft Edge, Opera, Brave).

1. Connect the Daisy Seed via USB.
2. Open `webui/index.html` in your browser.
3. Select the Daisy Seed MIDI port from the dropdown.
4. The visualizer will reflect pad strikes, continuous pressure rings, knobs, faders, switches, and message throughput in real-time.
5. Click **Sync** to request current hardware values, or **Re-Zero** to recalibrate touch pad baselines.
