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
#define SAMPLE_RATE  8000
// Software gain (50..800, 100 = unchanged). On RP the PDM output is about 4-6x
// lower above ~25 kHz, so 400 is used there (see README); other cases keep 100.
#if defined(ARDUINO_ARCH_RP2040)
#define GAIN_PERCENT ((SAMPLE_RATE) > 25000 ? 400 : 100)
#else
#define GAIN_PERCENT 100
#endif

DevLab_AudioInput mic;
int16_t samples[256];

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!mic.beginPDM(PDM_CLK_PIN, PDM_DATA_PIN, SAMPLE_RATE)) {
    Serial.println("ERROR: PDM initialization failed");
    while (true) delay(1000);
  }

  mic.setGainPercent(GAIN_PERCENT);
}

void loop() {
  size_t count = mic.read(samples, 256);
  for (size_t i = 0; i < count; ++i) {
    Serial.println(samples[i]);
  }
}
