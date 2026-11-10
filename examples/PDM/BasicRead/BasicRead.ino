#include <DevLab_AudioInput.h>

#if defined(ARDUINO_ARCH_RP2040)
#define PDM_CLK_PIN  10
#define PDM_DATA_PIN 11
#define SAMPLE_RATE  8000
#else
#define PDM_CLK_PIN  GPIO_NUM_2
#define PDM_DATA_PIN GPIO_NUM_3
#define SAMPLE_RATE  48000
#endif
// Software gain (50..800, 100 = unchanged). On RP the PDM output is about 4-6x
// lower above ~25 kHz, so 400 is used there (see README); other cases keep 100.
#if defined(ARDUINO_ARCH_RP2040)
#define GAIN_PERCENT ((SAMPLE_RATE) > 25000 ? 400 : 100)
#else
#define GAIN_PERCENT 100
#endif

DevLab_AudioInput pdm;
int16_t samples[512];

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!pdm.beginPDM(PDM_CLK_PIN, PDM_DATA_PIN, SAMPLE_RATE)) {
    Serial.println("PDM initialization failed.");
    while (true) delay(1000);
  }

  pdm.setGainPercent(GAIN_PERCENT);
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
