#include <DevLab_AudioInput.h>

#if defined(ARDUINO_ARCH_RP2040)
#define PDM_CLK_PIN  10
#define PDM_DATA_PIN 11
#else
#define PDM_CLK_PIN  GPIO_NUM_2
#define PDM_DATA_PIN GPIO_NUM_3
#endif

DevLab_AudioInput pdm;
int16_t samples[512];

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!pdm.beginPDM(PDM_CLK_PIN, PDM_DATA_PIN)) {
    Serial.println("PDM initialization failed.");
    while (true) delay(1000);
  }
}

void loop() {
  size_t n = pdm.read(samples, 512);
  if (n == 0) return;

  // Peak level of the block
  int32_t peak = 0;
  for (size_t i = 0; i < n; i++) {
    int32_t v = abs(static_cast<int32_t>(samples[i]));
    if (v > peak) peak = v;
  }
  Serial.println(peak);
}
