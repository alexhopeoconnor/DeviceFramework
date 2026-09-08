# DeviceFramework device UI contract

Run this real-board browser contract through
[`../../tools/device-ui-hardware`](../../tools/device-ui-hardware), never by
starting its Compose file directly. The host command selects the one serial
board and, for portal coverage, attaches only the explicitly named secondary
Wi-Fi adapter. Docker uses host networking only to reach the already-routed
device address; it never changes host Wi-Fi configuration.

The portal suite proves DeviceFramework's real WiFiManager integration: no-Wi-Fi
boot, applied UI configuration, scanning, timeout reset, and optional station
hand-off. The web suite proves the board-served Device Status, Serial Output,
Controls, and About pages. A requested output directory receives screenshots,
browser diagnostics, and reports. It never receives local credentials.
