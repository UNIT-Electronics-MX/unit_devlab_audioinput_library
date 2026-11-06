// Streams I2S microphone audio as mono PCM16 over Serial for computer playback.
// Run pc_audio.py on the computer; keep SAMPLE_RATE and BAUD_RATE in sync.
// Each block is framed as 'A' 'U' <sample count> <samples> <checksum>, with
// uint16 LE count and checksum (sum of the samples), so the computer can drop
// damaged frames and resync if the USB driver loses bytes.
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
#define TX_WAIT_MS  10  // max wait for Serial space before dropping a block

DevLab_AudioInput mic;
int16_t frame[2 + BUFFER_LEN + 1];  // header, samples, checksum
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
  // Answer queued 'R' requests with a single header; extra headers would
  // land in the audio stream and break the 2-byte sample alignment.
  bool startRequested = false;
  while (Serial.available() > 0) {
    char command = Serial.read();
    if (command == 'R') {
      startRequested = true;
    } else if (command == 'S') {
      startRequested = false;
      streamEnabled = false;
    }
  }
  if (startRequested) {
    Serial.println("RAW");
    Serial.flush();
    streamEnabled = true;
  }

  if (!streamEnabled) {
    delay(1);
    return;
  }

  size_t count = mic.read(frame + 2, BUFFER_LEN);
  if (count > 0) {
    uint8_t *bytes = reinterpret_cast<uint8_t *>(frame);
    bytes[0] = 'A';
    bytes[1] = 'U';
    bytes[2] = count & 0xFF;
    bytes[3] = count >> 8;
    uint16_t checksum = 0;
    for (size_t i = 0; i < count; i++) checksum += frame[2 + i];
    frame[2 + count] = checksum;
    writeFrame(bytes, 4 + (count + 1) * sizeof(int16_t));
  }
}

// If the computer stops reading, the rest of the frame is dropped; pc_audio.py
// discards incomplete frames and resyncs on the next header.
void writeFrame(const uint8_t *data, size_t len) {
  unsigned long start = millis();
  while (len > 0) {
    size_t room = Serial.availableForWrite();
    if (room == 0) {
      if (millis() - start > TX_WAIT_MS) {
        // ESP32 native USB can leave its TX interrupt disabled with a full
        // buffer; flush() re-arms it so sending resumes.
        Serial.flush();
        return;
      }
      delay(1);
      continue;
    }
    size_t n = room < len ? room : len;
    Serial.write(data, n);
    data += n;
    len -= n;
    start = millis();  // the timeout measures time without progress
  }
}
