// Show decimated I2S samples in the Arduino Serial Plotter at 115200 baud.
#include <DevLab_AudioInput.h>

#if defined(ARDUINO_ARCH_RP2040)
#define I2S_BCLK_PIN 0
#define I2S_WS_PIN   1
#define I2S_DATA_PIN 2
#define I2S_SEL_PIN  3
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
#define I2S_BCLK_PIN GPIO_NUM_6
#define I2S_WS_PIN   GPIO_NUM_7
#define I2S_DATA_PIN GPIO_NUM_5
#define I2S_SEL_PIN  GPIO_NUM_4
#else
#define I2S_BCLK_PIN GPIO_NUM_36
#define I2S_WS_PIN   GPIO_NUM_37
#define I2S_DATA_PIN GPIO_NUM_35
#define I2S_SEL_PIN  GPIO_NUM_34
#endif
#define BUFFER_LEN   512
#define DECIMATION   48  // about 1000 plotted samples per second at 48 kHz

DevLab_AudioInput mic;
int16_t samples[BUFFER_LEN];

void setup() {
  Serial.begin(115200);
  pinMode(I2S_SEL_PIN, OUTPUT);
  digitalWrite(I2S_SEL_PIN, HIGH);  // RIGHT slot

#if defined(ARDUINO_ARCH_RP2040)
  bool started = mic.beginI2S(I2S_BCLK_PIN, I2S_WS_PIN, I2S_DATA_PIN,
                              48000, true, 16);
#else
  bool started = mic.beginI2S(I2S_BCLK_PIN, I2S_WS_PIN, I2S_DATA_PIN,
                              48000, I2S_STD_SLOT_RIGHT);
#endif
  if (!started) {
    Serial.println("ERROR: I2S initialization failed");
    while (true) delay(1000);
  }
}

void loop() {
  size_t count = mic.read(samples, BUFFER_LEN);
  for (size_t i = 0; i < count; i += DECIMATION) {
    Serial.println(samples[i]);
  }
}
