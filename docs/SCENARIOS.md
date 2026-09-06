# Real-world scenario guides

These guides are derived from the patterns used by maintained DeviceFramework
consumers: temperature monitors, solar/battery and portable-load controllers,
pressure/presence devices, and dimmable lighting controllers. They describe
the reusable architecture, not a private product's credentials, pin mapping,
or electrical certification requirements.

| Scenario | Actual consumer pattern | Main lesson |
| --- | --- | --- |
| [Fault-tolerant temperature sensor](scenarios/fault-tolerant-temperature-sensor.md) | DS18B20 temperature-monitor firmware with interval/resolution/offset configuration | A missing sensor is an observable degraded condition, not a reason to stop provisioning and diagnostics. |
| [Protected output controller](scenarios/protected-output-controller.md) | Solar/battery and portable-load controllers with low-active relays, cutoff/recovery policy, and faults | Safe boot and actual applied state must outrank remote intent. |
| [Presence-aware lighting](scenarios/presence-aware-lighting.md) | Dual-channel LED/presence and dimmable-light controllers | HA/portal settings queue non-blocking peripheral work; native entities report live behaviour. |
| [Product-family deployment](scenarios/product-family-deployment.md) | Several separately versioned device sketches sharing DeviceFramework and source-owned branding | Stable identity, tagged dependencies, and optional profiles scale without copying framework boilerplate. |

Each scenario links the framework contract to a testable sequence. For an
executable starting point, use [Portal First](../examples/01-portal-first/),
[Home Assistant Telemetry](../examples/02-home-assistant-telemetry/), and the
[Protected Output](../examples/05-protected-output/) example.

Next: [Lifecycle](LIFECYCLE.md) · [Parameters](PARAMETERS.md) · [Operations](OPERATIONS.md).
