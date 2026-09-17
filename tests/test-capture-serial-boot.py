#!/usr/bin/env python3
"""Unit-check that passive boot capture never asserts ESP reset lines."""

import importlib.util
import sys
import tempfile
import types
from pathlib import Path


class FakeSerial:
    events = []

    def __init__(self, port=None, **kwargs):
        assert port is None
        assert kwargs["baudrate"] == 115200
        assert kwargs["timeout"] == 0.25
        assert kwargs["rtscts"] is False
        assert kwargs["dsrdtr"] is False
        self._dtr = True
        self._rts = True
        self._port = None
        self.is_open = False
        self.events.append(("init", port))

    @property
    def dtr(self):
        return self._dtr

    @dtr.setter
    def dtr(self, value):
        self._dtr = value
        self.events.append(("dtr", value))

    @property
    def rts(self):
        return self._rts

    @rts.setter
    def rts(self, value):
        self._rts = value
        self.events.append(("rts", value))

    @property
    def port(self):
        return self._port

    @port.setter
    def port(self, value):
        self._port = value
        self.events.append(("port", value))

    def open(self):
        assert self._dtr is False
        assert self._rts is False
        self.is_open = True
        self.events.append(("open", self._dtr, self._rts, self._port))

    def close(self):
        self.is_open = False
        self.events.append(("close",))

    def read(self, _size):
        return b""

    def __enter__(self):
        return self

    def __exit__(self, _type, _value, _traceback):
        self.close()


def load_capture_module():
    fake_serial_module = types.ModuleType("serial")
    fake_serial_module.Serial = FakeSerial
    sys.modules["serial"] = fake_serial_module

    source = Path(__file__).resolve().parents[1] / "tools" / "capture-serial-boot.py"
    spec = importlib.util.spec_from_file_location("capture_serial_boot", source)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    module = load_capture_module()

    serial_port = module.open_capture_port("/dev/fake")
    assert FakeSerial.events == [
        ("init", None),
        ("dtr", False),
        ("rts", False),
        ("port", "/dev/fake"),
        ("open", False, False, "/dev/fake"),
    ]
    serial_port.close()

    FakeSerial.events.clear()
    module.reset = lambda _serial_port: (_ for _ in ()).throw(
        AssertionError("passive capture called reset")
    )
    with tempfile.NamedTemporaryFile() as output:
        assert module.capture("/dev/fake", output.name, 0, [], False) == 0

    assert ("open", False, False, "/dev/fake") in FakeSerial.events
    print("Passive serial boot capture contract passed")


if __name__ == "__main__":
    main()
