# DeviceFramework device UI test harness

Run this real-board browser test harness through
[`../../tools/device-ui-hardware`](../../tools/device-ui-hardware), never by
starting its Compose file directly. The host command selects the one serial
board and, for portal coverage, attaches only the explicitly named secondary
Wi-Fi adapter. Docker uses host networking only to reach the already-routed
device address; it never changes host Wi-Fi configuration.

The portal suite proves DeviceFramework's real WiFiManager integration: no-Wi-Fi
boot, applied UI configuration, scanning, timeout reset, and optional station
hand-off. The web suite proves the board-served Device Status, Serial Output,
Controls, and About pages. A requested output directory receives screenshots,
browser diagnostics, and reports.

For an owned station fixture, run the web authentication matrix through the
host tool:

```bash
./tools/device-ui-hardware web --platform esp8266 --port /dev/serial/by-id/usb-... \
  --env-file test/.env --auth both
```

It runs open first and protected second. Protected coverage proves anonymous
and wrong-credential rejection; open coverage uses no Basic-auth header at all.
The tool generates a Wi-Fi-only mode-600 browser input rather than mounting the
shared local env file, disables traces that could retain form bodies, and
removes the input after the run. Treat retained browser artifacts as private.
For a portal-to-station `full` run, the host must first prove the fresh generated
`.local` identity through Avahi and its system resolver; the container receives
only that one fresh mapping while preserving the hostname in browser requests.
