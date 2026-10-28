// Streams raw 16-bit mono PCM over Serial so the computer can play it.
// Run pc_audio.py on the computer (see the header of that file).
// SAMPLE_RATE and BAUD_RATE must match the values in pc_audio.py.
#include <DevLab_I2SPDM.h>

#define PDM_CLK_PIN  GPIO_NUM_22  // Qwiic SCL
#define PDM_DATA_PIN GPIO_NUM_21  // Qwiic SDA
#define SAMPLE_RATE  16000        // 16000 * 2 bytes = 32 KB/s
#define BAUD_RATE    921600       // ~92 KB/s available
#define BUFFER_LEN   512
#define GAIN_PERCENT 800          // 50..800, software gain (100 = unchanged)

DevLab_I2SPDM pdm;
int16_t audioBuffer[BUFFER_LEN];

void setup() {
  Serial.begin(BAUD_RATE);
  delay(1000);

  if (!pdm.begin(PDM_CLK_PIN, PDM_DATA_PIN, SAMPLE_RATE)) {
    Serial.println("ERROR: PDM init failed");
    while (1) delay(1000);
  }

  pdm.setGain(GAIN_PERCENT);

  Serial.println("RAW");  // pc_audio.py starts reading audio after this line
  Serial.flush();
}

void loop() {
  size_t n = pdm.read(audioBuffer, BUFFER_LEN);
  if (n > 0) Serial.write((uint8_t *)audioBuffer, n * sizeof(int16_t));
}
