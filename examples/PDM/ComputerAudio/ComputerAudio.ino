// Streams raw 16-bit mono PCM over Serial so the computer can play it.
// Run pc_audio.py on the computer (see the header of that file).
// SAMPLE_RATE and BAUD_RATE must match the values in pc_audio.py.
#include <DevLab_AudioInput.h>

#if defined(ARDUINO_ARCH_RP2040)
#define PDM_CLK_PIN  10
#define PDM_DATA_PIN 11
#else
#define PDM_CLK_PIN  GPIO_NUM_2
#define PDM_DATA_PIN GPIO_NUM_3
#endif
#define SAMPLE_RATE  16000        // 16000 * 2 bytes = 32 KB/s
#define BAUD_RATE    921600       // ~92 KB/s available
#define BUFFER_LEN   512
#define GAIN_PERCENT 800          // 50..800, software gain (100 = unchanged)

DevLab_AudioInput pdm;
int16_t audioBuffer[BUFFER_LEN];
bool streamEnabled = false;

void setup() {
  Serial.begin(BAUD_RATE);
  delay(1000);

  if (!pdm.beginPDM(PDM_CLK_PIN, PDM_DATA_PIN, SAMPLE_RATE)) {
    Serial.println("ERROR: PDM init failed");
    while (1) delay(1000);
  }

  pdm.setGainPercent(GAIN_PERCENT);

}

void loop() {
  while (Serial.available() > 0) {
    char command = Serial.read();
    if (command == 'R') {
      Serial.println("RAW");
      Serial.flush();
      streamEnabled = true;
    } else if (command == 'S') {
      streamEnabled = false;
    }
  }
  if (!streamEnabled) {
    delay(1);
    return;
  }
  size_t n = pdm.read(audioBuffer, BUFFER_LEN);
  if (n > 0) Serial.write((uint8_t *)audioBuffer, n * sizeof(int16_t));
}
