#include "DevLab_AudioInput.h"

#if defined(ARDUINO_ARCH_RP2040) && !defined(__MBED__)
#include <PDM.h>

namespace {
DevLab_AudioInput *pdmOwner = nullptr;
}
#endif

#if defined(ARDUINO_ARCH_ESP32)
bool DevLab_AudioInput::beginPDM(gpio_num_t clkPin, gpio_num_t dataPin,
                                 uint32_t sampleRate, i2s_pdm_slot_mask_t slot) {
  if (_source != Source::NONE) return false;

#if !SOC_I2S_SUPPORTS_PDM2PCM
  // The read() API returns PCM, which raw-PDM-only chips cannot provide.
  return false;
#else
  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  if (i2s_new_channel(&chanCfg, nullptr, &_rxHandle) != ESP_OK) {
    _rxHandle = nullptr;
    return false;
  }

  i2s_pdm_rx_config_t pdmCfg = {
      .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(sampleRate),
      .slot_cfg = I2S_PDM_RX_SLOT_PCM_FMT_DEFAULT_CONFIG(
          I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
      .gpio_cfg = {
          .clk = clkPin,
          .din = dataPin,
          .invert_flags = {.clk_inv = false},
      },
  };
  pdmCfg.slot_cfg.slot_mask = slot;

  if (i2s_channel_init_pdm_rx_mode(_rxHandle, &pdmCfg) != ESP_OK ||
      i2s_channel_enable(_rxHandle) != ESP_OK) {
    i2s_del_channel(_rxHandle);
    _rxHandle = nullptr;
    return false;
  }

  _source = Source::PDM;
  return true;
#endif
}
#else
bool DevLab_AudioInput::beginPDM(int clkPin, int dataPin,
                                 uint32_t sampleRate) {
  if (_source != Source::NONE || pdmOwner != nullptr ||
      clkPin < 0 || dataPin < 0 || clkPin == dataPin ||
      sampleRate == 0 || sampleRate > 25000) return false;

  PDM.setCLK(clkPin);
  PDM.setDIN(dataPin);
  PDM.setBufferSize(1024);
  if (PDM.begin(1, sampleRate) != 1) return false;

  pdmOwner = this;
  _source = Source::PDM;
  return true;
}
#endif

void DevLab_AudioInput::endPDM() {
#if defined(ARDUINO_ARCH_ESP32)
  if (_rxHandle != nullptr) {
    i2s_channel_disable(_rxHandle);
    i2s_del_channel(_rxHandle);
    _rxHandle = nullptr;
  }
#else
  PDM.end();
  pdmOwner = nullptr;
#endif
}

size_t DevLab_AudioInput::readPDM(int16_t *buffer, size_t samples,
                                  uint32_t timeoutMs) {
#if defined(ARDUINO_ARCH_ESP32)
  if (_rxHandle == nullptr) return 0;
  size_t bytesRead = 0;
  esp_err_t err = i2s_channel_read(_rxHandle, buffer,
                                   samples * sizeof(int16_t), &bytesRead,
                                   pdMS_TO_TICKS(timeoutMs));
  if (err != ESP_OK && !(err == ESP_ERR_TIMEOUT && bytesRead > 0)) return 0;
  return bytesRead / sizeof(int16_t);
#else
  const uint32_t started = millis();
  while (true) {
    int availableBytes = PDM.available();
    if (availableBytes >= static_cast<int>(sizeof(int16_t))) {
      size_t count = static_cast<size_t>(availableBytes) / sizeof(int16_t);
      if (count > samples) count = samples;
      int bytesRead = PDM.read(buffer, count * sizeof(int16_t));
      return bytesRead > 0 ? bytesRead / sizeof(int16_t) : 0;
    }
    if (millis() - started >= timeoutMs) return 0;
    delay(1);
  }
#endif
}
