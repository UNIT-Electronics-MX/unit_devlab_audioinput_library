#include "DevLab_AudioInput.h"

DevLab_AudioInput::DevLab_AudioInput()
    : _source(Source::NONE),
#if defined(ARDUINO_ARCH_ESP32)
      _rxHandle(nullptr),
#else
      _rpI2S(INPUT),
      _rightChannel(true),
#endif
      _gainPercent(DEFAULT_GAIN_PERCENT),
      _i2sShift(DEFAULT_I2S_SHIFT) {}

void DevLab_AudioInput::end() {
  if (_source == Source::PDM) {
    endPDM();
  } else if (_source == Source::I2S) {
    endI2S();
  }
  _source = Source::NONE;
}

DevLab_AudioInput::Source DevLab_AudioInput::source() const {
  return _source;
}

size_t DevLab_AudioInput::read(int16_t *buffer, size_t samples,
                               uint32_t timeoutMs) {
  if (buffer == nullptr || samples == 0) return 0;

  size_t count = 0;
  if (_source == Source::PDM) {
    count = readPDM(buffer, samples, timeoutMs);
  } else if (_source == Source::I2S) {
    count = readI2S(buffer, samples, timeoutMs);
  }
  applyGain(buffer, count);
  return count;
}

void DevLab_AudioInput::setGainPercent(uint16_t percent) {
  _gainPercent = constrain(percent, MIN_GAIN_PERCENT, MAX_GAIN_PERCENT);
}

uint16_t DevLab_AudioInput::getGainPercent() const {
  return _gainPercent;
}

void DevLab_AudioInput::setI2SShift(uint8_t shift) {
  _i2sShift = (shift > 24) ? 24 : shift;
}

uint8_t DevLab_AudioInput::getI2SShift() const {
  return _i2sShift;
}

void DevLab_AudioInput::applyGain(int16_t *buffer, size_t samples) {
  if (_gainPercent == 100) return;

  for (size_t i = 0; i < samples; ++i) {
    int32_t value = (static_cast<int32_t>(buffer[i]) * _gainPercent) / 100;
    if (value > INT16_MAX) value = INT16_MAX;
    if (value < INT16_MIN) value = INT16_MIN;
    buffer[i] = static_cast<int16_t>(value);
  }
}
