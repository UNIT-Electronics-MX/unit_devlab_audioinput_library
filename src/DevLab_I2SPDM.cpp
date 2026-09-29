#include "DevLab_I2SPDM.h"

DevLab_I2SPDM::DevLab_I2SPDM() : _initialized(false) {}

bool DevLab_I2SPDM::begin() {
  // TODO: configure I2S / PDM peripheral
  _initialized = true;
  return true;
}

void DevLab_I2SPDM::end() {
  // TODO: release I2S / PDM peripheral
  _initialized = false;
}
