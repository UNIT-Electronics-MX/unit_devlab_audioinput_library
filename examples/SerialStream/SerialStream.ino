// Streams raw 16-bit PCM over Serial. Commands: 'R' -> send "RAW" header,
// 'G<percent>' -> set gain (e.g. G200 = 200%).
#include <DevLab_I2SPDM.h>

#define PDM_CLK_PIN  GPIO_NUM_22  // Qwiic SCL
#define PDM_DATA_PIN GPIO_NUM_21  // Qwiic SDA
#define BUFFER_LEN   512

DevLab_I2SPDM pdm;
int16_t audioBuffer[BUFFER_LEN];

uint16_t pendingGain = 0;
bool parsingGain = false;
bool gainHasDigit = false;

void sendRawHeader() {
  Serial.println("RAW");
  Serial.flush();
  delay(100);
}

void commitGain() {
  if (gainHasDigit) pdm.setGain(pendingGain);
  pendingGain = 0;
  parsingGain = false;
  gainHasDigit = false;
}

void processCommand(char c) {
  if (c == 'R') {
    sendRawHeader();
  } else if (c == 'G') {
    parsingGain = true;
    pendingGain = 0;
    gainHasDigit = false;
  }
}

void handleSerialCommands() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
      if (parsingGain) commitGain();
      continue;
    }

    if (parsingGain) {
      if (c >= '0' && c <= '9') {
        gainHasDigit = true;
        pendingGain = min<uint32_t>(pendingGain * 10 + (c - '0'), DevLab_I2SPDM::MAX_GAIN_PERCENT);
      } else {
        commitGain();
        processCommand(c);
      }
    } else {
      processCommand(c);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!pdm.begin(PDM_CLK_PIN, PDM_DATA_PIN, 48000)) {
    Serial.println("ERROR: PDM init failed");
    while (1) delay(1000);
  }
  sendRawHeader();
}

void loop() {
  handleSerialCommands();

  size_t n = pdm.read(audioBuffer, BUFFER_LEN);
  if (n > 0) Serial.write((uint8_t *)audioBuffer, n * sizeof(int16_t));
}
