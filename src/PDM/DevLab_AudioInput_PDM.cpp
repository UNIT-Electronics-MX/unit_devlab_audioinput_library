#include "DevLab_AudioInput.h"

#if defined(ARDUINO_ARCH_RP2040) && !defined(__MBED__)
#include <PDM.h>
#include "hardware/gpio.h"

namespace {
DevLab_AudioInput *pdmOwner = nullptr;
int pdmClkPin = -1;

// The core hands over one block of only 32-64 samples (1.3 ms at 48 kHz) and
// drops the next block if the previous one was not read yet. A DMA interrupt
// callback moves each block into this ring so read() callers (for example a
// sketch blocked on USB) do not have to keep up. One writer (interrupt), one
// reader (read()); head and tail only ever increase.
constexpr uint32_t PDM_RING_SAMPLES = 8192;  // power of two, ~170 ms at 48 kHz
int16_t pdmRing[PDM_RING_SAMPLES];
volatile uint32_t pdmHead = 0;
volatile uint32_t pdmTail = 0;

void pdmDrainBlock() {
  int16_t block[128];
  int bytes = PDM.available();
  if (bytes < static_cast<int>(sizeof(int16_t))) return;
  if (bytes > static_cast<int>(sizeof(block))) bytes = sizeof(block);
  bytes = PDM.read(block, bytes);
  uint32_t count = bytes > 0 ? bytes / sizeof(int16_t) : 0;
  uint32_t head = pdmHead;
  if (PDM_RING_SAMPLES - (head - pdmTail) < count) return;  // full: drop block
  for (uint32_t i = 0; i < count; ++i) {
    pdmRing[(head + i) & (PDM_RING_SAMPLES - 1)] = block[i];
  }
  pdmHead = head + count;
}
}
#endif

#if defined(ARDUINO_ARCH_ESP32)
// The read() API returns PCM, so the chip needs the PDM-to-PCM converter.
// ESP-IDF 5.4+ exposes this as SOC_I2S_SUPPORTS_PDM2PCM and a *_PCM_FMT_*
// slot macro. Older IDF (e.g. 5.3) has no such macro: PDM RX there always goes
// through the converter, and I2S_PDM_RX_SLOT_DEFAULT_CONFIG is the PCM config.
#if SOC_I2S_SUPPORTS_PDM2PCM
#define DEVLAB_PDM_RX_SLOT_CONFIG I2S_PDM_RX_SLOT_PCM_FMT_DEFAULT_CONFIG
#elif SOC_I2S_SUPPORTS_PDM_RX && ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 4, 0)
#define DEVLAB_PDM_RX_SLOT_CONFIG I2S_PDM_RX_SLOT_DEFAULT_CONFIG
#endif

bool DevLab_AudioInput::beginPDM(gpio_num_t clkPin, gpio_num_t dataPin,
                                 uint32_t sampleRate, i2s_pdm_slot_mask_t slot) {
  if (_source != Source::NONE) return false;

#ifndef DEVLAB_PDM_RX_SLOT_CONFIG
  // Raw-PDM-only chips cannot provide PCM.
  return false;
#else
  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  if (i2s_new_channel(&chanCfg, nullptr, &_rxHandle) != ESP_OK) {
    _rxHandle = nullptr;
    return false;
  }

  i2s_pdm_rx_config_t pdmCfg = {
      .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(sampleRate),
      .slot_cfg = DEVLAB_PDM_RX_SLOT_CONFIG(
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
                                 uint32_t sampleRate, bool rightChannel) {
  // Up to 48 kHz. Above ~25 kHz the core halves its decimation, so the PDM
  // clock stays at 3.07 MHz (inside typical mic specs) but the output level
  // drops about 4-6x: raise setGainPercent() to compensate (see README).
  if (_source != Source::NONE || pdmOwner != nullptr ||
      clkPin < 0 || dataPin < 0 || clkPin == dataPin ||
      sampleRate == 0 || sampleRate > 48000) return false;

  PDM.setCLK(clkPin);
  PDM.setDIN(dataPin);
  PDM.setBufferSize(1024);
  pdmHead = 0;
  pdmTail = 0;
  PDM.onReceive(pdmDrainBlock);
  if (PDM.begin(1, sampleRate) != 1) {
    PDM.onReceive(nullptr);
    return false;
  }

  // PDM.begin() routes CLK to the PIO, which clears any pad override, so the
  // inversion is applied afterwards.
  if (rightChannel) gpio_set_outover(clkPin, GPIO_OVERRIDE_INVERT);

  pdmClkPin = clkPin;
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
  PDM.onReceive(nullptr);
  if (pdmClkPin >= 0) gpio_set_outover(pdmClkPin, GPIO_OVERRIDE_NORMAL);
  pdmClkPin = -1;
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
    uint32_t tail = pdmTail;
    uint32_t count = pdmHead - tail;
    if (count > 0) {
      if (count > samples) count = samples;
      for (uint32_t i = 0; i < count; ++i) {
        buffer[i] = pdmRing[(tail + i) & (PDM_RING_SAMPLES - 1)];
      }
      pdmTail = tail + count;
      return count;
    }
    if (millis() - started >= timeoutMs) return 0;
    delay(1);
  }
#endif
}
