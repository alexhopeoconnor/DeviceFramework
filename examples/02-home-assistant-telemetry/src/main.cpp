#include <Arduino.h>
#include <ArduinoHA.h>
#include <DeviceFramework.h>
#include "FirmwareIdentity.h"

namespace {
constexpr char kReportInterval[] = "reportinterval";
constexpr unsigned long kDefaultReportIntervalMs = 10000;
HASensorNumber uptimeSensor("uptime", HASensorNumber::PrecisionP0);

unsigned long reportIntervalMs() {
    const float value = DeviceFramework::getParameterRegistry().getValueAsFloat(kReportInterval);
    return constrain(static_cast<unsigned long>(value), 1000UL, 60000UL);
}
}  // namespace

void setup() {
    FirmwareIdentity::configure();
    DeviceFramework::beforeSetup([] {
        auto& parameters = DeviceFramework::getParameterRegistry();
        parameters.setDefaultValue(
            DeviceFrameworkParameters::PARAM_DEVICE_NAME, "Telemetry Example"
        );

        DeviceFrameworkParameterMetadata interval;
        interval.id = kReportInterval;
        interval.label = "Telemetry report interval";
        interval.defaultValue = String(kDefaultReportIntervalMs);
        interval.maxLength = 5;
        interval.order = 50;
        interval.sources = SOURCE_WIFI_MANAGER | SOURCE_HOME_ASSISTANT;
        interval.haDeviceType = HAConfigDeviceType::NUMBER;
        interval.haConstraints.minValue = 1000;
        interval.haConstraints.maxValue = 60000;
        interval.haConstraints.step = 1000;
        interval.haUnitOfMeasurement = "ms";
        parameters.registerParameter(interval);
    });

    uptimeSensor.setName("Uptime");
    uptimeSensor.setUnitOfMeasurement("s");
    uptimeSensor.setIcon("mdi:timer-outline");
    DeviceFramework::setup();
}

void loop() {
    DeviceFramework::loop();

    static unsigned long lastReport = 0;
    if (millis() - lastReport >= reportIntervalMs()) {
        uptimeSensor.setValue(static_cast<uint32_t>(millis() / 1000UL));
        lastReport = millis();
    }
}
