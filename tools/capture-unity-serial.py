#!/usr/bin/env python3
"""Reset one serial test target and capture its complete Unity result."""

import argparse
import sys
import time

import serial


def reset_and_capture(port, output, timeout_seconds):
    with (
        serial.Serial(port, 115200, timeout=0.25) as serial_port,
        open(output, "ab", buffering=0) as output_file,
    ):
        # Hold GPIO0 high and pulse reset via the common ESP auto-reset circuit.
        serial_port.dtr = False
        serial_port.rts = False
        time.sleep(0.05)
        serial_port.rts = True
        time.sleep(0.10)
        serial_port.rts = False

        captured = bytearray()
        deadline = time.monotonic() + timeout_seconds
        while time.monotonic() < deadline:
            data = serial_port.read(4096)
            if not data:
                continue
            output_file.write(data)
            captured.extend(data)
            if b"\nOK\r\n" in captured or b"\nOK\n" in captured:
                return 0
            if b"last failed alloc" in captured or b"\nException (" in captured:
                sys.stderr.write("Target reset after an allocator or exception failure.\n")
                return 1

    sys.stderr.write("Unity serial result did not finish with OK before timeout.\n")
    return 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    return reset_and_capture(args.port, args.output, args.timeout)


if __name__ == "__main__":
    sys.exit(main())
