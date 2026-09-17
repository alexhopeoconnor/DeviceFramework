#!/usr/bin/env python3
"""Optionally reset one board and capture required boot markers from serial."""

import argparse
import sys
import time

import serial


def reset(serial_port):
    """Use the common ESP auto-reset circuit without changing the boot strap."""
    serial_port.dtr = False
    serial_port.rts = False
    time.sleep(0.05)
    serial_port.rts = True
    time.sleep(0.10)
    serial_port.rts = False


def capture(port, output, timeout_seconds, required, should_reset):
    remaining = set(required)
    deadline = time.monotonic() + timeout_seconds

    with (
        serial.Serial(port, 115200, timeout=0.25) as serial_port,
        open(output, "ab", buffering=0) as output_file,
    ):
        if should_reset:
            reset(serial_port)

        while time.monotonic() < deadline:
            data = serial_port.read(4096)
            if not data:
                continue
            output_file.write(data)
            text = data.decode("utf-8", errors="replace")
            sys.stdout.write(text)
            sys.stdout.flush()
            for marker in tuple(remaining):
                if marker in text:
                    remaining.remove(marker)
            if not remaining:
                return 0

    if remaining:
        sys.stderr.write(
            "Required serial boot marker(s) were not observed before timeout: {}\n".format(
                ", ".join(sorted(remaining))
            )
        )
        return 1
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--timeout", type=int, default=30)
    parser.add_argument("--require", action="append", default=[])
    parser.add_argument(
        "--reset",
        action="store_true",
        help="Pulse reset after opening serial; use only before the initial A image check.",
    )
    args = parser.parse_args()

    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    return capture(args.port, args.output, args.timeout, args.require, args.reset)


if __name__ == "__main__":
    sys.exit(main())
