// Streams PDM microphone audio as mono PCM16 over Serial for computer playback.
// Run pc_audio.py on the computer (see the header of that file).
// SAMPLE_RATE and BAUD_RATE must match the values in pc_audio.py.
// Each block is framed as 'A' 'U' <sample count> <samples> <checksum>, with
// uint16 LE count and checksum (sum of the samples), so the computer can drop
// damaged frames and resync if the USB driver loses bytes.
#include <DevLab_AudioInput.h>

#if defined(ARDUINO_ARCH_RP2040)
#define PDM_CLK_PIN  10
#define PDM_DATA_PIN 11
#else
#define PDM_CLK_PIN  GPIO_NUM_2
#define PDM_DATA_PIN GPIO_NUM_3
#endif
// SAMPLE_RATE must match --rate in pc_audio.py (default 48000; use --rate 24000 on RP).
#if defined(ARDUINO_ARCH_RP2040)
#define SAMPLE_RATE  24000        // 24000 samples/s * 2 bytes (int16) = 48 KB/s
#else
#define SAMPLE_RATE  48000        // 48000 samples/s * 2 bytes (int16) = 96 KB/s
#endif
#define BAUD_RATE    921600       // ignored on native USB; a UART bridge carries ~92 KB/s
#define BUFFER_LEN   512
// Software gain on the board (50..800, 100 = unchanged), per platform:
//  - RP2040/RP2350: the core's output is about 4-6x lower above ~25 kHz, so use
//    400 there and 100 at lower rates (800 clipped ~4% of a voice at 16 kHz).
//  - ESP32: 600 compensates its lower PDM level; 800 clipped on loud sounds.
// Further volume adjustment is done on the computer with pc_audio.py --gain.
#if defined(ARDUINO_ARCH_RP2040)
#define GAIN_PERCENT ((SAMPLE_RATE) > 25000 ? 400 : 100)
#else
#define GAIN_PERCENT 600
#endif
#define TX_WAIT_MS   10           // max wait for Serial space before dropping a block

DevLab_AudioInput pdm;
int16_t frame[2 + BUFFER_LEN + 1];  // header, samples, checksum
bool streamEnabled = false;

void setup() {
  Serial.begin(BAUD_RATE);
  delay(1000);

  if (!pdm.beginPDM(PDM_CLK_PIN, PDM_DATA_PIN, SAMPLE_RATE)) {
    while (1) {  // repeat: the message is lost if the port is opened later
      Serial.println("ERROR: PDM init failed (sample rate not supported by the core?)");
      delay(1000);
    }
  }

  pdm.setGainPercent(GAIN_PERCENT);
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
    while (pdm.read(frame + 2, BUFFER_LEN, 0) > 0) {}  // drop audio buffered before the start
    streamEnabled = true;
  }

  if (!streamEnabled) {
    delay(1);
    return;
  }

  size_t count = pdm.read(frame + 2, BUFFER_LEN);
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
#if defined(ARDUINO_ARCH_RP2040)
      delayMicroseconds(100);  // delay(1) would cap the USB throughput below 96 KB/s
#else
      delay(1);
#endif
      continue;
    }
    size_t n = room < len ? room : len;
    Serial.write(data, n);
    data += n;
    len -= n;
    start = millis();  // the timeout measures time without progress
  }
}
