# DevLab_AudioInput

Arduino library for capturing mono audio from I2S and PDM microphones on ESP32,
RP2040 and RP2350.
Both inputs return signed 16-bit PCM samples through `read()`.

> [!NOTE]
> **Hardware tested:** ESP32-S3 (I2S and PDM) and RP2350 PDM on the
> UNIT Pulsar RP2350A (ICS-41350 microphone), with the UNIT Electronics core
> 3.2.0. RP PDM was tested from 16 kHz up to 48 kHz. RP2040 and RP I2S have only
> been compile tested.

The I2S path receives 32-bit Philips-format data, as used by INMP441 and
ICS-43434 microphones, and shifts it to PCM16. On ESP32, PDM uses the chip's
hardware PDM-to-PCM converter. PDM PCM capture therefore requires a
target with that converter, such as ESP32-S3; ESP32-C6 can use the I2S path but
does not support this library's PDM PCM path. On RP2040/RP2350, the Arduino-Pico
core implements PDM and I2S input using PIO.

## Installation

Copy `unit_devlab_audio_input_library` into the Arduino sketchbook `libraries`
directory and restart the IDE. The examples are then available under
**File > Examples > DevLab_AudioInput > PDM** or **I2S**. Supported cores are
ESP32 Arduino (compiled with 3.3.1) and the UNIT Electronics `rp2040` core
**3.2.0 or later**, which is required for RP2040/RP2350. Install it from the
Boards Manager before using the library on those boards. The Arduino mbed
RP2040 core is not supported by this backend.

## Connections used by the examples

| Input | Microphone signal | ESP32-S3 GPIO | ESP32-C6 GPIO | RP2040/RP2350 GPIO |
| --- | --- | ---: | ---: | ---: |
| I2S | BCLK/SCK | 36 | 6 | 0 |
| I2S | WS/LRCLK | 37 | 7 | 1 |
| I2S | DATA/SD | 35 | 5 | 2 |
| I2S | L/R/SEL | 34 | 4 | 3 |
| PDM | CLK | 2 | — | 10 |
| PDM | DATA/DIN | 3 | — | 11 |

Connect microphone VDD to 3.3 V and GND to GND. The examples drive L/R HIGH
to select the RIGHT channel. Change the pin constants in each example if your
board is wired differently. For I2S, the microphone's
L/R level and the selected left/right channel must agree. On RP2040/RP2350,
`WS` must be the GPIO immediately after `BCLK` (`WS = BCLK + 1`). GPIO10/11
are the PDM pins from the supplied RP board sketch; check your board's pinout
if its microphone is wired differently.

## Basic use

```cpp
#include <DevLab_AudioInput.h>

DevLab_AudioInput mic;
int16_t audio[512];

void setup() {
  pinMode(34, OUTPUT);
  digitalWrite(34, HIGH);
  if (!mic.beginI2S(GPIO_NUM_36, GPIO_NUM_37, GPIO_NUM_35,
                    48000, I2S_STD_SLOT_RIGHT)) {
    while (true) delay(1000);
  }
}

void loop() {
  size_t samples = mic.read(audio, 512);
  // Process samples in audio[0] through audio[samples - 1].
}
```

For ESP32-S3 PDM, call `mic.beginPDM(GPIO_NUM_2, GPIO_NUM_3, 48000,
I2S_PDM_SLOT_LEFT)` instead. Use `mic.end()` before changing the input source.
`setGainPercent(100)` leaves the PCM level unchanged; `setI2SShift(10)` is
the default conversion shift for I2S microphones in this project.

For RP2040/RP2350 using an Arduino-Pico compatible core:

```cpp
#include <DevLab_AudioInput.h>

DevLab_AudioInput mic;
int16_t audio[256];

void setup() {
  if (!mic.beginPDM(10, 11, 8000)) {
    while (true) delay(1000);
  }
}

void loop() {
  size_t samples = mic.read(audio, 256);
  // Process audio[0] through audio[samples - 1].
}
```

For experimental RP I2S, call `mic.beginI2S(0, 1, 2, 48000, true, 16)`
instead and drive the microphone SEL pin on GPIO3 HIGH to select the right
channel. The fifth argument is `true` for right or `false` for left; the sixth
is the conversion shift from 32-bit I2S words to PCM16. RP PDM uses
the core's single global `PDM` instance, so only one PDM input can be active.

### RP PDM sample rates

`beginPDM()` accepts sample rates from 8 kHz up to 48 kHz with the UNIT
Electronics core 3.2.0 or later. The examples use 24 kHz on RP; to use 48 kHz,
set `SAMPLE_RATE` to 48000 in `PDM/ComputerAudio` and run `pc_audio.py` without
`--rate`.

### RP PDM channel (left/right)

A PDM microphone drives its data line only in half of each clock cycle: SELECT
low (left) and SELECT high (right) use opposite halves. The Arduino-Pico PDM
core samples at one fixed clock edge, so `beginPDM()` takes an optional last
argument:

```cpp
mic.beginPDM(10, 11, 16000);        // default: core's sampling, nothing changed
mic.beginPDM(10, 11, 16000, true);  // microphone with SELECT/LR high (right)
```

`rightChannel = true` inverts the CLK pin output after the core starts, which
moves the core's sampling edge to the other half of the cycle. It is off by
default, so existing sketches behave as before. This is experimental: it
compiles on RP2350 and RP2040, but it has not been verified on hardware yet. If
a right-channel microphone sounds worse with `true`, leave it `false`. ESP32
selects the channel with the `slot` argument of `beginPDM()` instead.

### RP PDM gain

The level of RP PDM audio depends on the sample rate. Measured on the UNIT
Pulsar RP2350A with a voice at a normal distance:

| Sample rate | Suggested `setGainPercent()` | Result |
| --- | ---: | --- |
| 8-25 kHz | 100 (default) | Voice peaks near 27000, no clipping |
| 26-48 kHz | about 400 | The core halves its decimation above ~25 kHz, so the output is about 4-6x lower |

At 16 kHz, a gain of 800% clipped about 4% of the samples of a normal voice.

## Examples

The library keeps its I2S and PDM implementations in separate `src/I2S` and
`src/PDM` folders. The public `DevLab_AudioInput.h` API is shared by both.

### I2S

| Sketch in `examples/I2S` | Serial output |
| --- | --- |
| `BasicRead` | Peak level as text, 115200 baud |
| `SerialPlotter` | Decimated samples as text, 115200 baud |
| `SerialStream` | PCM16 mono, 48 kHz, 2000000 baud |
| `ComputerAudio` | PCM16 mono, 16 kHz, 921600 baud |

### PDM

| Sketch in `examples/PDM` | Serial output |
| --- | --- |
| `BasicRead` | Peak level as text, 115200 baud |
| `SerialPlotter` | Decimated samples as text, 115200 baud |
| `SerialSamples` | Every PCM16 sample as text, 8 kHz, 115200 baud |
| `SerialStream` | PCM16 mono, 48 kHz on ESP32-S3 or 8 kHz on RP, 2000000 baud |
| `ComputerAudio` | PCM16 mono, 48 kHz on ESP32-S3 or 24 kHz on RP, 921600 baud |

The serial stream sketches send a `RAW` line before audio. Send `R` to
resynchronize and resume streaming, or `S` to stop it. In `I2S/SerialStream`,
`G10\n` sets the I2S shift
to 10; in `PDM/SerialStream`, `G200\n` sets software gain to 200%.
The stream is little-endian PCM16 without text delimiters between samples.
Close Arduino Serial Monitor before connecting another application to the
serial port. On ESP32, configure the receiver for 48 kHz and 2000000 baud.
The RP PDM stream is 8 kHz, so configure the receiver for 8 kHz instead.
`PDM/SerialSamples` uses text for inspection, not audio playback.

Each `ComputerAudio` example has a companion `pc_audio.py` in its directory.
Install `pyserial` and `sounddevice` on the computer, upload the I2S or PDM
`ComputerAudio.ino` sketch, and run `python3 pc_audio.py /dev/ttyACM0` from
that example's directory (native USB; use `/dev/ttyUSB0` or your port with a
USB-serial chip). The I2S example uses 16 kHz and the PDM example 48 kHz (24 kHz on RP, so run
`pc_audio.py --rate 24000` there), all mono PCM16 at 921600 baud. Close Arduino
Serial Monitor before running the script.

The `ComputerAudio` sketches send each block as a frame with a checksum
(`'A' 'U'`, sample count, samples, checksum). `pc_audio.py` discards damaged
frames, resyncs if the USB driver loses bytes, and requests the stream again
if the board stops sending.

If the I2S signal is too quiet or clips, adjust `I2S_SHIFT` in its sketch. The
PDM `pc_audio.py` also removes the microphone's DC offset and accepts:

| Option | Effect |
| --- | --- |
| `--rate <hz>` | Sample rate, must match `SAMPLE_RATE` in the sketch (default 48000; 24000 on RP) |
| `--gain <n>` | Software gain applied on the computer (default 1 = unchanged; raise it to taste) |
| `--meter` | Prints the input level (RMS and peak) and the output peak twice a second, to tune `--gain` |
| `--device <name>` | Output device, e.g. a virtual microphone sink |

Gain is applied in two stages that multiply: `GAIN_PERCENT` in the sketch (set per
platform) and `--gain` on the computer (default 1, so the sketch level is
played unchanged).

## License

MIT License; see `LICENSE`.
