#include "DevLab_AudioInput.h"

#if defined(ARDUINO_ARCH_ESP32)
bool DevLab_AudioInput::beginI2S(gpio_num_t bclkPin, gpio_num_t wsPin,
                                 gpio_num_t dataPin, uint32_t sampleRate,
                                 i2s_std_slot_mask_t slot, uint8_t shift) {
  if (_source != Source::NONE) return false;

  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  if (i2s_new_channel(&chanCfg, nullptr, &_rxHandle) != ESP_OK) {
    _rxHandle = nullptr;
    return false;
  }

  i2s_std_config_t stdCfg = {
      .clk_cfg = {
          .sample_rate_hz = sampleRate,
          .clk_src = I2S_CLK_SRC_DEFAULT,
          .mclk_multiple = I2S_MCLK_MULTIPLE_256,
          .bclk_div = 8,
      },
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
          I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
      .gpio_cfg = {
          .mclk = I2S_GPIO_UNUSED,
          .bclk = bclkPin,
          .ws = wsPin,
          .dout = I2S_GPIO_UNUSED,
          .din = dataPin,
          .invert_flags = {
              .mclk_inv = false,
              .bclk_inv = false,
              .ws_inv = false,
          },
      },
  };
  stdCfg.slot_cfg.slot_mask = slot;

  if (i2s_channel_init_std_mode(_rxHandle, &stdCfg) != ESP_OK ||
      i2s_channel_enable(_rxHandle) != ESP_OK) {
    i2s_del_channel(_rxHandle);
    _rxHandle = nullptr;
    return false;
  }

  setI2SShift(shift);
  _source = Source::I2S;
  return true;
}
#else
bool DevLab_AudioInput::beginI2S(int bclkPin, int wsPin, int dataPin,
                                 uint32_t sampleRate, bool rightChannel,
                                 uint8_t shift) {
  if (_source != Source::NONE || bclkPin < 0 ||
      wsPin != bclkPin + 1 || dataPin < 0 ||
      dataPin == bclkPin || dataPin == wsPin || sampleRate == 0) return false;

  if (!_rpI2S.setBCLK(bclkPin) || !_rpI2S.setDATA(dataPin) ||
      !_rpI2S.setBitsPerSample(32) ||
      !_rpI2S.setFrequency(sampleRate) || !_rpI2S.begin()) return false;

  _rightChannel = rightChannel;
  setI2SShift(shift);
  _source = Source::I2S;
  return true;
}
#endif

void DevLab_AudioInput::endI2S() {
#if defined(ARDUINO_ARCH_ESP32)
  if (_rxHandle != nullptr) {
    i2s_channel_disable(_rxHandle);
    i2s_del_channel(_rxHandle);
    _rxHandle = nullptr;
  }
#else
  _rpI2S.end();
#endif
}

size_t DevLab_AudioInput::readI2S(int16_t *buffer, size_t samples,
                                  uint32_t timeoutMs) {
#if defined(ARDUINO_ARCH_ESP32)
  if (_rxHandle == nullptr) return 0;
  int32_t raw[128];
  size_t total = 0;
  while (total < samples) {
    size_t chunk = samples - total;
    if (chunk > 128) chunk = 128;

    size_t bytesRead = 0;
    esp_err_t err = i2s_channel_read(_rxHandle, raw,
                                     chunk * sizeof(int32_t), &bytesRead,
                                     pdMS_TO_TICKS(timeoutMs));
    if (err != ESP_OK && !(err == ESP_ERR_TIMEOUT && bytesRead > 0)) break;

    size_t received = bytesRead / sizeof(int32_t);
    for (size_t i = 0; i < received; ++i) {
      int32_t pcm = raw[i] >> _i2sShift;
      if (pcm > INT16_MAX) pcm = INT16_MAX;
      if (pcm < INT16_MIN) pcm = INT16_MIN;
      buffer[total + i] = static_cast<int16_t>(pcm);
    }
    total += received;
    if (received < chunk) break;
  }
  return total;
#else
  const uint32_t started = millis();
  size_t total = 0;
  while (total < samples) {
    if (_rpI2S.available() < 2 * static_cast<int>(sizeof(int32_t))) {
      if (total > 0 || millis() - started >= timeoutMs) break;
      delay(1);
      continue;
    }

    int32_t left = 0;
    int32_t right = 0;
    if (!_rpI2S.read32(&left, &right)) break;
    int32_t pcm = (_rightChannel ? right : left) >> _i2sShift;
    if (pcm > INT16_MAX) pcm = INT16_MAX;
    if (pcm < INT16_MIN) pcm = INT16_MIN;
    buffer[total++] = static_cast<int16_t>(pcm);
  }
  return total;
#endif
}
