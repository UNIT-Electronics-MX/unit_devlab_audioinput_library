#!/usr/bin/env python3
"""Play audio from the I2S/ComputerAudio sketch on the computer.

Install:  pip install pyserial sounddevice
Usage:    python3 pc_audio.py /dev/ttyUSB0
          python3 pc_audio.py /dev/ttyUSB0 --device esp_mic

Close Arduino Serial Monitor before opening the port with this script.

On Linux with PulseAudio/PipeWire, a virtual microphone can be created with:
    pactl load-module module-null-sink sink_name=esp_mic
    python3 pc_audio.py /dev/ttyUSB0 --device esp_mic
Then select "Monitor of esp_mic" as the recording input in your application.
"""

import argparse
import time

import serial
import sounddevice as sd

SAMPLE_RATE = 16000  # must match ComputerAudio.ino
BAUD_RATE = 921600    # must match ComputerAudio.ino
HEADER = b"RAW\r\n"
CHUNK_BYTES = 1024


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("port", help="serial port, e.g. /dev/ttyUSB0")
    parser.add_argument("--device", help="output device name or index (see: python3 -m sounddevice)")
    args = parser.parse_args()

    with serial.Serial(args.port, BAUD_RATE, timeout=0.1) as ser, sd.RawOutputStream(
        samplerate=SAMPLE_RATE, channels=1, dtype="int16", device=args.device
    ) as out:
        ser.reset_input_buffer()
        window = b""
        deadline = time.monotonic() + 5
        last_request = 0
        while not window.endswith(HEADER):
            now = time.monotonic()
            if now > deadline:
                raise SystemExit(
                    "No 'RAW' header received. Is I2S/ComputerAudio.ino uploaded, "
                    f"and does its BAUD_RATE match {BAUD_RATE}?"
                )
            if now - last_request >= 0.5:
                ser.write(b"R")
                last_request = now
            window = (window + ser.read(1))[-len(HEADER):]

        print("Streaming... Ctrl+C to stop")
        leftover = b""
        while True:
            data = leftover + ser.read(CHUNK_BYTES)
            usable = len(data) - (len(data) % 2)
            leftover = data[usable:]
            if usable:
                out.write(data[:usable])


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
