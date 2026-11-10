#ifndef DEVLAB_AUDIO_INPUT_H
#define DEVLAB_AUDIO_INPUT_H

#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <driver/i2s_pdm.h>
#include <driver/i2s_std.h>
#elif defined(ARDUINO_ARCH_RP2040) && !defined(__MBED__)
#include <I2S.h>
#else
#error "DevLab_AudioInput supports ESP32 and Arduino-Pico RP2040/RP2350 cores"
#endif

// Receives a mono microphone signal and returns signed 16-bit PCM samples.
// I2S input expects 32-bit Philips-format samples (INMP441/ICS-43434 style).
class DevLab_AudioInput {
public:
  enum class Source : uint8_t { NONE, PDM, I2S };

  static const uint16_t DEFAULT_GAIN_PERCENT = 100;
  static const uint16_t MIN_GAIN_PERCENT = 50;
  static const uint16_t MAX_GAIN_PERCENT = 800;
  static const uint8_t DEFAULT_I2S_SHIFT = 10;

  DevLab_AudioInput();
  DevLab_AudioInput(const DevLab_AudioInput &) = delete;
  DevLab_AudioInput &operator=(const DevLab_AudioInput &) = delete;

#if defined(ARDUINO_ARCH_ESP32)
  // ESP32 PDM input requires a hardware PDM-to-PCM converter (ESP32-S3).
  bool beginPDM(gpio_num_t clkPin, gpio_num_t dataPin,
                uint32_t sampleRate = 48000,
                i2s_pdm_slot_mask_t slot = I2S_PDM_SLOT_LEFT);

  // The microphone L/R or SEL pin must match the chosen slot.
  bool beginI2S(gpio_num_t bclkPin, gpio_num_t wsPin, gpio_num_t dataPin,
                uint32_t sampleRate = 48000,
                i2s_std_slot_mask_t slot = I2S_STD_SLOT_RIGHT,
                uint8_t shift = DEFAULT_I2S_SHIFT);
#else
  // Arduino-Pico PDM uses the core's PIO converter. One PDM input at a time.
  // sampleRate up to 48000 (above ~25000 needs the patched core, see README).
  // rightChannel is false by default and leaves the core's sampling untouched.
  // Set it to true for a microphone with SELECT/LR high: the CLK pin output is
  // inverted so the core's fixed sampling edge falls in the right channel's
  // data window. Experimental on RP: not yet verified on hardware.
  bool beginPDM(int clkPin, int dataPin, uint32_t sampleRate = 8000,
                bool rightChannel = false);

  // Arduino-Pico requires WS/LRCLK = BCLK + 1. Select the mic's active side.
  bool beginI2S(int bclkPin, int wsPin, int dataPin,
                uint32_t sampleRate = 48000, bool rightChannel = true,
                uint8_t shift = 16);
#endif

  void end();
  Source source() const;

  // Returns the number of PCM16 mono samples placed in buffer (0 on no data).
  size_t read(int16_t *buffer, size_t samples, uint32_t timeoutMs = 10);

  void setGainPercent(uint16_t percent);
  uint16_t getGainPercent() const;
  void setI2SShift(uint8_t shift);
  uint8_t getI2SShift() const;

private:
  Source _source;
#if defined(ARDUINO_ARCH_ESP32)
  i2s_chan_handle_t _rxHandle;
#else
  I2S _rpI2S;
  bool _rightChannel;
#endif
  uint16_t _gainPercent;
  uint8_t _i2sShift;

  size_t readPDM(int16_t *buffer, size_t samples, uint32_t timeoutMs);
  size_t readI2S(int16_t *buffer, size_t samples, uint32_t timeoutMs);
  void endPDM();
  void endI2S();
  void applyGain(int16_t *buffer, size_t samples);
};

#endif  // DEVLAB_AUDIO_INPUT_H
