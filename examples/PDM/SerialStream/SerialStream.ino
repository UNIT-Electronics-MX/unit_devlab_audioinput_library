// Streams raw 16-bit PCM over Serial. 'R' starts/resyncs, 'S' stops,
// and 'G<percent>\n' sets gain (e.g. G200 = 200%).
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
#define BUFFER_LEN   512
#define BAUD_RATE    2000000
// Software gain (50..800, 100 = unchanged). On RP the PDM output is about 4-6x
// lower above ~25 kHz, so 400 is used there (see README); other cases keep 100.
#if defined(ARDUINO_ARCH_RP2040)
#define GAIN_PERCENT ((SAMPLE_RATE) > 25000 ? 400 : 100)
#else
#define GAIN_PERCENT 100
#endif

DevLab_AudioInput pdm;
int16_t audioBuffer[BUFFER_LEN];

uint16_t pendingGain = 0;
bool parsingGain = false;
bool gainHasDigit = false;
bool streamEnabled = false;

void sendRawHeader() {
  Serial.println("RAW");
  Serial.flush();
  delay(100);
  streamEnabled = true;
}

void commitGain() {
  if (gainHasDigit) pdm.setGainPercent(pendingGain);
  pendingGain = 0;
  parsingGain = false;
  gainHasDigit = false;
}

void processCommand(char c) {
  if (c == 'R') {
    sendRawHeader();
  } else if (c == 'S') {
    streamEnabled = false;
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
        pendingGain = min<uint32_t>(pendingGain * 10 + (c - '0'), DevLab_AudioInput::MAX_GAIN_PERCENT);
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
  Serial.begin(BAUD_RATE);
  delay(1000);

  if (!pdm.beginPDM(PDM_CLK_PIN, PDM_DATA_PIN, SAMPLE_RATE)) {
    Serial.println("ERROR: PDM init failed");
    while (1) delay(1000);
  }
  pdm.setGainPercent(GAIN_PERCENT);  // 'G<percent>' changes it at runtime
  sendRawHeader();
}

void loop() {
  handleSerialCommands();

  if (!streamEnabled) {
    delay(1);
    return;
  }
  size_t n = pdm.read(audioBuffer, BUFFER_LEN);
  if (n > 0) Serial.write((uint8_t *)audioBuffer, n * sizeof(int16_t));
}
