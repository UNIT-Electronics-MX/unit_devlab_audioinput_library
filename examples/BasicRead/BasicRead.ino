#include <DevLab_I2SPDM.h>

#define PDM_CLK_PIN  GPIO_NUM_2
#define PDM_DATA_PIN GPIO_NUM_3

DevLab_I2SPDM pdm;
int16_t samples[512];

void setup() {
  Serial.begin(115200);

  if (!pdm.begin(PDM_CLK_PIN, PDM_DATA_PIN)) {
    Serial.println("I2S/PDM initialization failed.");
    while (1);
  }
}

void loop() {
  size_t n = pdm.read(samples, 512);
  if (n == 0) return;

  // Peak level of the block
  int16_t peak = 0;
  for (size_t i = 0; i < n; i++) {
    int16_t v = abs(samples[i]);
    if (v > peak) peak = v;
  }
  Serial.println(peak);
}
