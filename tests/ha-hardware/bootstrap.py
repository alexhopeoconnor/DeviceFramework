"""Bootstrap the disposable Home Assistant volume used by the hardware harness."""

import os
from pathlib import Path

from ha_mqtt_test_harness import HomeAssistantClient


def main() -> None:
    client = HomeAssistantClient.bootstrap(
        os.environ.get("HA_URL", "http://homeassistant:8123"),
        Path("/state"),
        owner_name="DeviceFramework Hardware Test Harness Owner",
        username="deviceframework-hardware",
        password="deviceframework-hardware-password",
    )
    try:
        client.configure_mqtt(
            os.environ.get("MQTT_HOST", "mqtt"),
            int(os.environ.get("MQTT_PORT", "1883")),
        )
    finally:
        client.close()
    print("DeviceFramework Home Assistant MQTT integration is ready")


if __name__ == "__main__":
    main()
