// Prints PCM16 samples from a PDM microphone, one sample per line.
// Useful for checking the waveform in Serial Monitor or Serial Plotter.
// Text output at 115200 baud can lose real-time samples; for audio capture
// use PDM/SerialStream or PDM/ComputerAudio instead.
#include <DevLab_AudioInput.h>

#if defined(ARDUINO_ARCH_RP2040)
#define PDM_CLK_PIN  10
#define PDM_DATA_PIN 11
#else
#define PDM_CLK_PIN  GPIO_NUM_2
#define PDM_DATA_PIN GPIO_NUM_3
#endif

DevLab_AudioInput mic;
int16_t samples[256];

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!mic.beginPDM(PDM_CLK_PIN, PDM_DATA_PIN, 8000)) {
    Serial.println("ERROR: PDM initialization failed");
    while (true) delay(1000);
  }
}

void loop() {
  size_t count = mic.read(samples, 256);
  for (size_t i = 0; i < count; ++i) {
    Serial.println(samples[i]);
  }
}
