# Portal First

This is the smallest complete DeviceFramework device. It has no selected local profile, so a new board starts the normal Wi-Fi provisioning portal. After joining a network, its local web interface, OTA service, mDNS, and MQTT lifecycle use the saved configuration.

1. Build and flash `esp8266` or `esp32`.
2. On a clean board, join the provisioning network announced by the device and open `http://192.168.4.1/`.
3. Enter Wi-Fi and any device settings. The board stores verified values and reconnects normally after reboot.

The example’s application ID, firmware version, and configuration schema live in `include/FirmwareIdentity.h`. They are deliberately product-neutral values you should replace in a real firmware.

See [getting started](../../docs/GETTING_STARTED.md) and the shared [examples guide](../README.md).
