#ifndef DEVLAB_I2SPDM_H
#define DEVLAB_I2SPDM_H

#include <Arduino.h>
#include <driver/i2s_pdm.h>

class DevLab_I2SPDM {
public:
  static const uint16_t DEFAULT_GAIN_PERCENT = 100;
  static const uint16_t MIN_GAIN_PERCENT = 50;
  static const uint16_t MAX_GAIN_PERCENT = 800;

  DevLab_I2SPDM();

  /// @brief Configure and start the PDM microphone.
  /// @param clkPin PDM clock pin.
  /// @param dataPin PDM data pin.
  /// @param sampleRate Sample rate in Hz.
  /// @param slot I2S_PDM_SLOT_LEFT or I2S_PDM_SLOT_RIGHT (depends on the mic SEL pin).
  /// @return true on success.
  bool begin(gpio_num_t clkPin, gpio_num_t dataPin,
             uint32_t sampleRate = 48000,
             i2s_pdm_slot_mask_t slot = I2S_PDM_SLOT_LEFT);

  /// @brief Stop the microphone and release the I2S channel.
  void end();

  /// @brief Read audio samples (16-bit mono), with the gain already applied.
  /// @return Number of samples read (0 on timeout or error).
  size_t read(int16_t *buffer, size_t samples, uint32_t timeoutMs = 10);

  /// @brief Set the software gain, clamped to MIN/MAX_GAIN_PERCENT.
  void setGain(uint16_t percent);
  uint16_t getGain() const;

private:
  bool _initialized;
  i2s_chan_handle_t _rxHandle;
  uint16_t _gainPercent;

  void applyGain(int16_t *buffer, size_t samples);
};

#endif  // DEVLAB_I2SPDM_H
