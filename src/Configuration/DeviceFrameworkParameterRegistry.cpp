#include "DeviceFrameworkParameterRegistry.h"
#include "DeviceFrameworkParameters.h"
#include "../MQTT/DeviceFrameworkMQTT.h"
#include "../Storage/DeviceFrameworkStorage.h"
#include "../DeviceFrameworkDebug.h"
#include <WiFiManager.h>
#include <algorithm>

// Forward declaration for WiFi access
class DeviceFrameworkWiFi {
public:
    static WiFiManager& getWiFiManager();
};

// Static member initialization
std::map<void*, String> DeviceFrameworkParameterRegistry::haDeviceToParamId;
DeviceFrameworkParameterRegistry* DeviceFrameworkParameterRegistry::instance = nullptr;

bool DeviceFrameworkParameterRegistry::isSensitiveParameter(const String& id) const {
    const DeviceFrameworkParameterMetadata* metadata = getMetadata(id);
    if (metadata && metadata->htmlAttributes.inputType.equalsIgnoreCase("password")) {
        return true;
    }

    String normalizedId = id;
    normalizedId.toLowerCase();
    return normalizedId.indexOf("password") >= 0 ||
           normalizedId.indexOf("passwd") >= 0 ||
           normalizedId.indexOf("secret") >= 0 ||
           normalizedId.indexOf("token") >= 0;
}

String DeviceFrameworkParameterRegistry::valueForLog(const String& id, const String& value) const {
    if (!isSensitiveParameter(id)) {
        return value;
    }
    return String(F("[redacted, ")) + String(value.length()) + F(" chars]");
}

DeviceFrameworkParameterRegistry::DeviceFrameworkParameterRegistry() :
    parameterCount(0),
    mqttReady(false),
    haResyncPending(false),
    haResyncNextIndex(0),
    lastHAResyncAt(0),
    changeCallback(nullptr) {
    instance = this;
    // Allocate parameters array to full size upfront
    parameters = new DeviceFrameworkParameterEntry[CONFIG_maxParameters];
    if (parameters == nullptr) {
        LOG_ERRORLN(F("Failed to allocate parameters array in constructor"));
    }
    // Initialize fixed-size arrays to null - will allocate during setup
    wifiManagerRefs = nullptr;
    wifiManagerRefCount = 0;
    haDeviceRefs = nullptr;
    haDeviceRefCount = 0;
}

DeviceFrameworkParameterRegistry::~DeviceFrameworkParameterRegistry() {
    // Clean up dynamically allocated parameters array
    if (parameters != nullptr) {
        delete[] parameters;
        parameters = nullptr;
    }

    // Clean up dynamically allocated WiFiManagerParameters
    if (wifiManagerRefs != nullptr) {
        for (size_t i = 0; i < wifiManagerRefCount; i++) {
            if (wifiManagerRefs[i].parameter != nullptr) {
                delete wifiManagerRefs[i].parameter;
                wifiManagerRefs[i].parameter = nullptr;
            }
        }
        delete[] wifiManagerRefs;
        wifiManagerRefs = nullptr;
    }

    // Clean up dynamically allocated HA devices
    if (haDeviceRefs != nullptr) {
        for (size_t i = 0; i < haDeviceRefCount; i++) {
            if (haDeviceRefs[i].device != nullptr) {
                switch (haDeviceRefs[i].deviceType) {
                    case HAConfigDeviceType::NUMBER:
                        delete static_cast<HANumber*>(haDeviceRefs[i].device);
                        break;
                    case HAConfigDeviceType::SWITCH:
                        delete static_cast<HASwitch*>(haDeviceRefs[i].device);
                        break;
                    case HAConfigDeviceType::SELECT:
                        delete static_cast<HASelect*>(haDeviceRefs[i].device);
                        break;
                    case HAConfigDeviceType::TEXT:
                        delete static_cast<HAText*>(haDeviceRefs[i].device);
                        break;
                    default:
                        break;
                }
                haDeviceRefs[i].device = nullptr;
            }
        }
        delete[] haDeviceRefs;
        haDeviceRefs = nullptr;
    }
}

// Parameter registration
bool DeviceFrameworkParameterRegistry::registerParameter(const DeviceFrameworkParameterMetadata& meta) {
    if (meta.id.length() == 0) {
        LOG_ERRORLN(F("Cannot register parameter with empty ID"));
        return false;
    }

    // Validate that ID contains only alphanumeric characters
    for (unsigned int i = 0; i < meta.id.length(); i++) {
        char c = meta.id[i];
        if (!isalnum(c)) {
            LOG_ERROR_SP(F("Parameter ID must contain only alphanumeric characters (a-z, A-Z, 0-9): "), true);
            LOG_ERRORLN_SP(meta.id, false);
            return false;
        }
    }

    if (hasParameter(meta.id)) {
        LOG_WARN_SP(F("Parameter already registered: "), true);
        LOG_WARNLN_SP(meta.id, false);
        return false;
    }

    // Insert into array
    return addParameter(meta);
}

bool DeviceFrameworkParameterRegistry::hasParameter(const String& id) const {
    return findParameter(id) != nullptr;
}

// Value access
String DeviceFrameworkParameterRegistry::getValue(const String& id) const {
    const DeviceFrameworkParameterEntry* entry = findParameter(id);
    if (entry != nullptr) {
        return entry->value.asString();
    }

    return "";
}

bool DeviceFrameworkParameterRegistry::setValue(const String& id, const String& value, DeviceFrameworkParameterUpdateOrigin origin) {
    DeviceFrameworkParameterEntry* entry = findParameter(id);
    if (entry == nullptr) {
        LOG_WARN_SP(F("Cannot set value for unregistered parameter: "), true);
        LOG_WARNLN_SP(id, false);
        return false;
    }

    // Truncate to maxLength (maxLength excludes null terminator, which is written separately during save)
    String validatedValue = value;
    uint8_t maxLength = entry->metadata.maxLength;
    if (validatedValue.length() > maxLength) {
        const String originalForLog = valueForLog(id, validatedValue);
        validatedValue = validatedValue.substring(0, maxLength);
        LOG_WARN_SP(F("Parameter value exceeds maxLength ("), true);
        LOG_WARN_SP(String(maxLength), false);
        LOG_WARN_SP(F("), truncating: "), false);
        LOG_WARN_SP(id, false);
        LOG_WARN_SP(F(" = '"), false);
        LOG_WARN_SP(originalForLog, false);
        LOG_WARN_SP(F("' -> '"), false);
        LOG_WARNLN_SP(valueForLog(id, validatedValue) + "'", false);
    }

    String oldValue = entry->value.asString();
    entry->value.setValue(validatedValue);

    LOG_DEBUG_SP(F("Parameter updated: "), true);
    LOG_DEBUG_SP(id, false);
    LOG_DEBUG_SP(F(" = "), false);
    LOG_DEBUGLN_SP(valueForLog(id, validatedValue), false);

    notifyValueChanged(id, oldValue, validatedValue, origin);

    return true;
}

bool DeviceFrameworkParameterRegistry::setValue(const String& id, const char* value, DeviceFrameworkParameterUpdateOrigin origin) {
    // Explicit overload to prevent const char* -> bool implicit conversion
    // This ensures string literals and char pointers call the String overload
    return setValue(id, String(value ? value : ""), origin);
}

bool DeviceFrameworkParameterRegistry::setValue(const String& id, int value, DeviceFrameworkParameterUpdateOrigin origin) {
    return setValue(id, String(value), origin);
}

bool DeviceFrameworkParameterRegistry::setValue(const String& id, float value, int decimalPlaces, DeviceFrameworkParameterUpdateOrigin origin) {
    return setValue(id, String(value, decimalPlaces), origin);
}

bool DeviceFrameworkParameterRegistry::setValue(const String& id, bool value, DeviceFrameworkParameterUpdateOrigin origin) {
    return setValue(id, value ? "true" : "false", origin);
}

// Convenience getters with type conversion
int DeviceFrameworkParameterRegistry::getValueAsInt(const String& id) const {
    return getValue(id).toInt();
}

float DeviceFrameworkParameterRegistry::getValueAsFloat(const String& id) const {
    return getValue(id).toFloat();
}

bool DeviceFrameworkParameterRegistry::getValueAsBool(const String& id) const {
    String val = getValue(id);
    return val == "1" || val.equalsIgnoreCase("true") ||
           val.equalsIgnoreCase("on") || val.equalsIgnoreCase("yes");
}

const char* DeviceFrameworkParameterRegistry::getValueAsCStr(const String& id) const {
    const DeviceFrameworkParameterEntry* entry = findParameter(id);
    if (entry != nullptr) {
        return entry->value.c_str();
    }

    return "";
}

// Metadata access
const DeviceFrameworkParameterMetadata* DeviceFrameworkParameterRegistry::getMetadata(const String& id) const {
    const DeviceFrameworkParameterEntry* entry = findParameter(id);
    if (entry != nullptr) {
        return &entry->metadata;
    }
    return nullptr;
}

ParameterIdList DeviceFrameworkParameterRegistry::getParameterIds() const {
    if (parameterCount == 0) {
        return ParameterIdList();
    }

    String* ids = new String[parameterCount];
    for (size_t i = 0; i < parameterCount; i++) {
        ids[i] = parameters[i].metadata.id;
    }

    return ParameterIdList(ids, parameterCount);
}

ParameterIdList DeviceFrameworkParameterRegistry::getParameterIdsSorted() const {
    return getParameterIdsSorted(SOURCE_ALL);
}

ParameterIdList DeviceFrameworkParameterRegistry::getParameterIds(DeviceFrameworkParameterSource source) const {
    if (parameterCount == 0) {
        return ParameterIdList();
    }

    // Count matching parameters first
    size_t matchCount = 0;
    for (size_t i = 0; i < parameterCount; i++) {
        if (source == DeviceFrameworkParameterSource::SOURCE_ALL ||
            (parameters[i].metadata.sources & source) != 0) {
            matchCount++;
        }
    }

    if (matchCount == 0) {
        return ParameterIdList();
    }

    // Create array of matching String objects
    String* matchingIds = new String[matchCount];
    size_t index = 0;

    for (size_t i = 0; i < parameterCount; i++) {
        if (source == DeviceFrameworkParameterSource::SOURCE_ALL ||
            (parameters[i].metadata.sources & source) != 0) {
            matchingIds[index++] = parameters[i].metadata.id;
        }
    }

    return ParameterIdList(matchingIds, matchCount);
}

ParameterIdList DeviceFrameworkParameterRegistry::getParameterIdsSorted(DeviceFrameworkParameterSource source) const {
    if (parameterCount == 0) {
        return ParameterIdList();
    }

    // Count matching parameters first
    size_t matchCount = 0;
    for (size_t i = 0; i < parameterCount; i++) {
        if (source == DeviceFrameworkParameterSource::SOURCE_ALL ||
            (parameters[i].metadata.sources & source) != 0) {
            matchCount++;
        }
    }

    if (matchCount == 0) {
        return ParameterIdList();
    }

    // Create array of indices for matching parameters
    size_t* indices = new size_t[matchCount];
    size_t index = 0;

    for (size_t i = 0; i < parameterCount; i++) {
        if (source == DeviceFrameworkParameterSource::SOURCE_ALL ||
            (parameters[i].metadata.sources & source) != 0) {
            indices[index++] = i;
        }
    }

    // Sort indices by order field using bubble sort
    for (size_t i = 0; i < matchCount - 1; i++) {
        for (size_t j = 0; j < matchCount - i - 1; j++) {
            if (parameters[indices[j]].metadata.order > parameters[indices[j + 1]].metadata.order) {
                // Swap indices
                size_t temp = indices[j];
                indices[j] = indices[j + 1];
                indices[j + 1] = temp;
            }
        }
    }

    // Create sorted array of String objects (copies)
    String* sortedIds = new String[matchCount];
    for (size_t i = 0; i < matchCount; i++) {
        sortedIds[i] = parameters[indices[i]].metadata.id;
    }

    // Clean up indices array
    delete[] indices;

    return ParameterIdList(sortedIds, matchCount);
}

// Metadata modification
bool DeviceFrameworkParameterRegistry::setDefaultValue(const String& id, const String& defaultValue) {
    DeviceFrameworkParameterEntry* entry = findParameter(id);
    if (entry == nullptr) {
        LOG_WARN_SP(F("Cannot set default for unregistered parameter: "), true);
        LOG_WARNLN_SP(id, false);
        return false;
    }

    // Truncate to maxLength (maxLength excludes null terminator, which is written separately during save)
    String validatedDefault = defaultValue;
    uint8_t maxLength = entry->metadata.maxLength;
    if (validatedDefault.length() > maxLength) {
        LOG_WARN_SP(F("Default value exceeds maxLength ("), true);
        LOG_WARN_SP(String(maxLength), false);
        LOG_WARN_SP(F("), truncating: "), false);
        LOG_WARN_SP(id, false);
        LOG_WARN_SP(F(" = '"), false);
        LOG_WARN_SP(validatedDefault, false);
        LOG_WARN_SP(F("' -> '"), false);
        validatedDefault = validatedDefault.substring(0, maxLength);
        LOG_WARNLN_SP(validatedDefault + "'", false);
    }

    // Update the default value in metadata
    entry->metadata.defaultValue = validatedDefault;

    // Also update the current value to the new default (already validated/truncated above)
    entry->value.setValue(validatedDefault);

    LOG_DEBUG_SP(F("Updated default for parameter: "), true);
    LOG_DEBUG_SP(id, false);
    LOG_DEBUG_SP(F(" = "), false);
    LOG_DEBUGLN_SP(validatedDefault, false);

    return true;
}

// Generate custom HTML attributes string from metadata
String DeviceFrameworkParameterRegistry::generateCustomHTML(const HTMLInputAttributes& attrs) const {
    String html = "";

    // Add input type if not default text
    if (attrs.inputType.length() > 0 && attrs.inputType != "text") {
        html += " type='" + attrs.inputType + "'";
    }

    // Add autocapitalize
    if (attrs.autocapitalize.length() > 0) {
        html += " autocapitalize='" + attrs.autocapitalize + "'";
    }

    // Add autocorrect
    if (attrs.autocorrect.length() > 0) {
        html += " autocorrect='" + attrs.autocorrect + "'";
    }

    // Add autocomplete
    if (attrs.autocomplete.length() > 0) {
        html += " autocomplete='" + attrs.autocomplete + "'";
    }

    // Add inputmode
    if (attrs.inputmode.length() > 0) {
        html += " inputmode='" + attrs.inputmode + "'";
    }

    // Add placeholder if specified
    if (attrs.placeholder.length() > 0) {
        html += " placeholder='" + attrs.placeholder + "'";
    }

    return html;
}

// Generate complete custom HTML for input elements like select dropdowns
String DeviceFrameworkParameterRegistry::generateCustomInputHTML(const DeviceFrameworkParameterMetadata& meta) const {
    String html = "";

    // For select elements, generate complete HTML
    if (meta.htmlAttributes.inputType == "select") {
        html += "<br/>";
        html += "<label for='" + meta.id + "'>" + meta.label + "</label>";
        html += "<select name='" + meta.id + "' id='" + meta.id + "' class='button'";

        // Add autocomplete if specified
        if (meta.htmlAttributes.autocomplete.length() > 0) {
            html += " autocomplete='" + meta.htmlAttributes.autocomplete + "'";
        }

        html += ">";

        // Parse options and generate option elements
        if (meta.htmlAttributes.options.length() > 0) {
            String currentValue = getValue(meta.id);
            int start = 0;
            size_t end = 0;
            int optionIndex = 0;

            while (end < meta.htmlAttributes.options.length()) {
                // Find next semicolon or end of string
                while (end < meta.htmlAttributes.options.length() && meta.htmlAttributes.options.charAt(end) != ';') {
                    end++;
                }

                String option = meta.htmlAttributes.options.substring(start, end);
                option.trim(); // Remove whitespace

                if (option.length() > 0) {
                    html += "<option value='" + option + "'";

                    // Check if this option should be selected
                    if (currentValue == option || (currentValue == "" && optionIndex == 0)) {
                        html += " selected";
                    }

                    html += ">" + option + "</option>";
                    optionIndex++;
                }

                // Move to next option
                start = end + 1;
                end = start;
            }
        }

        html += "</select>";
        return html;
    }

    // For other custom input types, return empty string (use regular parameter)
    return html;
}

// WiFiManager integration
WiFiManagerParameterList DeviceFrameworkParameterRegistry::createWiFiManagerParameters() {
    String paramList = "";

    // Get only WiFiManager parameters in sorted order
    auto paramIds = getParameterIdsSorted(DeviceFrameworkParameterSource::SOURCE_WIFI_MANAGER);

    if (paramIds.count == 0) {
        return WiFiManagerParameterList();
    }

    // Allocate fixed-size array for WiFiManager parameter references
    wifiManagerRefs = new WiFiManagerParameterRef[paramIds.count];
    wifiManagerRefCount = paramIds.count;

    // Create array directly
    WiFiManagerParameter** paramArray = new WiFiManagerParameter*[paramIds.count];
    size_t paramIndex = 0;

    for (size_t i = 0; i < paramIds.count; i++) {
        const String& id = paramIds.ids[i];
        const DeviceFrameworkParameterMetadata* meta = getMetadata(id);
        if (!meta) continue;

        // Set up the reference in the fixed array
        wifiManagerRefs[i].parameterId = id;
        wifiManagerRefs[i].label = meta->label;

        // Check if this is a custom HTML input (like select dropdown)
        String customInputHTML = generateCustomInputHTML(*meta);
        if (customInputHTML.length() > 0) {
            // Use custom HTML constructor for select dropdowns, etc.
            wifiManagerRefs[i].customHTML = customInputHTML;
            wifiManagerRefs[i].parameter = new WiFiManagerParameter(wifiManagerRefs[i].customHTML.c_str());
            paramArray[paramIndex++] = wifiManagerRefs[i].parameter;
        } else {
            // Use regular constructor for normal input fields
            wifiManagerRefs[i].customHTML = generateCustomHTML(meta->htmlAttributes);
            wifiManagerRefs[i].parameter = new WiFiManagerParameter(
                wifiManagerRefs[i].parameterId.c_str(),  // Stable pointer to stored ID
                wifiManagerRefs[i].label.c_str(),         // Stable pointer to stored label
                getValue(id).c_str(),                     // WiFiManager copies this, so temporary is OK
                meta->maxLength,
                wifiManagerRefs[i].customHTML.c_str()     // Stable pointer to stored custom HTML
            );
            paramArray[paramIndex++] = wifiManagerRefs[i].parameter;
        }

        // Build parameter list
        if (paramList.length() > 0) paramList += ", ";
        paramList += id;
    }

    if (paramIds.count > 0) {
        LOG_DEBUG_SP(F("Created "), true);
        LOG_DEBUG_SP(String(paramIds.count), false);
        LOG_DEBUG_SP(F(" WiFiManager parameter(s): "), false);
        LOG_DEBUGLN_SP(paramList, false);
    }

    return WiFiManagerParameterList(paramArray, paramIds.count);
}

void DeviceFrameworkParameterRegistry::syncFromWiFiManager(WiFiManager::WiFiManagerRequestArgs requestArgs) {
    LOG_DEBUGLN(F("Syncing parameters from WiFiManager..."));

    // NO SERVER ACCESS NEEDED! Use RequestArgs instead

    for (size_t i = 0; i < wifiManagerRefCount; i++) {
        WiFiManagerParameterRef& ref = wifiManagerRefs[i];
        if (ref.parameter != nullptr) {
            String newValue;

            // Check if this is a custom HTML parameter (ID is null)
            if (ref.parameter->getID() == nullptr) {
                // For custom HTML parameters, get value from RequestArgs
                if (requestArgs.hasArg(ref.parameterId)) {
                    newValue = requestArgs.getArg(ref.parameterId);
                    LOG_DEBUG_SP(F("Custom HTML parameter received: "), true);
                    LOG_DEBUGLN_SP(ref.parameterId, false);
                } else {
                    LOG_DEBUG_SP(F("No argument found for custom HTML parameter: "), true);
                    LOG_DEBUGLN_SP(ref.parameterId, false);
                    continue;
                }
            } else {
                // For regular parameters, get value from parameter object
                // (doParamSave already populated these)
                const char* paramValue = ref.parameter->getValue();
                newValue = String(paramValue);

                // Fallback to RequestArgs if parameter wasn't updated
                if (newValue.length() == 0 && requestArgs.hasArg(ref.parameterId)) {
                    newValue = requestArgs.getArg(ref.parameterId);
                }
            }

            String currentValue = getValue(ref.parameterId);

            if (newValue != currentValue) {
                setValue(ref.parameterId, newValue, DeviceFrameworkParameterUpdateOrigin::WIFI_MANAGER);
                LOG_DEBUG_SP(F("Synced from WiFiManager: "), true);
                LOG_DEBUG_SP(ref.parameterId, false);
                LOG_DEBUG_SP(F(" = "), false);
                LOG_DEBUGLN_SP(valueForLog(ref.parameterId, newValue), false);
            } else {
                LOG_DEBUG_SP(F("Skipped (no change): "), true);
                LOG_DEBUGLN_SP(ref.parameterId, false);
            }
        }
    }
}

void DeviceFrameworkParameterRegistry::syncToWiFiManager(const String& id) {
    // Find the WiFiManager parameter for this ID
    for (size_t i = 0; i < wifiManagerRefCount; i++) {
        WiFiManagerParameterRef& ref = wifiManagerRefs[i];
        if (ref.parameterId == id && ref.parameter != nullptr) {
            String value = getValue(id);
            const DeviceFrameworkParameterMetadata* meta = getMetadata(id);

            if (meta != nullptr) {
                // Check if value is different before syncing
                const char* currentWmValue = ref.parameter->getValue();
                if (String(currentWmValue) != value) {
                    ref.parameter->setValue(value.c_str(), meta->maxLength);
                    LOG_DEBUG_SP(F("Synced to WiFiManager: "), true);
                    LOG_DEBUG_SP(id, false);
                    LOG_DEBUG_SP(F(" = "), false);
                    LOG_DEBUGLN_SP(valueForLog(id, value), false);
                } else {
                    LOG_DEBUG_SP(F("Skipped (no change) to WiFiManager: "), true);
                    LOG_DEBUGLN_SP(id, false);
                }
            }
            break;
        }
    }
}

WiFiManagerParameter* DeviceFrameworkParameterRegistry::getWiFiManagerParameter(const String& id) {
    for (size_t i = 0; i < wifiManagerRefCount; i++) {
        WiFiManagerParameterRef& ref = wifiManagerRefs[i];
        if (ref.parameterId == id) {
            return ref.parameter;
        }
    }
    return nullptr;
}

String DeviceFrameworkParameterRegistry::buildDefaultEntityId(const char* domain, const String& parameterId) const {
    String slug = parameterId;
    slug.toLowerCase();

    String entityId = String(domain) + "." + slug;
    return entityId;
}

namespace {

const String& effectiveIcon(const DeviceFrameworkParameterMetadata* meta)
{
    if (meta->haEntityCommon.icon.length() > 0) {
        return meta->haEntityCommon.icon;
    }

    return meta->haIcon;
}

const String& effectiveDeviceClass(const DeviceFrameworkParameterMetadata* meta)
{
    if (meta->haEntityCommon.deviceClass.length() > 0) {
        return meta->haEntityCommon.deviceClass;
    }

    return meta->haDeviceClass;
}

void parseAvailabilityTopics(HABaseDeviceType* entity, const String& topics)
{
    if (!entity || topics.length() == 0) {
        return;
    }

    int start = 0;
    for (;;) {
        const int sep = topics.indexOf(';', start);
        const String t = (sep < 0) ? topics.substring(start) : topics.substring(start, sep);
        String topicCopy = t;
        topicCopy.trim();
        if (topicCopy.length() > 0) {
            entity->addAvailabilityEntry(topicCopy.c_str());
        }
        if (sep < 0) {
            break;
        }
        start = sep + 1;
    }
}

void applyAvailabilityToEntity(
    HABaseDeviceType* entity,
    const DeviceFrameworkParameterMetadata* meta
)
{
    if (!entity || !meta) {
        return;
    }

    if (meta->haAvailability.payloadAvailable.length() > 0) {
        entity->setPayloadAvailable(meta->haAvailability.payloadAvailable.c_str());
    }
    if (meta->haAvailability.payloadNotAvailable.length() > 0) {
        entity->setPayloadNotAvailable(meta->haAvailability.payloadNotAvailable.c_str());
    }
    if (meta->haAvailability.availabilityMode.length() > 0) {
        entity->setAvailabilityMode(meta->haAvailability.availabilityMode.c_str());
    }
    parseAvailabilityTopics(entity, meta->haAvailability.availabilityTopics);
}

void applyEntityCommon(
    HABaseDeviceType* entity,
    const DeviceFrameworkParameterMetadata* meta
)
{
    if (!entity || !meta) {
        return;
    }

    if (meta->haEntityCommon.hasEnabledByDefault) {
        entity->setEnabledByDefault(meta->haEntityCommon.enabledByDefault);
    }
    if (meta->haEntityCommon.entityPicture.length() > 0) {
        entity->setEntityPicture(meta->haEntityCommon.entityPicture.c_str());
    }
    if (meta->haEntityCommon.hasQos) {
        entity->setQos(meta->haEntityCommon.qos);
    }
    if (meta->haEntityCommon.encoding.length() > 0) {
        entity->setEncoding(meta->haEntityCommon.encoding.c_str());
    }
    if (meta->haEntityCommon.entityCategory.length() > 0) {
        entity->setEntityCategory(meta->haEntityCommon.entityCategory.c_str());
    }
    applyAvailabilityToEntity(entity, meta);
}

} // namespace

// Home Assistant integration
void DeviceFrameworkParameterRegistry::createHADevices(HAMqtt& mqtt) {
    String haDeviceList = "";

    // Get only Home Assistant parameters in sorted order
    auto paramIds = getParameterIdsSorted(DeviceFrameworkParameterSource::SOURCE_HOME_ASSISTANT);

    if (paramIds.count == 0) {
        return;
    }

    HADevice& haDevice = DeviceFrameworkMQTT::getHADevice();
    bool hasModelId = false;
    bool hasHardwareVersion = false;
    bool hasSerialNumber = false;
    bool hasSuggestedArea = false;
    bool hasViaDevice = false;
    bool hasConnections = false;
    bool hasSupportUrl = false;

    for (size_t i = 0; i < paramIds.count; i++) {
        const DeviceFrameworkParameterMetadata* meta = getMetadata(paramIds.ids[i]);
        if (!meta) {
            continue;
        }

        if (!hasModelId && meta->haIntegrationDevice.modelId.length() > 0) {
            haDevice.setModelId(meta->haIntegrationDevice.modelId.c_str());
            hasModelId = true;
        }
        if (!hasHardwareVersion && meta->haIntegrationDevice.hardwareVersion.length() > 0) {
            haDevice.setHardwareVersion(meta->haIntegrationDevice.hardwareVersion.c_str());
            hasHardwareVersion = true;
        }
        if (!hasSerialNumber && meta->haIntegrationDevice.serialNumber.length() > 0) {
            haDevice.setSerialNumber(meta->haIntegrationDevice.serialNumber.c_str());
            hasSerialNumber = true;
        }
        if (!hasSuggestedArea && meta->haIntegrationDevice.suggestedArea.length() > 0) {
            haDevice.setSuggestedArea(meta->haIntegrationDevice.suggestedArea.c_str());
            hasSuggestedArea = true;
        }
        if (!hasViaDevice && meta->haIntegrationDevice.viaDevice.length() > 0) {
            haDevice.setViaDevice(meta->haIntegrationDevice.viaDevice.c_str());
            hasViaDevice = true;
        }
        if (!hasConnections && meta->haIntegrationDevice.connectionsJson.length() > 0) {
            haDevice.setConnectionsJson(meta->haIntegrationDevice.connectionsJson.c_str());
            hasConnections = true;
        }
        if (!hasSupportUrl && meta->haIntegrationDevice.supportUrl.length() > 0) {
            mqtt.setOriginSupportUrl(meta->haIntegrationDevice.supportUrl.c_str());
            hasSupportUrl = true;
        }
    }

    // Allocate fixed-size array for HA device references
    haDeviceRefs = new HADeviceRef[paramIds.count];
    haDeviceRefCount = paramIds.count;

    size_t deviceIndex = 0;
    for (size_t i = 0; i < paramIds.count; i++) {
        const String& id = paramIds.ids[i];
        const DeviceFrameworkParameterMetadata* meta = getMetadata(id);
        if (!meta) continue;

        // Skip if no HA device type configured
        if (meta->haDeviceType == HAConfigDeviceType::NONE) {
            continue;
        }

        String currentValue = getValue(id);

        // Set up the reference in the fixed array
        haDeviceRefs[deviceIndex].parameterId = id;
        haDeviceRefs[deviceIndex].deviceType = meta->haDeviceType;

        switch (haDeviceRefs[deviceIndex].deviceType) {
            case HAConfigDeviceType::NUMBER: {
                // Create HANumber with precision
                // Map precision value to enum
                HANumber::NumberPrecision precision = HANumber::PrecisionP0;
                if (meta->haConstraints.precision == 1) precision = HANumber::PrecisionP1;
                else if (meta->haConstraints.precision == 2) precision = HANumber::PrecisionP2;
                else if (meta->haConstraints.precision == 3) precision = HANumber::PrecisionP3;

                HANumber* number = new HANumber(haDeviceRefs[deviceIndex].parameterId.c_str(), precision);

                number->setName(meta->label.c_str());
                haDeviceRefs[deviceIndex].defaultEntityId =
                    buildDefaultEntityId("number", haDeviceRefs[deviceIndex].parameterId);
                number->setDefaultEntityId(haDeviceRefs[deviceIndex].defaultEntityId.c_str());
                number->setMin(meta->haConstraints.minValue);
                number->setMax(meta->haConstraints.maxValue);
                number->setStep(meta->haConstraints.step);

                applyEntityCommon(number, meta);

                if (meta->haUnitOfMeasurement.length() > 0) {
                    number->setUnitOfMeasurement(meta->haUnitOfMeasurement.c_str());
                }
                {
                    const String& ic = effectiveIcon(meta);
                    if (ic.length() > 0) {
                        number->setIcon(ic.c_str());
                    }
                }
                {
                    const String& dc = effectiveDeviceClass(meta);
                    if (dc.length() > 0) {
                        number->setDeviceClass(dc.c_str());
                    }
                }

                if (meta->haTemplates.valueTemplate.length() > 0) {
                    number->setValueTemplate(meta->haTemplates.valueTemplate.c_str());
                }
                if (meta->haTemplates.commandTemplate.length() > 0) {
                    number->setCommandTemplate(meta->haTemplates.commandTemplate.c_str());
                }
                if (meta->haTemplates.payloadReset.length() > 0) {
                    number->setPayloadReset(meta->haTemplates.payloadReset.c_str());
                }

                // Set mode if specified (default is -1 for Auto, 1=Box, 2=Slider)
                if (meta->haConstraints.numberMode > 0) {
                    number->setMode(static_cast<HANumber::Mode>(meta->haConstraints.numberMode));
                }

                // Don't set state here - let onMqttConnected handle initial state publish
                // Setting state before MQTT is connected will fail

                // Map device pointer to parameter ID for static callback
                haDeviceToParamId[number] = haDeviceRefs[deviceIndex].parameterId;

                // Set up callback to update registry when HA changes value
                number->onCommand(onHANumberCommand);

                // Register with MQTT
                mqtt.addDeviceType(number);

                haDeviceRefs[deviceIndex].device = number;
                break;
            }

            case HAConfigDeviceType::SWITCH: {
                HASwitch* switchDevice = new HASwitch(haDeviceRefs[deviceIndex].parameterId.c_str());

                switchDevice->setName(meta->label.c_str());
                haDeviceRefs[deviceIndex].defaultEntityId =
                    buildDefaultEntityId("switch", haDeviceRefs[deviceIndex].parameterId);
                switchDevice->setDefaultEntityId(haDeviceRefs[deviceIndex].defaultEntityId.c_str());

                applyEntityCommon(switchDevice, meta);
                {
                    const String& ic = effectiveIcon(meta);
                    if (ic.length() > 0) {
                        switchDevice->setIcon(ic.c_str());
                    }
                }
                {
                    const String& dc = effectiveDeviceClass(meta);
                    if (dc.length() > 0) {
                        switchDevice->setDeviceClass(dc.c_str());
                    }
                }
                if (meta->haTemplates.valueTemplate.length() > 0) {
                    switchDevice->setValueTemplate(meta->haTemplates.valueTemplate.c_str());
                }
                if (meta->haTemplates.commandTemplate.length() > 0) {
                    switchDevice->setCommandTemplate(meta->haTemplates.commandTemplate.c_str());
                }

                // Don't set state here - let onMqttConnected handle initial state publish
                // Setting state before MQTT is connected will fail

                // Map device pointer to parameter ID for static callback
                haDeviceToParamId[switchDevice] = haDeviceRefs[deviceIndex].parameterId;

                // Set up callback to update registry when HA changes value
                switchDevice->onCommand(onHASwitchCommand);

                // Register with MQTT
                mqtt.addDeviceType(switchDevice);

                haDeviceRefs[deviceIndex].device = switchDevice;
                break;
            }

            case HAConfigDeviceType::SELECT: {
                HASelect* select = new HASelect(haDeviceRefs[deviceIndex].parameterId.c_str());

                select->setName(meta->label.c_str());
                haDeviceRefs[deviceIndex].defaultEntityId =
                    buildDefaultEntityId("select", haDeviceRefs[deviceIndex].parameterId);
                select->setDefaultEntityId(haDeviceRefs[deviceIndex].defaultEntityId.c_str());

                applyEntityCommon(select, meta);
                {
                    const String& ic = effectiveIcon(meta);
                    if (ic.length() > 0) {
                        select->setIcon(ic.c_str());
                    }
                }
                if (meta->haTemplates.valueTemplate.length() > 0) {
                    select->setValueTemplate(meta->haTemplates.valueTemplate.c_str());
                }
                if (meta->haTemplates.commandTemplate.length() > 0) {
                    select->setCommandTemplate(meta->haTemplates.commandTemplate.c_str());
                }

                // Set options (semicolon-separated for HASelect)
                select->setOptions(meta->haConstraints.options.c_str());

                // Don't set state here - let onMqttConnected handle initial state publish
                // Setting state before MQTT is connected will fail

                // Map device pointer to parameter ID for static callback
                haDeviceToParamId[select] = haDeviceRefs[deviceIndex].parameterId;

                // Set up callback to update registry when HA changes value
                select->onCommand(onHASelectCommand);

                // Register with MQTT
                mqtt.addDeviceType(select);

                haDeviceRefs[deviceIndex].device = select;
                break;
            }

            case HAConfigDeviceType::TEXT: {
                HAText* text = new HAText(haDeviceRefs[deviceIndex].parameterId.c_str());

                text->setName(meta->label.c_str());
                haDeviceRefs[deviceIndex].defaultEntityId =
                    buildDefaultEntityId("text", haDeviceRefs[deviceIndex].parameterId);
                text->setDefaultEntityId(haDeviceRefs[deviceIndex].defaultEntityId.c_str());

                applyEntityCommon(text, meta);
                {
                    const String& ic = effectiveIcon(meta);
                    if (ic.length() > 0) {
                        text->setIcon(ic.c_str());
                    }
                }
                if (meta->haTemplates.valueTemplate.length() > 0) {
                    text->setValueTemplate(meta->haTemplates.valueTemplate.c_str());
                }
                if (meta->haTemplates.commandTemplate.length() > 0) {
                    text->setCommandTemplate(meta->haTemplates.commandTemplate.c_str());
                }

                // Optional length hints for text controls.
                if (meta->haConstraints.textMinLength > 0) {
                    text->setMin(meta->haConstraints.textMinLength);
                }
                if (meta->haConstraints.textMaxLength > 0) {
                    text->setMax(meta->haConstraints.textMaxLength);
                }

                // Map device pointer to parameter ID for static callback
                haDeviceToParamId[text] = haDeviceRefs[deviceIndex].parameterId;

                // Set up callback to update registry when HA changes value
                text->onCommand(onHATextCommand);

                // Register with MQTT
                mqtt.addDeviceType(text);

                haDeviceRefs[deviceIndex].device = text;
                break;
            }

            default:
                LOG_WARN_SP(F("Unsupported HA device type for parameter: "), true);
                LOG_WARNLN_SP(haDeviceRefs[deviceIndex].parameterId, false);
                // Skip this device since we're not using it
                continue;
        }

        // Build parameter list for logging
        if (haDeviceList.length() > 0) haDeviceList += ", ";
        haDeviceList += haDeviceRefs[deviceIndex].parameterId;

        deviceIndex++;
    }

    if (deviceIndex > 0) {
        LOG_DEBUG_SP(F("Created "), true);
        LOG_DEBUG_SP(String(deviceIndex), false);
        LOG_DEBUG_SP(F(" HA device parameter(s): "), false);
        LOG_DEBUGLN_SP(haDeviceList, false);
    }
}

void DeviceFrameworkParameterRegistry::syncToHA(const String& id, bool forcePublish) {
    // When forcePublish is requested we still need to run the entity setter so
    // the local ArduinoHA shadow state is updated, even if transport is down.
    if (!mqttReady && !forcePublish) {
        LOG_DEBUG_SP(F("Skipping HA sync for '"), true);
        LOG_DEBUG_SP(id, false);
        LOG_DEBUGLN_SP(F("' - MQTT not ready"), false);
        return;
    }

    // Find the HA device for this ID and update its state
    for (size_t i = 0; i < haDeviceRefCount; i++) {
        HADeviceRef& ref = haDeviceRefs[i];
        if (ref.parameterId == id && ref.device != nullptr) {
            const DeviceFrameworkParameterMetadata* meta = getMetadata(id);
            if (meta == nullptr) continue;

            String value = getValue(id);

            switch (ref.deviceType) {
                case HAConfigDeviceType::NUMBER: {
                    HANumber* number = static_cast<HANumber*>(ref.device);
                    number->setState(HANumeric(value.toFloat(), meta->haConstraints.precision), forcePublish);
                    LOG_DEBUG_SP(F("Synced to HANumber: "), true);
                    LOG_DEBUG_SP(id, false);
                    LOG_DEBUG_SP(F(" = "), false);
                    LOG_DEBUGLN_SP(value, false);
                    break;
                }
                case HAConfigDeviceType::SWITCH: {
                    HASwitch* switchDevice = static_cast<HASwitch*>(ref.device);
                    switchDevice->setState(getValueAsBool(id), forcePublish);
                    LOG_DEBUG_SP(F("Synced to HASwitch: "), true);
                    LOG_DEBUG_SP(id, false);
                    LOG_DEBUG_SP(F(" = "), false);
                    LOG_DEBUGLN_SP(value, false);
                    break;
                }
                case HAConfigDeviceType::SELECT: {
                    HASelect* select = static_cast<HASelect*>(ref.device);

                    // Convert string value to index for HASelect
                    String options = meta->haConstraints.options;
                    String currentValue = value;
                    int8_t index = -1;

                    // Parse options and find matching index
                    int start = 0;
                    size_t end = 0;
                    int optionIndex = 0;

                    while (end < options.length()) {
                        // Find next semicolon or end of string
                        while (end < options.length() && options.charAt(end) != ';') {
                            end++;
                        }

                        String option = options.substring(start, end);
                        option.trim(); // Remove whitespace

                        if (option.length() > 0) {
                            if (currentValue == option) {
                                index = optionIndex;
                                break;
                            }
                            optionIndex++;
                        }

                        // Move to next option
                        start = end + 1;
                        end = start;
                    }

                    if (index >= 0) {
                        select->setState(index, forcePublish);
                        LOG_DEBUG_SP(F("Synced to HASelect: "), true);
                        LOG_DEBUG_SP(id, false);
                        LOG_DEBUG_SP(F(" = "), false);
                        LOG_DEBUG_SP(value, false);
                        LOG_DEBUG_SP(F(" (index: "), false);
                        LOG_DEBUG_SP(String(index), false);
                        LOG_DEBUGLN_SP(F(")"), false);
                    } else {
                        LOG_WARN_SP(F("HASelect value not found in options: "), true);
                        LOG_WARN_SP(id, false);
                        LOG_WARN_SP(F(" = "), false);
                        LOG_WARNLN_SP(value, false);
                        // Set to first option as fallback
                        select->setState(0, forcePublish);
                    }
                    break;
                }
                case HAConfigDeviceType::TEXT: {
                    HAText* text = static_cast<HAText*>(ref.device);
                    const char* currentValue = getValueAsCStr(id);
                    text->setState(currentValue, forcePublish);
                    LOG_DEBUG_SP(F("Synced to HAText: "), true);
                    LOG_DEBUG_SP(id, false);
                    LOG_DEBUG_SP(F(" = "), false);
                    LOG_DEBUGLN_SP(currentValue, false);
                    break;
                }
                default:
                    break;
            }
            break;
        }
    }
}

void DeviceFrameworkParameterRegistry::syncAllToHA() {
    for (size_t i = 0; i < haDeviceRefCount; i++) {
        syncToHA(haDeviceRefs[i].parameterId);
    }
}

void DeviceFrameworkParameterRegistry::scheduleFullHASync() {
    if (!mqttReady || haDeviceRefCount == 0) {
        return;
    }

    haResyncPending = true;
    haResyncNextIndex = 0;
    lastHAResyncAt = 0;

    LOG_DEBUGLN(F("MQTT became ready - scheduling paced HA parameter resync"));
    LOG_DEBUG_SP(F("Pending HA resync count: "), true);
    LOG_DEBUGLN_SP(String(haDeviceRefCount), false);
}

void DeviceFrameworkParameterRegistry::cancelPendingHASync() {
    haResyncPending = false;
    haResyncNextIndex = 0;
    lastHAResyncAt = 0;
}

bool DeviceFrameworkParameterRegistry::processPendingHASync(unsigned long now) {
    if (!mqttReady || !haResyncPending) {
        return false;
    }

    if (haResyncNextIndex >= haDeviceRefCount) {
        haResyncPending = false;
        LOG_DEBUGLN(F("Paced HA parameter resync complete"));
        return false;
    }

    const uint32_t interval = getConfigMQTTHAResyncInterval();
    if (interval > 0 && lastHAResyncAt > 0 && (now - lastHAResyncAt) < interval) {
        return false;
    }

    const String parameterId = haDeviceRefs[haResyncNextIndex].parameterId;
    haResyncNextIndex++;
    lastHAResyncAt = now;

    LOG_DEBUG_SP(F("Paced HA resync: "), true);
    LOG_DEBUG_SP(parameterId, false);
    LOG_DEBUG_SP(F(" ("), false);
    LOG_DEBUG_SP(String(haResyncNextIndex), false);
    LOG_DEBUG_SP(F("/"), false);
    LOG_DEBUG_SP(String(haDeviceRefCount), false);
    LOG_DEBUGLN_SP(F(")"), false);

    syncToHA(parameterId);

    if (haResyncNextIndex >= haDeviceRefCount) {
        haResyncPending = false;
        LOG_DEBUGLN(F("Paced HA parameter resync complete"));
    }

    return true;
}

bool DeviceFrameworkParameterRegistry::hasPendingHASync() const {
    return haResyncPending;
}

// Persistence
void DeviceFrameworkParameterRegistry::loadFromStorage() {
    DeviceFrameworkStorage::load();
}

void DeviceFrameworkParameterRegistry::saveToStorage() {
    DeviceFrameworkStorage::save();
}

// Callback registration
void DeviceFrameworkParameterRegistry::setChangeCallback(ParameterChangeCallback callback) {
    changeCallback = callback;
}

void DeviceFrameworkParameterRegistry::setMqttReady(bool ready) {
    mqttReady = ready;

    LOG_DEBUG_SP(F("setMqttReady called: ready="), true);
    LOG_DEBUGLN_SP(String(ready), false);

    if (ready) {
        scheduleFullHASync();
    } else {
        cancelPendingHASync();
    }
}

// Internal helpers
void DeviceFrameworkParameterRegistry::notifyValueChanged(const String& id, const String& oldValue, const String& newValue, DeviceFrameworkParameterUpdateOrigin origin) {
    if (oldValue == newValue) {
        return;  // No actual change
    }

    // Handle special cases for specific parameters
    if (id == DeviceFrameworkParameters::PARAM_LOG_LEVEL) {
        // Special handling for log level parameter - apply the new log level immediately
        applyLogLevel(newValue.c_str());
        LOG_DEBUG_SP(F("Log level parameter changed: "), true);
        LOG_DEBUG_SP(oldValue, false);
        LOG_DEBUG_SP(F(" -> "), false);
        LOG_DEBUGLN_SP(newValue, false);
    }

    // Sync to WiFiManager if this parameter has one
    const DeviceFrameworkParameterMetadata* meta = getMetadata(id);
    if (meta != nullptr && meta->isWiFiManagerParameter()) {
        syncToWiFiManager(id);
    }

    // Sync to HA if this parameter has an HA device
    if (meta != nullptr &&
        meta->isHomeAssistantParameter() &&
        origin != DeviceFrameworkParameterUpdateOrigin::HOME_ASSISTANT) {
        syncToHA(id);
    }

    // Call user callback
    if (changeCallback != nullptr) {
        changeCallback(id, oldValue, newValue);
    }
}

// Helper function to convert index to option string
String DeviceFrameworkParameterRegistry::indexToOptionString(const String& options, int8_t index) const {
    if (index < 0) return "";

    int start = 0;
    size_t end = 0;
    int optionIndex = 0;

    while (end < options.length()) {
        // Find next semicolon or end of string
        while (end < options.length() && options.charAt(end) != ';') {
            end++;
        }

        String option = options.substring(start, end);
        option.trim(); // Remove whitespace

        if (option.length() > 0) {
            if (optionIndex == index) {
                return option;
            }
            optionIndex++;
        }

        // Move to next option
        start = end + 1;
        end = start;
    }

    return ""; // Index not found
}

// Static callbacks for HA devices
void DeviceFrameworkParameterRegistry::onHANumberCommand(HANumeric value, HANumber* sender) {
    if (instance == nullptr) return;

    auto it = haDeviceToParamId.find(sender);
    if (it != haDeviceToParamId.end()) {
        const String& paramId = it->second;
        const DeviceFrameworkParameterMetadata* meta = instance->getMetadata(paramId);
        if (meta != nullptr) {
            String newValue = String(value.toFloat(), (unsigned int)meta->haConstraints.precision);
            instance->setValue(paramId, newValue, DeviceFrameworkParameterUpdateOrigin::HOME_ASSISTANT);
            instance->syncToHA(paramId, true);
            LOG_DEBUG_SP(F("HA Number updated: "), true);
            LOG_DEBUG_SP(paramId, false);
            LOG_DEBUG_SP(F(" = "), false);
            LOG_DEBUGLN_SP(newValue, false);

            // Save to EEPROM so change persists across reboots
            instance->saveToStorage();
        }
    }
}

void DeviceFrameworkParameterRegistry::onHASwitchCommand(bool state, HASwitch* sender) {
    if (instance == nullptr) return;

    auto it = haDeviceToParamId.find(sender);
    if (it != haDeviceToParamId.end()) {
        const String& paramId = it->second;
        instance->setValue(paramId, state, DeviceFrameworkParameterUpdateOrigin::HOME_ASSISTANT);
        instance->syncToHA(paramId, true);
        LOG_DEBUG_SP(F("HA Switch updated: "), true);
        LOG_DEBUG_SP(paramId, false);
        LOG_DEBUG_SP(F(" = "), false);
        LOG_DEBUGLN_SP(String(state ? "ON" : "OFF"), false);

        // Save to EEPROM so change persists across reboots
        instance->saveToStorage();
    }
}

void DeviceFrameworkParameterRegistry::onHASelectCommand(int8_t index, HASelect* sender) {
    if (instance == nullptr) return;

    auto it = haDeviceToParamId.find(sender);
    if (it != haDeviceToParamId.end()) {
        const String& paramId = it->second;
        const DeviceFrameworkParameterMetadata* meta = instance->getMetadata(paramId);

        if (meta != nullptr) {
            // Convert index to option string
            String optionString = instance->indexToOptionString(meta->haConstraints.options, index);
            if (optionString.length() > 0) {
                instance->setValue(paramId, optionString, DeviceFrameworkParameterUpdateOrigin::HOME_ASSISTANT);
                instance->syncToHA(paramId, true);
                LOG_DEBUG_SP(F("HA Select updated: "), true);
                LOG_DEBUG_SP(paramId, false);
                LOG_DEBUG_SP(F(" = "), false);
                LOG_DEBUG_SP(optionString, false);
                LOG_DEBUG_SP(F(" (index: "), false);
                LOG_DEBUG_SP(String(index), false);
                LOG_DEBUGLN_SP(F(")"), false);

                // Save to EEPROM so change persists across reboots
                instance->saveToStorage();
            } else {
                LOG_WARN_SP(F("HA Select: Invalid index "), true);
                LOG_WARN_SP(String(index), false);
                LOG_WARN_SP(F(" for parameter "), false);
                LOG_WARNLN_SP(paramId, false);
            }
        }
    }
}

void DeviceFrameworkParameterRegistry::onHATextCommand(const char* value, HAText* sender) {
    if (instance == nullptr) return;

    auto it = haDeviceToParamId.find(sender);
    if (it != haDeviceToParamId.end()) {
        const String& paramId = it->second;
        instance->setValue(paramId, value ? value : "", DeviceFrameworkParameterUpdateOrigin::HOME_ASSISTANT);
        instance->syncToHA(paramId, true);
        LOG_DEBUG_SP(F("HA Text updated: "), true);
        LOG_DEBUG_SP(paramId, false);
        LOG_DEBUG_SP(F(" = "), false);
        LOG_DEBUGLN_SP(instance->getValue(paramId), false);

        // Save to EEPROM so change persists across reboots
        instance->saveToStorage();
    }
}

void* DeviceFrameworkParameterRegistry::getHADeviceForParameter(const String& id) {
    for (size_t i = 0; i < haDeviceRefCount; i++) {
        if (haDeviceRefs[i].parameterId == id) {
            return haDeviceRefs[i].device;
        }
    }

    return nullptr;
}

// Debug
void DeviceFrameworkParameterRegistry::printRegistry() const {
    LOG_DEBUGLN(F("=== Parameter Registry ==="));
    LOG_DEBUG_SP(F("Registered parameters: "), true);
    LOG_DEBUGLN_SP(String(parameterCount), false);

    for (size_t i = 0; i < parameterCount; i++) {
        const DeviceFrameworkParameterEntry& entry = parameters[i];
        String value = entry.value.asString();

        LOG_DEBUG_SP(F("  "), true);
        LOG_DEBUG_SP(entry.metadata.id, false);
        LOG_DEBUG_SP(F(" = \""), false);
        LOG_DEBUG_SP(valueForLog(entry.metadata.id, value), false);
        LOG_DEBUG_SP(F("\" (default: \""), false);
        LOG_DEBUG_SP(valueForLog(entry.metadata.id, entry.metadata.defaultValue), false);

        String sources = "";
        if (entry.metadata.isWiFiManagerParameter()) sources += "WiFiManager ";
        if (entry.metadata.isHomeAssistantParameter()) sources += "HA ";

        LOG_DEBUG_SP(F(" ["), false);
        LOG_DEBUG_SP(sources, false);
        LOG_DEBUGLN_SP(F("]"), false);
    }

    LOG_DEBUG_SP(F("WiFiManager parameters: "), true);
    LOG_DEBUGLN_SP(String(wifiManagerRefCount), false);
    LOG_DEBUG_SP(F("HA devices: "), true);
    LOG_DEBUGLN_SP(String(haDeviceRefCount), false);
}

// Array management helpers
DeviceFrameworkParameterEntry* DeviceFrameworkParameterRegistry::findParameter(const String& id) {
    if (parameters == nullptr) {
        return nullptr;
    }
    for (size_t i = 0; i < parameterCount; i++) {
        if (parameters[i].metadata.id == id) {
            return &parameters[i];
        }
    }
    return nullptr;
}

const DeviceFrameworkParameterEntry* DeviceFrameworkParameterRegistry::findParameter(const String& id) const {
    if (parameters == nullptr) {
        return nullptr;
    }
    for (size_t i = 0; i < parameterCount; i++) {
        if (parameters[i].metadata.id == id) {
            return &parameters[i];
        }
    }
    return nullptr;
}

bool DeviceFrameworkParameterRegistry::addParameter(const DeviceFrameworkParameterMetadata& meta) {
    // Ensure parameters array is allocated
    if (parameters == nullptr) {
        parameters = new DeviceFrameworkParameterEntry[CONFIG_maxParameters];
        if (parameters == nullptr) {
            LOG_ERRORLN("Failed to allocate parameters array");
            return false;
        }
    }

    // Check against configured maximum (user-configurable limit)
    if (parameterCount >= CONFIG_maxParameters) {
        LOG_ERROR_SP(F("Maximum parameter count reached: "), true);
        LOG_ERROR_SP(String(CONFIG_maxParameters), false);
        LOG_ERRORLN_SP(F(" (configured limit)"), false);
        return false;
    }

    // Add to end of array
    parameters[parameterCount] = DeviceFrameworkParameterEntry(meta);
    parameterCount++;

    // Don't sort here - keep insertion order for stability
    // Sorting will be done on-the-fly in getParameterIdsSorted()

    return true;
}

void DeviceFrameworkParameterRegistry::reallocateParameters() {
    // If we have existing parameters, we need to preserve them
    if (parameters != nullptr && parameterCount > 0) {
        // Create temporary array to hold existing parameters
        DeviceFrameworkParameterEntry* temp = new DeviceFrameworkParameterEntry[parameterCount];
        if (temp == nullptr) {
            LOG_ERRORLN("Failed to allocate temporary array for reallocation");
            return;
        }

        // Copy existing parameters to temporary array
        for (size_t i = 0; i < parameterCount; i++) {
            temp[i] = parameters[i];
        }

        // Delete old array
        delete[] parameters;

        // Allocate new array with new size
        parameters = new DeviceFrameworkParameterEntry[CONFIG_maxParameters];
        if (parameters == nullptr) {
            LOG_ERRORLN("Failed to allocate new parameters array");
            // Restore from temp array
            parameters = temp;
            return;
        }

        // Copy parameters back (up to the new limit)
        size_t copyCount = (parameterCount > CONFIG_maxParameters) ? CONFIG_maxParameters : parameterCount;
        for (size_t i = 0; i < copyCount; i++) {
            parameters[i] = temp[i];
        }

        // Update parameter count if we had to truncate
        if (parameterCount > CONFIG_maxParameters) {
            LOG_WARN_SP(F("Parameter count truncated from "), true);
            LOG_WARN_SP(String(parameterCount), false);
            LOG_WARN_SP(F(" to "), false);
            LOG_WARNLN_SP(String(CONFIG_maxParameters), false);
            parameterCount = CONFIG_maxParameters;
        }

        // Clean up temporary array
        delete[] temp;
    } else {
        // No existing parameters, just allocate new array
        if (parameters != nullptr) {
            delete[] parameters;
        }
        parameters = new DeviceFrameworkParameterEntry[CONFIG_maxParameters];
        if (parameters == nullptr) {
            LOG_ERRORLN("Failed to allocate parameters array");
        }
    }
}
