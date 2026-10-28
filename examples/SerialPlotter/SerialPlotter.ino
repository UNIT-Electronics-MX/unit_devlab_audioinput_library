// Prints raw 16-bit samples as text, one per line, for the Arduino Serial Plotter
// (Tools > Serial Plotter, 115200 baud). Samples are decimated so the serial
// link can keep up.
#include <DevLab_I2SPDM.h>

#define PDM_CLK_PIN  GPIO_NUM_22  // Qwiic SCL
#define PDM_DATA_PIN GPIO_NUM_21  // Qwiic SDA
#define BUFFER_LEN   512
#define DECIMATION   48           // 48000 Hz / 48 = 1000 values per second

DevLab_I2SPDM pdm;
int16_t audioBuffer[BUFFER_LEN];

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!pdm.begin(PDM_CLK_PIN, PDM_DATA_PIN, 48000)) {
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
