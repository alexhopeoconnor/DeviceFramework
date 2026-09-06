# Portal First

This is the smallest complete DeviceFramework device. It has no selected local profile, so a new board starts the normal Wi-Fi provisioning portal. This example is built with `ENABLE_WEB_INTERFACE`, so its local web interface is available after Wi-Fi provisioning.

1. Build and flash `esp8266` or `esp32`.
2. On a clean board, join the **Portal First Example** provisioning network with the example password `default1`, then open `http://192.168.4.1/`.
3. Enter Wi-Fi and any device settings, then submit the form. The board verifies and saves the values before restarting.
4. After reboot, open `http://portal-first-example.local/` on a network with mDNS support, or use the DHCP address assigned by your router. The local web interface, OTA service, mDNS, and MQTT lifecycle now use the saved configuration.

> **Example credentials only:** `default1` is DeviceFramework's development password. Do not ship it; set an application-specific password through a local profile or the device controls before deployment.

The example’s application ID, firmware version, and configuration schema live in `include/FirmwareIdentity.h`. They are deliberately product-neutral values you should replace in a real firmware.

See [getting started](../../docs/GETTING_STARTED.md), the [lifecycle contract](../../docs/LIFECYCLE.md), and the shared [examples guide](../README.md).
