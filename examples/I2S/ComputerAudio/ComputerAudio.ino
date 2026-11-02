// Streams I2S microphone audio as mono PCM16 over Serial for computer playback.
// Run pc_audio.py on the computer; keep SAMPLE_RATE and BAUD_RATE in sync.
#include <DevLab_AudioInput.h>

#if defined(ARDUINO_ARCH_RP2040)
#define I2S_BCLK_PIN 0
#define I2S_WS_PIN   1
#define I2S_DATA_PIN 2
#define I2S_SEL_PIN  3
#define I2S_SHIFT    16
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
#define I2S_BCLK_PIN GPIO_NUM_6
#define I2S_WS_PIN   GPIO_NUM_7
#define I2S_DATA_PIN GPIO_NUM_5
#define I2S_SEL_PIN  GPIO_NUM_4
#define I2S_SHIFT    10
#else
#define I2S_BCLK_PIN GPIO_NUM_36
#define I2S_WS_PIN   GPIO_NUM_37
#define I2S_DATA_PIN GPIO_NUM_35
#define I2S_SEL_PIN  GPIO_NUM_34
#define I2S_SHIFT    10
#endif

#define SAMPLE_RATE 16000
#define BAUD_RATE   921600
#define BUFFER_LEN  512

DevLab_AudioInput mic;
int16_t audioBuffer[BUFFER_LEN];
bool streamEnabled = false;

void setup() {
  Serial.begin(BAUD_RATE);
  delay(1000);

  pinMode(I2S_SEL_PIN, OUTPUT);
  digitalWrite(I2S_SEL_PIN, HIGH);  // Select the RIGHT microphone channel.

#if defined(ARDUINO_ARCH_RP2040)
  bool started = mic.beginI2S(I2S_BCLK_PIN, I2S_WS_PIN, I2S_DATA_PIN,
                              SAMPLE_RATE, true, I2S_SHIFT);
#else
  bool started = mic.beginI2S(I2S_BCLK_PIN, I2S_WS_PIN, I2S_DATA_PIN,
                              SAMPLE_RATE, I2S_STD_SLOT_RIGHT, I2S_SHIFT);
#endif
  if (!started) {
    Serial.println("ERROR: I2S initialization failed");
    while (true) delay(1000);
  }
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

  size_t count = mic.read(audioBuffer, BUFFER_LEN);
  if (count > 0) {
    Serial.write(reinterpret_cast<uint8_t *>(audioBuffer),
                 count * sizeof(int16_t));
  }
}
