#include <Arduino.h>
#include <ArduinoHA.h>
#include <DeviceFramework.h>
#include "FirmwareIdentity.h"

namespace {
constexpr char kMaxOnSeconds[] = "maxonseconds";
#if !DF_LED_AVAILABLE
#error "Protected Output needs a board LED; define LED_BUILTIN and DF_LED_ACTIVE_LOW for this target."
#endif
constexpr uint8_t kOutputPin = DF_LED_PIN;
constexpr uint8_t kOffLevel = DF_LED_ACTIVE_LOW ? HIGH : LOW;
constexpr uint8_t kOnLevel = DF_LED_ACTIVE_LOW ? LOW : HIGH;

// This example controls only the configured demonstration indicator. Verify
// the board LED pin and polarity before relying on the visual result; replace
// both levels only after completing a real controller's safety design.

HASwitch protectedOutput("protected_output");
HABinarySensor timeoutLockout("output_timeout_lockout");

bool requestedOn = false;
bool appliedOn = false;
bool timeoutTripped = false;
bool outputEvaluationPending = true;
unsigned long outputStartedAt = 0;

uint32_t maximumOnMillis() {
    const int configuredSeconds = DeviceFramework::getParameterRegistry()
                                      .getValueAsInt(kMaxOnSeconds);
    return static_cast<uint32_t>(constrain(configuredSeconds, 10, 600)) * 1000UL;
}

void makeOutputSafe() {
    pinMode(kOutputPin, OUTPUT);
    digitalWrite(kOutputPin, kOffLevel);
}

void applyOutputState() {
    const bool mayApply = requestedOn && !timeoutTripped;
    if (appliedOn == mayApply) {
        return;
    }

    appliedOn = mayApply;
    digitalWrite(kOutputPin, appliedOn ? kOnLevel : kOffLevel);
    outputStartedAt = appliedOn ? millis() : 0;
    protectedOutput.setState(appliedOn);  // Report actual output, not request.
}

void updateSafetyState() {
    if (requestedOn && appliedOn &&
        (millis() - outputStartedAt) >= maximumOnMillis()) {
        timeoutTripped = true;
        outputEvaluationPending = true;
        timeoutLockout.setState(true);
    }
}

void onOutputCommand(bool state, HASwitch*) {
    requestedOn = state;
    if (!requestedOn) {
        timeoutTripped = false;
        timeoutLockout.setState(false);
    }
    outputEvaluationPending = true;
}

void onParameterChange(const String& id, const String&, const String&) {
    if (id == kMaxOnSeconds) {
        outputEvaluationPending = true;
    }
}

void configureEntities() {
    protectedOutput.setName("Protected output (LED demo)");
    protectedOutput.setIcon("mdi:led-on");
    protectedOutput.onCommand(onOutputCommand);

    timeoutLockout.setName("Output timeout lockout");
    timeoutLockout.setDeviceClass("safety");
}

void registerParameters() {
    auto& parameters = DeviceFramework::getParameterRegistry();
    parameters.setDefaultValue(DeviceFrameworkParameters::PARAM_DEVICE_NAME,
                               "Protected Output Example");

    DeviceFrameworkParameterMetadata maxOn;
    maxOn.id = kMaxOnSeconds;
    maxOn.label = "Maximum output-on time";
    maxOn.defaultValue = "60";
    maxOn.maxLength = 3;
    maxOn.order = 50;
    maxOn.valueType = DeviceFrameworkParameterValueType::UnsignedInteger;
    maxOn.hasNumericRange = true;
    maxOn.minValue = 10;
    maxOn.maxValue = 600;
    maxOn.sources = SOURCE_WIFI_MANAGER | SOURCE_HOME_ASSISTANT;
    maxOn.htmlAttributes.inputType = "number";
    maxOn.htmlAttributes.inputmode = "numeric";
    maxOn.haDeviceType = HAConfigDeviceType::NUMBER;
    maxOn.haConstraints.minValue = 10;
    maxOn.haConstraints.maxValue = 600;
    maxOn.haConstraints.step = 10;
    maxOn.haConstraints.precision = 0;
    maxOn.haUnitOfMeasurement = "s";
    parameters.registerParameter(maxOn);
}
}  // namespace

void setup() {
    makeOutputSafe();
    FirmwareIdentity::configure();
    configureEntities();
    DeviceFramework::beforeSetup(registerParameters);
    DeviceFramework::getParameterRegistry().setChangeCallback(onParameterChange);
    DeviceFramework::setup();

    protectedOutput.setState(false);
    timeoutLockout.setState(false);
    applyOutputState();
}

void loop() {
    DeviceFramework::loop();

    updateSafetyState();
    if (outputEvaluationPending) {
        outputEvaluationPending = false;
        applyOutputState();
    }
}
