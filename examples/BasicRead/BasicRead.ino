#include <DevLab_I2SPDM.h>

DevLab_I2SPDM pdm;

void setup() {
  Serial.begin(115200);

  if (!pdm.begin()) {
    Serial.println("I2S/PDM initialization failed.");
    while (1);
  }
}

void loop() {
}
