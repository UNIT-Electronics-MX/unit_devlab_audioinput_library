#!/usr/bin/env python3
"""Play the PDM/ComputerAudio sketch's PCM stream on the computer.

Install:  pip install pyserial sounddevice
Usage:    python3 pc_audio.py /dev/ttyACM0   (native USB; /dev/ttyUSB0 with a USB-serial chip)
          python3 pc_audio.py /dev/ttyACM0 --device esp_mic   # to a virtual mic
          python3 pc_audio.py /dev/ttyACM0 --gain 2           # louder (default 1 = unchanged)
          python3 pc_audio.py /dev/ttyACM0 --rate 24000       # RP2040/RP2350 (sketch rate)
          python3 pc_audio.py /dev/ttyACM0 --meter            # print input level

Close the Arduino Serial Monitor first: only one program can use the port.

Virtual microphone (Linux, PulseAudio/PipeWire): create a sink and play into it,
then pick "Monitor of esp_mic" as the input in Zoom/Discord/OBS/etc.
    pactl load-module module-null-sink sink_name=esp_mic
    python3 pc_audio.py /dev/ttyACM0 --device esp_mic
"""
import argparse
import math
import threading
import time
from array import array

import serial
import sounddevice as sd

DEFAULT_RATE = 48000  # --rate must match SAMPLE_RATE in ComputerAudio.ino (24000 on RP)
BAUD_RATE = 921600   # must match BAUD_RATE in ComputerAudio.ino
HEADER = b"RAW\r\n"  # Serial.println() ends lines with \r\n
FRAME_MAGIC = b"AU"
MAX_FRAME_SAMPLES = 512  # BUFFER_LEN in ComputerAudio.ino
CHUNK_BYTES = 1024
BUFFER_SECONDS = 0.2  # older audio is dropped to keep latency low
STALL_SECONDS = 1  # re-request the stream after this long without data
DC_CUTOFF_HZ = 15    # DC blocker: the PDM mic output sits on a constant offset



class Amplifier:
    """Removes the DC offset, then applies a gain with clipping."""

    def __init__(self, gain, rate):
        self.gain = gain
        self.pole = 1 - 2 * math.pi * DC_CUTOFF_HZ / rate
        self.prev_in = 0.0
        self.prev_out = 0.0
        self.in_rms = 0.0
        self.in_peak = 0.0
        self.out_peak = 0

    def process(self, raw):
        samples = array("h")
        samples.frombytes(raw)
        in_sq = 0.0
        in_peak = 0.0
        out_peak = 0
        for i, x in enumerate(samples):
            y = x - self.prev_in + self.pole * self.prev_out
            self.prev_in, self.prev_out = x, y
            in_sq += y * y
            in_peak = max(in_peak, abs(y))
            samples[i] = max(-32768, min(32767, int(y * self.gain)))
            out_peak = max(out_peak, abs(samples[i]))
        self.in_rms = math.sqrt(in_sq / max(len(samples), 1))
        self.in_peak = in_peak
        self.out_peak = out_peak
        return samples.tobytes()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("port", help="serial port, e.g. /dev/ttyACM0 or /dev/ttyUSB0")
    parser.add_argument("--device", help="output device name or index (see: python3 -m sounddevice)")
    parser.add_argument("--rate", type=int, default=DEFAULT_RATE,
                        help=f"sample rate, must match SAMPLE_RATE in the sketch (default {DEFAULT_RATE}; 24000 on RP)")
    parser.add_argument("--gain", type=float, default=1.0,
                        help="software gain on the computer, 1 = unchanged (default 1)")
    parser.add_argument("--meter", action="store_true",
                        help="print the input level (before gain) twice a second")
    args = parser.parse_args()

    with serial.Serial(args.port, BAUD_RATE, timeout=0.1, write_timeout=1) as ser:
        try:
            wait_for_header(ser)
            stream(ser, args)
        finally:
            ser.write(b"S")


def wait_for_header(ser):
    ser.reset_input_buffer()
    window = b""
    deadline = time.monotonic() + 5
    last_request = 0
    while not window.endswith(HEADER):
        now = time.monotonic()
        if now > deadline:
            raise SystemExit(
                "No 'RAW' header received. Is PDM/ComputerAudio.ino uploaded, "
                f"and does its BAUD_RATE match {BAUD_RATE}?"
            )
        if now - last_request >= 0.5:
            ser.write(b"R")
            last_request = now
        window = (window + ser.read(1))[-len(HEADER):]


def extract_frames(buf):
    """Remove complete frames from buf; return (audio bytes, resync count).

    A frame whose checksum does not match lost bytes on the way; it is
    discarded and the search restarts at the next header.
    """
    audio = bytearray()
    resyncs = 0
    while True:
        start = buf.find(FRAME_MAGIC)
        if start < 0:
            del buf[:-1]  # keep a trailing 'A' that may start the next header
            return audio, resyncs
        if start:
            del buf[:start]
        if len(buf) < 4:
            return audio, resyncs
        count = int.from_bytes(buf[2:4], "little")
        end = 4 + 2 * count
        if not 0 < count <= MAX_FRAME_SAMPLES:
            del buf[:2]
            resyncs += 1
            continue
        if len(buf) < end + 2:
            return audio, resyncs
        samples = memoryview(buf)[4:end]
        checksum = sum(samples.cast("H")) & 0xFFFF
        samples.release()
        if checksum != int.from_bytes(buf[end:end + 2], "little"):
            del buf[:2]
            resyncs += 1
            continue
        audio += buf[4:end]
        del buf[:end + 2]


def stream(ser, args):
    # The serial reader and the audio callback run independently, so playback
    # never stalls the port. pending always holds a whole number of samples.
    amp = Amplifier(args.gain, args.rate)
    pending = bytearray()
    max_buffer = int(args.rate * 2 * BUFFER_SECONDS)
    lock = threading.Lock()

    def play(outdata, frames, time_info, status):
        with lock:
            n = min(len(outdata), len(pending))
            outdata[:n] = pending[:n]
            del pending[:n]
        outdata[n:] = b"\x00" * (len(outdata) - n)

    with sd.RawOutputStream(
        samplerate=args.rate, channels=1, dtype="int16", device=args.device, callback=play
    ):
        print("Streaming... Ctrl+C to stop")
        buf = bytearray()
        last_data = time.monotonic()
        last_meter = 0.0
        while True:
            chunk = ser.read(CHUNK_BYTES)
            if chunk:
                last_data = time.monotonic()
            elif time.monotonic() - last_data > STALL_SECONDS:
                # Last resort if the board stops sending: answering a new
                # request makes it write and flush again.
                print("Stream stalled, requesting it again...")
                wait_for_header(ser)
                buf.clear()
                last_data = time.monotonic()
                continue
            buf += chunk
            audio, resyncs = extract_frames(buf)
            if resyncs:
                print(f"Lost bytes on USB, resynced ({resyncs})")
            if audio:
                audio = amp.process(bytes(audio))
                with lock:
                    pending += audio
                    excess = len(pending) - max_buffer
                    if excess > 0:
                        del pending[:excess]
                if args.meter and time.monotonic() - last_meter >= 0.5:
                    last_meter = time.monotonic()
                    print(f"in rms {amp.in_rms:7.0f}  peak {amp.in_peak:7.0f}  "
                          f"| out peak {amp.out_peak:6d}/32767")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
