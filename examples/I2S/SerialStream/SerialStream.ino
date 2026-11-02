// Stream raw signed PCM16 mono at 48 kHz over Serial at 2 Mbaud.
// 'R' starts/resyncs, 'S' stops, 'G<shift>\n' sets the I2S conversion shift.
// Close Serial Monitor before connecting mic_gui.py to the same port.
#include <DevLab_AudioInput.h>

#if defined(ARDUINO_ARCH_RP2040)
#define I2S_BCLK_PIN 0
#define I2S_WS_PIN   1
#define I2S_DATA_PIN 2
#define I2S_SEL_PIN  3
#define MAX_SHIFT    24
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
#define I2S_BCLK_PIN GPIO_NUM_6
#define I2S_WS_PIN   GPIO_NUM_7
#define I2S_DATA_PIN GPIO_NUM_5
#define I2S_SEL_PIN  GPIO_NUM_4
#define MAX_SHIFT    14
#else
#define I2S_BCLK_PIN GPIO_NUM_36
#define I2S_WS_PIN   GPIO_NUM_37
#define I2S_DATA_PIN GPIO_NUM_35
#define I2S_SEL_PIN  GPIO_NUM_34
#define MAX_SHIFT    14
#endif
#define SAMPLE_RATE  48000
#define BAUD_RATE    2000000
#define BUFFER_LEN   512

DevLab_AudioInput mic;
int16_t audioBuffer[BUFFER_LEN];
uint8_t pendingShift = 0;
bool parsingShift = false;
bool hasDigit = false;
bool streamEnabled = false;

void sendRawHeader() {
  Serial.println("RAW");
  Serial.flush();
  delay(20);
  streamEnabled = true;
}

void commitShift() {
  if (hasDigit) mic.setI2SShift(constrain(pendingShift, 8, MAX_SHIFT));
  pendingShift = 0;
  parsingShift = false;
  hasDigit = false;
}

void processCommand(char command) {
  if (command == 'R') {
    sendRawHeader();
  } else if (command == 'S') {
    streamEnabled = false;
  } else if (command == 'G') {
    parsingShift = true;
    pendingShift = 0;
    hasDigit = false;
  }
}

void handleCommands() {
  while (Serial.available() > 0) {
    char command = Serial.read();
    if (command == '\r' || command == '\n' || command == ' ' || command == '\t') {
      if (parsingShift) commitShift();
      continue;
    }
    if (parsingShift) {
      if (command >= '0' && command <= '9') {
        hasDigit = true;
        uint16_t value = pendingShift * 10 + (command - '0');
        pendingShift = (value > MAX_SHIFT) ? MAX_SHIFT : value;
      } else {
        commitShift();
        processCommand(command);
      }
    } else {
      processCommand(command);
    }
  }
}

void setup() {
  Serial.begin(BAUD_RATE);
  delay(1000);
  pinMode(I2S_SEL_PIN, OUTPUT);
  digitalWrite(I2S_SEL_PIN, HIGH);  // RIGHT slot

#if defined(ARDUINO_ARCH_RP2040)
  bool started = mic.beginI2S(I2S_BCLK_PIN, I2S_WS_PIN, I2S_DATA_PIN,
                              SAMPLE_RATE, true, 16);
#else
  bool started = mic.beginI2S(I2S_BCLK_PIN, I2S_WS_PIN, I2S_DATA_PIN,
                              SAMPLE_RATE, I2S_STD_SLOT_RIGHT);
#endif
  if (!started) {
    Serial.println("ERROR: I2S initialization failed");
    while (true) delay(1000);
  }
  sendRawHeader();
}

void loop() {
  handleCommands();
  if (!streamEnabled) {
    delay(1);
    return;
  }
  size_t count = mic.read(audioBuffer, BUFFER_LEN);
  if (count > 0) {
    Serial.write(reinterpret_cast<uint8_t *>(audioBuffer),
                 count * sizeof(int16_t));
  }
  handleCommands();
}
