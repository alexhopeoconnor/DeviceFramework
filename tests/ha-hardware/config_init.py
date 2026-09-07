"""Seed the disposable HA configuration volume with a stable UI-test dashboard."""

from __future__ import annotations

import shutil
from pathlib import Path

from ha_mqtt_test_harness import TestHarnessError


SOURCE = Path("/tests/config")
TARGET = Path("/config")
CONFIGURATION = "configuration.yaml"
DASHBOARD = "deviceframework-e2e-dashboard.yaml"
MARKER = "# DeviceFramework HA hardware harness"


def copy_if_missing(name: str) -> None:
    target = TARGET / name
    if not target.exists():
        shutil.copyfile(SOURCE / name, target)


def main() -> None:
    TARGET.mkdir(parents=True, exist_ok=True)
    configuration = TARGET / CONFIGURATION
    if configuration.exists() and MARKER not in configuration.read_text(encoding="utf-8"):
        raise TestHarnessError(f"refusing to reuse a non-harness Home Assistant configuration at {configuration}")
    copy_if_missing(CONFIGURATION)
    copy_if_missing(DASHBOARD)
    print("DeviceFramework visual dashboard configuration is ready")


if __name__ == "__main__":
    main()
