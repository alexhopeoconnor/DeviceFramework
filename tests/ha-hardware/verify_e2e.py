"""DeviceFramework-specific assertions layered on the generic HA/MQTT testkit."""

from __future__ import annotations

import json
import os
import time
from pathlib import Path
from typing import Any, Callable

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


def publish_fixture_states(publisher: RetainedPublisher, fixture: dict[str, Any]) -> None:
    """Publish retained state before discovery so HA renders a known initial state."""
    for state in fixture.get("states", []):
        payload = state["payload"]
        publisher.publish(
            state["topic"],
            payload if isinstance(payload, str) else json.dumps(payload),
            retain=state.get("retain", True),
        )


def assert_fixture_states(client: HomeAssistantClient, entities: dict[str, dict[str, Any]]) -> None:
    """Ensure retained MQTT states have reached HA before visual capture."""
    by_domain = {entity_domain(entry): entry for entry in entities.values()}
    client.wait_for_state(by_domain["sensor"]["entity_id"], "1", timeout=90)
    client.wait_for_state(by_domain["switch"]["entity_id"], "off", timeout=90)
    wait_for_numeric_state(client, by_domain["number"]["entity_id"], 7)
    client.wait_for_state(by_domain["select"]["entity_id"], "normal", timeout=90)
    client.wait_for_state(by_domain["text"]["entity_id"], "ready", timeout=90)


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
            publish_fixture_states(publisher, fixture)
            payload = fixture["payload"]
            publisher.publish(fixture["topic"], json.dumps(payload), retain=fixture.get("retain", True))
        expected = fixture["expect"]
        device = wait_for_device(client, expected["device_identifier"])
        entities = assert_entities(client, device, expected["entities"])
        assert_fixture_states(client, entities)
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


def wait_for_numeric_state(
    client: HomeAssistantClient,
    entity_id: str,
    expected: float,
    *,
    timeout: int = 90,
) -> dict[str, Any]:
    def matches() -> dict[str, Any] | None:
        value = client.state(entity_id)
        if value is None:
            return None
        try:
            return value if float(value.get("state", "nan")) == expected else None
        except (TypeError, ValueError):
            return None

    return wait_until(f"Home Assistant state {entity_id}={expected}", matches, timeout=timeout)


def call_service_until_state(
    client: HomeAssistantClient,
    domain: str,
    service: str,
    data: dict[str, Any],
    wait_for_result: Callable[[int], Any],
    description: str,
) -> None:
    """Retry a command while HA's MQTT client reconnects after a broker restart."""
    last_error: ContractError | None = None
    for attempt in range(1, 4):
        try:
            client.call_service(domain, service, data)
            wait_for_result(30)
            return
        except ContractError as error:
            last_error = error
            if attempt < 3:
                time.sleep(3)
    raise ContractError(f"{description} did not converge after 3 service attempts") from last_error


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


def prepare_visual_states(
    client: HomeAssistantClient,
    entities: dict[str, dict[str, Any]],
    device_id: str,
) -> None:
    """Give fixture and hardware dashboards the same reviewed screenshot state."""
    switch = entities[f"{device_id}_e2e_switch"]
    call_service_until_state(
        client, "switch", "turn_off", {"entity_id": switch["entity_id"]},
        lambda timeout: client.wait_for_state(switch["entity_id"], "off", timeout=timeout),
        "switch baseline",
    )

    number = entities[f"{device_id}_e2enumber"]
    call_service_until_state(
        client, "number", "set_value", {"entity_id": number["entity_id"], "value": 7},
        lambda timeout: wait_for_numeric_state(client, number["entity_id"], 7, timeout=timeout),
        "number baseline",
    )

    select = entities[f"{device_id}_e2eselect"]
    call_service_until_state(
        client, "select", "select_option", {"entity_id": select["entity_id"], "option": "normal"},
        lambda timeout: client.wait_for_state(select["entity_id"], "normal", timeout=timeout),
        "select baseline",
    )

    text = entities[f"{device_id}_e2etext"]
    call_service_until_state(
        client, "text", "set_value", {"entity_id": text["entity_id"], "value": "ready"},
        lambda timeout: client.wait_for_state(text["entity_id"], "ready", timeout=timeout),
        "text baseline",
    )


def prepare_visual_state_contract() -> None:
    device_id = os.environ.get("E2E_EXPECTED_DEVICE_ID", "").strip()
    if not device_id:
        raise ContractError("E2E_EXPECTED_DEVICE_ID is required to prepare hardware UI state")
    client = HomeAssistantClient.from_state(HA_URL, STATE)
    try:
        device = wait_for_device(client, device_id)
        entities = assert_entities(client, device, expected_hardware_entities(device_id))
        prepare_visual_states(client, entities, device_id)
        write_json_artifact(
            ARTIFACTS,
            "ui-expected-state.json",
            {"switch": "off", "number": 7, "select": "normal", "text": "ready"},
        )
    finally:
        client.close()


def assert_browser_switch_round_trip() -> None:
    result_path = ARTIFACTS / "ui-result.json"
    if not result_path.exists():
        raise ContractError("browser visual test did not write its switch result")
    result = json.loads(result_path.read_text(encoding="utf-8"))
    entity_id = result.get("entity_id")
    expected_state = result.get("expected_state")
    if not isinstance(entity_id, str) or not isinstance(expected_state, str):
        raise ContractError(f"browser visual result is invalid: {result}")
    client = HomeAssistantClient.from_state(HA_URL, STATE)
    try:
        client.wait_for_state(entity_id, expected_state, timeout=90)
        write_json_artifact(ARTIFACTS, "ui-browser-round-trip.json", result)
    finally:
        client.close()


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
    elif MODE == "visual-state":
        prepare_visual_state_contract()
    elif MODE == "ready":
        pass
    elif MODE == "ui-after-browser":
        assert_browser_switch_round_trip()
    else:
        raise ContractError(f"unknown E2E_MODE: {MODE}")
    print(f"DeviceFramework HA hardware contract mode {MODE} passed")


if __name__ == "__main__":
    main()
