#!/usr/bin/env python3
"""Play the PDM/ComputerAudio sketch's PCM stream on the computer.

Install:  pip install pyserial sounddevice
Usage:    python3 pc_audio.py /dev/ttyUSB0
          python3 pc_audio.py /dev/ttyUSB0 --device esp_mic   # to a virtual mic

Close the Arduino Serial Monitor first: only one program can use the port.

Virtual microphone (Linux, PulseAudio/PipeWire): create a sink and play into it,
then pick "Monitor of esp_mic" as the input in Zoom/Discord/OBS/etc.
    pactl load-module module-null-sink sink_name=esp_mic
    python3 pc_audio.py /dev/ttyUSB0 --device esp_mic
"""
import argparse
import time

import serial
import sounddevice as sd

SAMPLE_RATE = 16000  # must match SAMPLE_RATE in PDM_ComputerAudio.ino
BAUD_RATE = 921600   # must match BAUD_RATE in PDM_ComputerAudio.ino
HEADER = b"RAW\r\n"  # Serial.println() ends lines with \r\n
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
                    "No 'RAW' header received. Is PDM_ComputerAudio.ino uploaded, and does "
                    f"its BAUD_RATE match {BAUD_RATE}?"
                )
            if now - last_request >= 0.5:
                ser.write(b"R")
                last_request = now
            window = (window + ser.read(1))[-len(HEADER):]
        print("Streaming... Ctrl+C to stop")

        leftover = b""
        while True:
            data = leftover + ser.read(CHUNK_BYTES)
            usable = len(data) - (len(data) % 2)  # keep whole 16-bit samples
            leftover = data[usable:]
            if usable:
                out.write(data[:usable])


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
