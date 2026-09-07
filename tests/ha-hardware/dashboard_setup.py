"""Render the DeviceFramework UI dashboard after discovery has assigned entity IDs."""

from __future__ import annotations

import json
import os
import tempfile
from pathlib import Path
from typing import Any

from ha_mqtt_test_harness import TestHarnessError, HomeAssistantClient


ROOT = Path(__file__).parent
STATE = Path("/state")
CONFIG = Path("/config")
HA_URL = os.environ.get("HA_URL", "http://homeassistant:8123")
MODE = os.environ.get("E2E_MODE", "fixture")
DASHBOARD = CONFIG / "deviceframework-e2e-dashboard.yaml"
MANIFEST = STATE / "ui-manifest.json"


def load_fixture() -> dict[str, Any]:
    records = [
        json.loads(line)
        for line in (ROOT / "fixtures" / "deviceframework-e2e.jsonl").read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]
    if len(records) != 1:
        raise TestHarnessError(f"expected exactly one UI fixture record, found {len(records)}")
    return records[0]


def expected_hardware_entities(device_id: str) -> list[dict[str, str]]:
    return [
        {"key": "sensor", "unique_id": f"{device_id}_e2e_sensor"},
        {"key": "switch", "unique_id": f"{device_id}_e2e_switch"},
        {"key": "number", "unique_id": f"{device_id}_e2enumber"},
        {"key": "select", "unique_id": f"{device_id}_e2eselect"},
        {"key": "text", "unique_id": f"{device_id}_e2etext"},
    ]


def expected_entities() -> tuple[str, list[dict[str, str]]]:
    if MODE == "fixture":
        fixture = load_fixture()
        entries = fixture["expect"]["entities"]
        keys = ["sensor", "switch", "number", "select", "text"]
        return fixture["expect"]["device_identifier"], [
            {"key": key, "unique_id": entry["unique_id"]} for key, entry in zip(keys, entries, strict=True)
        ]
    if MODE == "hardware":
        device_id = os.environ.get("E2E_EXPECTED_DEVICE_ID", "").strip()
        if not device_id:
            raise TestHarnessError("E2E_EXPECTED_DEVICE_ID is required for hardware UI setup")
        return device_id, expected_hardware_entities(device_id)
    raise TestHarnessError(f"unsupported UI dashboard mode: {MODE}")


def render_dashboard(entity_ids: dict[str, str]) -> str:
    text = (ROOT / "config" / "deviceframework-e2e-dashboard.yaml.in").read_text(encoding="utf-8")
    for key, entity_id in entity_ids.items():
        text = text.replace(f"__{key.upper()}_ENTITY_ID__", entity_id)
    if "__" in text:
        raise TestHarnessError("dashboard template has unresolved placeholders")
    return text


def atomic_write(path: Path, text: str) -> None:
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=path.parent, delete=False) as output:
        output.write(text)
        temporary = Path(output.name)
    temporary.replace(path)


def main() -> None:
    device_id, expected = expected_entities()
    client = HomeAssistantClient.from_state(HA_URL, STATE)
    try:
        entity_ids = {
            entry["key"]: client.wait_for_entity(entry["unique_id"], timeout=90)["entity_id"]
            for entry in expected
        }
        for key, entity_id in entity_ids.items():
            if not isinstance(entity_id, str) or not entity_id:
                raise TestHarnessError(f"{key} UI entity has no entity_id")
        atomic_write(DASHBOARD, render_dashboard(entity_ids))
        manifest = {
            "dashboard_url": "http://homeassistant:8123/deviceframework-e2e/controls",
            "device_id": device_id,
            "entities": entity_ids,
            "mode": MODE,
        }
        STATE.mkdir(parents=True, exist_ok=True)
        atomic_write(MANIFEST, json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        MANIFEST.chmod(0o644)
    finally:
        client.close()
    print(f"DeviceFramework visual dashboard rendered for {device_id}")


if __name__ == "__main__":
    main()
