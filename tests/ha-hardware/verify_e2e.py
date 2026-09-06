"""DeviceFramework-specific assertions layered on the generic HA/MQTT testkit."""

from __future__ import annotations

import json
import os
from pathlib import Path
from typing import Any

from ha_mqtt_contract import (
    ContractError,
    HomeAssistantClient,
    MqttObserver,
    RetainedPublisher,
    wait_until,
    write_json_artifact,
    write_json_lines,
)


ROOT = Path(__file__).parent
STATE = Path("/state")
ARTIFACTS = Path("/artifacts")
MODE = os.environ.get("E2E_MODE", "fixture")
HA_URL = os.environ.get("HA_URL", "http://homeassistant:8123")
MQTT_HOST = os.environ.get("MQTT_HOST", "mqtt")
MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))


def load_fixture() -> dict[str, Any]:
    path = ROOT / "fixtures" / "deviceframework-e2e.jsonl"
    records = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    if len(records) != 1:
        raise ContractError(f"expected exactly one DeviceFramework fixture record, found {len(records)}")
    fixture = records[0]
    for field in ("topic", "payload", "expect"):
        if field not in fixture:
            raise ContractError(f"DeviceFramework fixture is missing {field}")
    return fixture


def entity_domain(entry: dict[str, Any]) -> str:
    return str(entry.get("entity_id", "")).split(".", 1)[0]


def find_device(client: HomeAssistantClient, identifier: str) -> dict[str, Any] | None:
    for device in client.device_registry():
        for candidate in device.get("identifiers", []):
            if isinstance(candidate, (list, tuple)) and identifier in candidate:
                return device
    return None


def wait_for_device(client: HomeAssistantClient, identifier: str) -> dict[str, Any]:
    return wait_until(
        f"Home Assistant device {identifier}",
        lambda: find_device(client, identifier),
        timeout=90,
    )


def assert_entities(
    client: HomeAssistantClient,
    device: dict[str, Any],
    expected: list[dict[str, str]],
) -> dict[str, dict[str, Any]]:
    matched: dict[str, dict[str, Any]] = {}
    for item in expected:
        unique_id = item["unique_id"]
        entry = client.wait_for_entity(unique_id, timeout=90)
        if entry.get("device_id") != device["id"]:
            raise ContractError(f"entity {unique_id} belongs to {entry.get('device_id')}, expected {device['id']}")
        if entity_domain(entry) != item["domain"]:
            raise ContractError(f"entity {unique_id} has domain {entity_domain(entry)}, expected {item['domain']}")
        matched[unique_id] = entry
    return matched


def write_snapshot(client: HomeAssistantClient, observer: MqttObserver, *, device: dict[str, Any] | None = None) -> None:
    write_json_lines(ARTIFACTS, "mqtt-discovery.raw.jsonl", observer.messages())
    write_json_artifact(ARTIFACTS, "ha-device-registry.json", client.device_registry())
    write_json_artifact(ARTIFACTS, "ha-entity-registry.json", client.entity_registry())
    if device:
        states = [
            client.state(entry["entity_id"])
            for entry in client.entities_for_device(device["id"])
            if entry.get("entity_id")
        ]
        write_json_artifact(ARTIFACTS, "ha-states.json", states)


def fixture_contract(*, republish: bool = True, identifier_suffix: str = "") -> None:
    fixture = load_fixture()
    if identifier_suffix:
        # This uses the same retained schema with a fresh discovery identity so
        # a broker-restart check cannot pass solely from HA's old registry data.
        fixture = json.loads(
            json.dumps(fixture).replace(
                "df_e2e_fixture", f"df_e2e_fixture_{identifier_suffix}"
            )
        )
    client = HomeAssistantClient.from_state(HA_URL, STATE)
    observer = MqttObserver(MQTT_HOST, MQTT_PORT)
    publisher = RetainedPublisher(MQTT_HOST, MQTT_PORT) if republish else None
    try:
        if publisher is not None:
            payload = fixture["payload"]
            publisher.publish(fixture["topic"], json.dumps(payload), retain=fixture.get("retain", True))
        expected = fixture["expect"]
        device = wait_for_device(client, expected["device_identifier"])
        assert_entities(client, device, expected["entities"])
        observer.wait_for_topic(fixture["topic"])
        write_snapshot(client, observer, device=device)
    finally:
        if publisher is not None:
            publisher.close()
        observer.close()
        client.close()


def expected_hardware_entities(device_id: str) -> list[dict[str, str]]:
    return [
        {"domain": "sensor", "unique_id": f"{device_id}_e2e_sensor"},
        {"domain": "switch", "unique_id": f"{device_id}_e2e_switch"},
        {"domain": "number", "unique_id": f"{device_id}_e2enumber"},
        {"domain": "select", "unique_id": f"{device_id}_e2eselect"},
        {"domain": "text", "unique_id": f"{device_id}_e2etext"},
    ]


def wait_for_numeric_state(client: HomeAssistantClient, entity_id: str, expected: float) -> dict[str, Any]:
    def matches() -> dict[str, Any] | None:
        value = client.state(entity_id)
        if value is None:
            return None
        try:
            return value if float(value.get("state", "nan")) == expected else None
        except (TypeError, ValueError):
            return None

    return wait_until(f"Home Assistant state {entity_id}={expected}", matches, timeout=90)


def assert_command_round_trips(client: HomeAssistantClient, entities: dict[str, dict[str, Any]], device_id: str) -> None:
    """Verify HA service -> MQTT command -> firmware state -> HA state for each control style."""
    switch = entities[f"{device_id}_e2e_switch"]
    client.call_service("switch", "turn_on", {"entity_id": switch["entity_id"]})
    client.wait_for_state(switch["entity_id"], "on", timeout=90)

    number = entities[f"{device_id}_e2enumber"]
    client.call_service("number", "set_value", {"entity_id": number["entity_id"], "value": 13})
    wait_for_numeric_state(client, number["entity_id"], 13)

    select = entities[f"{device_id}_e2eselect"]
    client.call_service("select", "select_option", {"entity_id": select["entity_id"], "option": "verbose"})
    client.wait_for_state(select["entity_id"], "verbose", timeout=90)

    text = entities[f"{device_id}_e2etext"]
    client.call_service("text", "set_value", {"entity_id": text["entity_id"], "value": "checked"})
    client.wait_for_state(text["entity_id"], "checked", timeout=90)


def normalize(value: Any, device_id: str) -> Any:
    if isinstance(value, str):
        return value.replace(device_id, "__DEVICE_ID__")
    if isinstance(value, list):
        return [normalize(item, device_id) for item in value]
    if isinstance(value, dict):
        return {key: normalize(item, device_id) for key, item in value.items() if key != "received_at"}
    return value


def hardware_contract(*, restart_only: bool = False) -> None:
    device_id = os.environ.get("E2E_EXPECTED_DEVICE_ID", "").strip()
    if not device_id:
        raise ContractError("E2E_EXPECTED_DEVICE_ID is required for hardware verification")
    client = HomeAssistantClient.from_state(HA_URL, STATE)
    observer = MqttObserver(MQTT_HOST, MQTT_PORT)
    try:
        topic = f"homeassistant/device/{device_id}/config"
        observer.wait_for_topic(topic, timeout=90)
        device = wait_for_device(client, device_id)
        entities = assert_entities(client, device, expected_hardware_entities(device_id))
        if not restart_only:
            assert_command_round_trips(client, entities, device_id)
        write_snapshot(client, observer, device=device)
        messages = [entry for entry in observer.messages() if entry["topic"] == topic]
        write_json_lines(
            ARTIFACTS,
            "mqtt-discovery.normalized.jsonl",
            [normalize(entry, device_id) for entry in messages],
        )
    finally:
        observer.close()
        client.close()


def main() -> None:
    ARTIFACTS.mkdir(parents=True, exist_ok=True)
    HomeAssistantClient.wait_until_ready(HA_URL)
    if MODE == "fixture":
        fixture_contract()
    elif MODE == "fixture-restart":
        fixture_contract(republish=False)
    elif MODE == "fixture-after-mqtt-restart":
        fixture_contract(identifier_suffix="after_mqtt_restart")
    elif MODE == "hardware":
        hardware_contract()
    elif MODE in {"ha-restart", "mqtt-restart"}:
        hardware_contract(restart_only=True)
    else:
        raise ContractError(f"unknown E2E_MODE: {MODE}")
    print(f"DeviceFramework HA hardware contract mode {MODE} passed")


if __name__ == "__main__":
    main()
