// Prints raw 16-bit samples as text, one per line, for the Arduino Serial Plotter
// (Tools > Serial Plotter, 115200 baud). Samples are decimated so the serial
// link can keep up.
#include <DevLab_AudioInput.h>

#if defined(ARDUINO_ARCH_RP2040)
#define PDM_CLK_PIN  10
#define PDM_DATA_PIN 11
#define SAMPLE_RATE  8000
#define DECIMATION   8
#else
#define PDM_CLK_PIN  GPIO_NUM_2
#define PDM_DATA_PIN GPIO_NUM_3
#define SAMPLE_RATE  48000
#define DECIMATION   48
#endif
#define BUFFER_LEN   512

DevLab_AudioInput pdm;
int16_t audioBuffer[BUFFER_LEN];

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!pdm.beginPDM(PDM_CLK_PIN, PDM_DATA_PIN, SAMPLE_RATE)) {
    Serial.println("ERROR: PDM init failed");
    while (1) delay(1000);
  }
}

void loop() {
  size_t n = pdm.read(audioBuffer, BUFFER_LEN);
  for (size_t i = 0; i < n; i += DECIMATION) {
    Serial.println(audioBuffer[i]);
  }
}
