#include "DevLab_I2SPDM.h"

DevLab_I2SPDM::DevLab_I2SPDM()
    : _initialized(false),
      _rxHandle(nullptr),
      _gainPercent(DEFAULT_GAIN_PERCENT) {}

bool DevLab_I2SPDM::begin(gpio_num_t clkPin, gpio_num_t dataPin,
                          uint32_t sampleRate, i2s_pdm_slot_mask_t slot) {
  if (_initialized) return true;

  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  if (i2s_new_channel(&chanCfg, NULL, &_rxHandle) != ESP_OK) {
    _rxHandle = nullptr;
    return false;
  }

  i2s_pdm_rx_config_t pdmCfg = {
      .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(sampleRate),
      .slot_cfg = {
          .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
          .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
          .slot_mode = I2S_SLOT_MODE_MONO,
          .slot_mask = slot,
      },
      .gpio_cfg = {
          .clk = clkPin,
          .din = dataPin,
          .invert_flags = {
              .clk_inv = false,
          },
      },
  };

  if (i2s_channel_init_pdm_rx_mode(_rxHandle, &pdmCfg) != ESP_OK ||
      i2s_channel_enable(_rxHandle) != ESP_OK) {
    i2s_del_channel(_rxHandle);
    _rxHandle = nullptr;
    return false;
  }

  _initialized = true;
  return true;
}

void DevLab_I2SPDM::end() {
  if (!_initialized) return;
  i2s_channel_disable(_rxHandle);
  i2s_del_channel(_rxHandle);
  _rxHandle = nullptr;
  _initialized = false;
}

size_t DevLab_I2SPDM::read(int16_t *buffer, size_t samples, uint32_t timeoutMs) {
  if (!_initialized) return 0;

  size_t bytesRead = 0;
  esp_err_t err = i2s_channel_read(_rxHandle, buffer, samples * sizeof(int16_t),
                                   &bytesRead, pdMS_TO_TICKS(timeoutMs));
  // On timeout the driver has already consumed the bytes it returned: keep them.
  if (err != ESP_OK && !(err == ESP_ERR_TIMEOUT && bytesRead > 0)) return 0;

  size_t samplesRead = bytesRead / sizeof(int16_t);
  applyGain(buffer, samplesRead);
  return samplesRead;
}

void DevLab_I2SPDM::setGain(uint16_t percent) {
  _gainPercent = constrain(percent, MIN_GAIN_PERCENT, MAX_GAIN_PERCENT);
}

uint16_t DevLab_I2SPDM::getGain() const {
  return _gainPercent;
}

void DevLab_I2SPDM::applyGain(int16_t *buffer, size_t samples) {
  if (_gainPercent == 100) return;

  for (size_t i = 0; i < samples; i++) {
    int32_t sample = ((int32_t)buffer[i] * _gainPercent) / 100;

    if (sample > INT16_MAX) {
      sample = INT16_MAX;
    } else if (sample < INT16_MIN) {
      sample = INT16_MIN;
    }

    buffer[i] = (int16_t)sample;
  }
}
