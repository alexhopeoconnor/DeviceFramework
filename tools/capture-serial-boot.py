#!/usr/bin/env python3
"""Safely attach to serial and optionally reset one board for boot evidence."""

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


def open_capture_port(port):
    """Open a monitor without PySerial's default DTR/RTS assertion.

    ESP USB-UART auto-reset circuits commonly react to modem-control changes.
    Constructing ``Serial(port, ...)`` opens immediately with both lines
    asserted, which can manufacture a second physical reset after a serial
    uploader has already booted the application. Set inactive states while
    closed, then open the requested port. ``--reset`` remains available for
    explicit diagnostics; ordinary OTA capture deliberately does not use it.
    """
    serial_port = serial.Serial(
        port=None,
        baudrate=115200,
        timeout=0.25,
        rtscts=False,
        dsrdtr=False,
    )
    serial_port.dtr = False
    serial_port.rts = False
    serial_port.port = port
    serial_port.open()
    return serial_port


def capture(port, output, timeout_seconds, required, should_reset):
    remaining = set(required)
    deadline = time.monotonic() + timeout_seconds

    with (
        open_capture_port(port) as serial_port,
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
