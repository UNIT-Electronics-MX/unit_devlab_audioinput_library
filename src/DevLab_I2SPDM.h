#ifndef DEVLAB_I2SPDM_H
#define DEVLAB_I2SPDM_H

#include <Arduino.h>

class DevLab_I2SPDM {
public:
  DevLab_I2SPDM();

  bool begin();
  void end();

private:
  bool _initialized;
};

#endif  // DEVLAB_I2SPDM_H
