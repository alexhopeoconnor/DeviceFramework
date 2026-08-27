#include "DeviceFrameworkMDNS.h"
#include "../Utils/TimeUtils.h"
#include <DeviceFrameworkPlatform.h>  // Platform abstraction (includes MDNS headers)
#include "../Configuration/DeviceFrameworkConfig.h"

// Static member definitions
#ifdef DF_PLATFORM_ESP8266
    // ESP8266: Use mDNSResolver library
    WiFiUDP DeviceFrameworkMDNS::udp;
    mDNSResolver::Resolver DeviceFrameworkMDNS::resolver(DeviceFrameworkMDNS::udp);
#endif
IPAddress DeviceFrameworkMDNS::currentResolverIP = INADDR_NONE;
bool DeviceFrameworkMDNS::initialized = false;

// Non-blocking resolution state
IPAddress DeviceFrameworkMDNS::cachedIP = INADDR_NONE;
String DeviceFrameworkMDNS::lastResolvedHostname = "";
String DeviceFrameworkMDNS::activeHostname = "";
unsigned long DeviceFrameworkMDNS::lastResolutionAttempt = 0;
bool DeviceFrameworkMDNS::isResolving = false;

// Packet draining state
unsigned long DeviceFrameworkMDNS::lastPacketDrainTime = 0;

namespace {

bool hasMDNSHeapHeadroom(uint32_t minimumFreeHeap) {
#ifdef DF_PLATFORM_ESP8266
    uint32_t freeHeap = 0;
    uint32_t largestFreeBlock = 0;
    uint8_t fragmentation = 0;
    DF_GET_HEAP_STATS(freeHeap, largestFreeBlock, fragmentation);
    return freeHeap >= minimumFreeHeap &&
           largestFreeBlock >= getConfigMDNSMinLargestBlock();
#else
    return ESP.getFreeHeap() >= minimumFreeHeap;
#endif
}

} // namespace

void DeviceFrameworkMDNS::setup(const char* hostname) {
    if (initialized) {
        LOG_DEBUGLN(F("MDNSManager already initialized"));
        return;
    }

    if (!hostname || strlen(hostname) == 0) {
        LOG_ERRORLN(F("MDNSManager setup failed: hostname is null or empty"));
        return;
    }

    // Start mDNS responder
    if (!MDNS.begin(hostname)) {
        LOG_ERRORLN(F("Error setting up MDNS responder!"));
        return;
    }

    LOG_INFO_SP(F("mDNS responder started with hostname: "), true);
    LOG_INFOLN_SP(String(hostname), false);

    // Initialize mDNSResolver with the current IP
    #ifdef DF_PLATFORM_ESP32
        // On ESP32, don't initialize the resolver - it causes UDP socket conflicts
        // ESP32's native MDNS can resolve .local domains directly via MDNS.queryHost()
        currentResolverIP = WiFi.localIP();
    #else
        currentResolverIP = WiFi.localIP();
        resolver.setLocalIP(currentResolverIP);
    #endif

    activeHostname = hostname;
    initialized = true;
    LOG_INFOLN(F("MDNSManager initialized successfully"));
}

void DeviceFrameworkMDNS::onNetworkReady(const char* hostname) {
    if (!hostname || hostname[0] == 0 || WiFi.status() != WL_CONNECTED ||
        WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
        return;
    }

    if (initialized && currentResolverIP == WiFi.localIP() && activeHostname == hostname) {
        return;
    }
    if (initialized) onNetworkLost();
    setup(hostname);
}

void DeviceFrameworkMDNS::onNetworkLost() {
    if (!initialized && currentResolverIP == INADDR_NONE) return;

    if (initialized) {
        #ifdef DF_PLATFORM_ESP8266
            MDNS.close();
        #elif defined(DF_PLATFORM_ESP32)
            MDNS.end();
        #endif
    }

    initialized = false;
    currentResolverIP = INADDR_NONE;
    cachedIP = INADDR_NONE;
    activeHostname = "";
    lastResolvedHostname = "";
    lastResolutionAttempt = 0;
    lastPacketDrainTime = 0;
    isResolving = false;
}

void DeviceFrameworkMDNS::loop() {
    if (!initialized) {
        return;
    }

    // Update resolver's local IP if it changes
    #ifndef DF_PLATFORM_ESP32
        // On ESP32, skip resolver IP updates - resolver is not used
        updateResolverIP();
    #endif

    // Process MDNS (responds to queries, processes announcements)
    #ifdef DF_PLATFORM_ESP8266
        // ESP8266: MDNS.update() parses multicast responses using dynamic allocations
        // Require total and contiguous heap headroom before entering the core parser
        if (hasMDNSHeapHeadroom(getConfigMDNSMinFreeHeap())) {
            MDNS.update();
        }
        // If memory is low, skip MDNS processing - packets will stay in UDP buffer temporarily
    #elif defined(DF_PLATFORM_ESP32)
        // ESP32: MDNS is handled automatically, no update() call needed
        // MDNS updates happen automatically in the background
    #endif

    // Drain resolver packets periodically instead of every loop iteration
    // resolver.loop() allocates memory just to throw packets away, so calling it
    // every loop is wasteful. We only need to drain packets occasionally to prevent
    // UDP buffer buildup, not every single loop iteration.
    if (TimeUtils::hasTimeElapsed(millis(), lastPacketDrainTime, getConfigMDNSPacketDrainInterval())) {
        // Only drain when heap is sufficient and contiguous (resolver.loop() allocates dynamically)
        if (hasMDNSHeapHeadroom(getConfigMDNSPacketDrainMinFreeHeap())) {
            #ifdef DF_PLATFORM_ESP32
                // On ESP32, skip resolver.loop() - it causes UDP socket binding conflicts
                // ESP32's MDNS handles resolution automatically without needing resolver packet draining
                // The resolver UDP socket conflicts with MDNS.begin()'s internal UDP socket
                // Skip packet draining on ESP32 to avoid crashes
            #else
                resolver.loop(); // Drain accumulated packets
            #endif
            lastPacketDrainTime = millis();
        }
        // If memory is low, skip draining - packets will stay in UDP buffer temporarily
        // This prevents OOM crashes during heavy operations like HTTP template processing
    }

    // Handle non-blocking DNS resolution timeout
    if (isResolving && TimeUtils::hasTimeElapsed(millis(), lastResolutionAttempt, getConfigDNSResolutionTimeout())) {
        LOG_WARNLN(F("DNS resolution timeout, will retry later"));
        isResolving = false;
    }
}

IPAddress DeviceFrameworkMDNS::resolve(const char* hostname) {
    if (!initialized) {
        LOG_ERRORLN(F("MDNSManager not initialized"));
        return INADDR_NONE;
    }

    if (!hostname || strlen(hostname) == 0) {
        LOG_ERRORLN(F("MDNSManager resolve failed: hostname is null or empty"));
        return INADDR_NONE;
    }

    String hostnameStr(hostname);
    LOG_DEBUG_SP(F("Resolving hostname: "), true);
    LOG_DEBUGLN_SP(hostnameStr, false);

    IPAddress result = INADDR_NONE;

    if (hostnameStr.endsWith(".local")) {
        #ifdef DF_PLATFORM_ESP32
            // ESP32: Use native MDNS.queryHost() for .local domains
            LOG_DEBUG_SP(F("Resolving via ESP32 MDNS.queryHost: "), true);
            LOG_DEBUGLN_SP(hostnameStr, false);

            // Remove .local suffix for ESP32's queryHost
            String queryHost = hostnameStr;
            queryHost.replace(".local", "");

            uint32_t timeout = getConfigDNSResolutionTimeout();
            unsigned long startTime = millis();

            // ESP32's queryHost is blocking but has timeout
            result = MDNS.queryHost(queryHost.c_str(), timeout);

            unsigned long elapsed = TimeUtils::safeTimeDifference(millis(), startTime);
            if (result == INADDR_NONE) {
                LOG_ERRORLN(F("Failed to resolve via ESP32 MDNS.queryHost"));
            } else {
                LOG_DEBUG_SP(F("Resolved via ESP32 MDNS.queryHost to: "), true);
                LOG_DEBUGLN_SP(result, false);
            }
        #else
            // ESP8266: Use mDNS resolver for .local domains with timeout protection
            LOG_DEBUG_SP(F("Resolving via mDNS: "), true);
            LOG_DEBUGLN_SP(hostnameStr, false);

            unsigned long startTime = millis();
            uint32_t timeout = getConfigDNSResolutionTimeout();

            // Try mDNS resolution with timeout
            result = resolver.search(hostnameStr.c_str());

            // Check if it took too long (resolver.search() is blocking, but we can check timing)
            unsigned long elapsed = TimeUtils::safeTimeDifference(millis(), startTime);
            if (elapsed > timeout) {
                LOG_WARN_SP(F("mDNS resolution took "), true);
                LOG_WARN_SP(String(elapsed), false);
                LOG_WARN_SP(F("ms, exceeding timeout of "), false);
                LOG_WARN_SP(String(timeout), false);
                LOG_WARNLN_SP(F("ms"), false);
                result = INADDR_NONE;
            } else if (result == INADDR_NONE) {
                LOG_ERRORLN(F("Failed to resolve via mDNS"));
            } else {
                LOG_DEBUG_SP(F("Resolved via mDNS to: "), true);
                LOG_DEBUGLN_SP(result, false);
            }
        #endif
    } else {
        // Use WiFi.hostByName for other domains
        #ifdef DF_PLATFORM_ESP8266
            // ESP8266: WiFi.hostByName supports timeout parameter
            uint32_t timeout = getConfigDNSResolutionTimeout();
            if (!WiFi.hostByName(hostnameStr.c_str(), result, timeout)) {
                LOG_ERROR_SP(F("Failed to resolve hostname using WiFi.hostByName (timeout after "), true);
                LOG_ERROR_SP(String(timeout), false);
                LOG_ERRORLN_SP(F("ms)"), false);
                result = INADDR_NONE;
            } else {
                LOG_DEBUG_SP(F("Resolved via WiFi.hostByName to: "), true);
                LOG_DEBUGLN_SP(result, false);
            }
        #elif defined(DF_PLATFORM_ESP32)
            // ESP32: WiFi.hostByName doesn't support timeout parameter (2 params only)
            // Note: WiFi.hostByName is blocking and cannot be interrupted
            // Unlike ESP8266, there's no way to enforce a timeout on ESP32
            if (!WiFi.hostByName(hostnameStr.c_str(), result)) {
                LOG_ERRORLN(F("Failed to resolve hostname using WiFi.hostByName"));
                result = INADDR_NONE;
            } else {
                LOG_DEBUG_SP(F("Resolved via WiFi.hostByName to: "), true);
                LOG_DEBUGLN_SP(result, false);
            }
        #endif
    }

    return result;
}

bool DeviceFrameworkMDNS::isResolvable(const char* hostname) {
    return resolve(hostname) != INADDR_NONE;
}

void DeviceFrameworkMDNS::addService(const char* service, const char* protocol, uint16_t port, const char* txt) {
    if (!initialized) {
        LOG_ERRORLN(F("MDNSManager not initialized"));
        return;
    }

    if (!service || !protocol) {
        LOG_ERRORLN(F("MDNSManager addService failed: service or protocol is null"));
        return;
    }

    #ifdef DF_PLATFORM_ESP8266
        // ESP8266: MDNS.addService(service, protocol, txt, port)
        if (txt) {
            MDNS.addService(service, protocol, txt, port);
            LOG_DEBUG_SP(F("Added mDNS service: "), true);
            LOG_DEBUG_SP(service, false);
            LOG_DEBUG_SP(F("."), false);
            LOG_DEBUG_SP(protocol, false);
            LOG_DEBUG_SP(F(" on port "), false);
            LOG_DEBUG_SP(port, false);
            LOG_DEBUG_SP(F(" with txt: "), false);
            LOG_DEBUGLN_SP(txt, false);
        } else {
            MDNS.addService(service, protocol, "", port);
            LOG_DEBUG_SP(F("Added mDNS service: "), true);
            LOG_DEBUG_SP(service, false);
            LOG_DEBUG_SP(F("."), false);
            LOG_DEBUG_SP(protocol, false);
            LOG_DEBUG_SP(F(" on port "), false);
            LOG_DEBUGLN_SP(port, false);
        }
    #elif defined(DF_PLATFORM_ESP32)
        // ESP32: MDNS.addService(service, protocol, port) - txt handled separately
        MDNS.addService(service, protocol, port);
        if (txt && strlen(txt) > 0) {
            // ESP32 requires setting txt records separately after adding service
            // Parse txt string (format: "key1=value1,key2=value2" or just "value")
            String txtStr(txt);
            int commaIndex = txtStr.indexOf(',');
            if (commaIndex > 0) {
                // Multiple key=value pairs - parse and add each
                int startPos = 0;
                while (startPos < txtStr.length()) {
                    int endPos = txtStr.indexOf(',', startPos);
                    if (endPos == -1) endPos = txtStr.length();
                    String pair = txtStr.substring(startPos, endPos);
                    pair.trim();
                    int eqIndex = pair.indexOf('=');
                    if (eqIndex > 0) {
                        String key = pair.substring(0, eqIndex);
                        String value = pair.substring(eqIndex + 1);
                        MDNS.addServiceTxt(service, protocol, key.c_str(), value.c_str());
                    } else {
                        // No '=' found, treat as single value
                        MDNS.addServiceTxt(service, protocol, "txt", pair.c_str());
                    }
                    startPos = endPos + 1;
                }
            } else {
                // Single txt record - check if it's key=value or just value
                int eqIndex = txtStr.indexOf('=');
                if (eqIndex > 0) {
                    String key = txtStr.substring(0, eqIndex);
                    String value = txtStr.substring(eqIndex + 1);
                    MDNS.addServiceTxt(service, protocol, key.c_str(), value.c_str());
                } else {
                    // No '=' found, treat as single value with default key
                    MDNS.addServiceTxt(service, protocol, "txt", txtStr.c_str());
                }
            }
            LOG_DEBUG_SP(F("Added mDNS service: "), true);
            LOG_DEBUG_SP(service, false);
            LOG_DEBUG_SP(F("."), false);
            LOG_DEBUG_SP(protocol, false);
            LOG_DEBUG_SP(F(" on port "), false);
            LOG_DEBUG_SP(port, false);
            LOG_DEBUG_SP(F(" with txt: "), false);
            LOG_DEBUGLN_SP(txt, false);
        } else {
            LOG_DEBUG_SP(F("Added mDNS service: "), true);
            LOG_DEBUG_SP(service, false);
            LOG_DEBUG_SP(F("."), false);
            LOG_DEBUG_SP(protocol, false);
            LOG_DEBUG_SP(F(" on port "), false);
            LOG_DEBUGLN_SP(port, false);
        }
    #endif
}

void DeviceFrameworkMDNS::removeService(const char* service, const char* protocol) {
    if (!initialized) {
        LOG_ERRORLN(F("MDNSManager not initialized"));
        return;
    }

    if (!service || !protocol) {
        LOG_ERRORLN(F("MDNSManager removeService failed: service or protocol is null"));
        return;
    }

    #ifdef DF_PLATFORM_ESP8266
        // ESP8266: MDNS.removeService(instance, service, protocol)
        MDNS.removeService("", service, protocol);
        LOG_DEBUG_SP(F("Removed mDNS service: "), true);
        LOG_DEBUG_SP(service, false);
        LOG_DEBUG_SP(F("."), false);
        LOG_DEBUGLN_SP(protocol, false);
    #elif defined(DF_PLATFORM_ESP32)
        // ESP32: MDNS library doesn't have removeService() method
        // Services are automatically cleaned up when MDNS is stopped/restarted
        // For now, we just log the attempt - actual removal would require restarting MDNS
        LOG_DEBUG_SP(F("MDNS removeService not supported on ESP32 (services auto-cleanup on MDNS restart): "), true);
        LOG_DEBUG_SP(service, false);
        LOG_DEBUG_SP(F("."), false);
        LOG_DEBUGLN_SP(protocol, false);
    #endif
}

bool DeviceFrameworkMDNS::isInitialized() {
    return initialized;
}

void DeviceFrameworkMDNS::updateResolverIP() {
    // Update resolver's local IP if it changes
    #ifndef DF_PLATFORM_ESP32
        // On ESP32, resolver is not used - skip UDP socket operations
        if (WiFi.localIP() != currentResolverIP) {
            currentResolverIP = WiFi.localIP();
            resolver.setLocalIP(currentResolverIP);
            LOG_DEBUGLN(F("Updated resolver local IP"));
        }
    #endif
}

bool DeviceFrameworkMDNS::resolveCached(const char* hostname, IPAddress& result) {
    if (!initialized) {
        LOG_ERRORLN(F("MDNSManager not initialized"));
        return false;
    }

    if (!hostname || strlen(hostname) == 0) {
        LOG_ERRORLN(F("MDNSManager resolveCached failed: hostname is null or empty"));
        return false;
    }

    String hostnameStr(hostname);

    // Check if we have a valid cached IP
    if (cachedIP != INADDR_NONE && lastResolvedHostname == hostnameStr) {
        // Check if cache is still valid
        if (!TimeUtils::hasTimeElapsed(millis(), lastResolutionAttempt, getConfigDNSCacheDuration())) {
            result = cachedIP;
            LOG_DEBUG_SP(F("Using cached IP for "), true);
            LOG_DEBUG_SP(hostnameStr, false);
            LOG_DEBUG_SP(F(": "), false);
            LOG_DEBUGLN_SP(result, false);
            return true;
        }
    }

    // If we're already resolving, don't start another resolution
    if (isResolving) {
        return false;
    }

    // Check if we should retry (not too frequent)
    if (!TimeUtils::hasTimeElapsed(millis(), lastResolutionAttempt, 5000)) { // Wait at least 5 seconds between attempts
        return false;
    }

    // Start new resolution attempt
    LOG_DEBUG_SP(F("Starting cached DNS resolution for: "), true);
    LOG_DEBUGLN_SP(hostnameStr, false);

    isResolving = true;
    lastResolutionAttempt = millis();
    lastResolvedHostname = hostnameStr;

    // Try to resolve (still blocking, but with caching to avoid repeated calls)
    IPAddress resolvedIP = resolve(hostnameStr.c_str());

    if (resolvedIP != INADDR_NONE) {
        cachedIP = resolvedIP;
        result = resolvedIP;
        isResolving = false;
        LOG_DEBUG_SP(F("Successfully resolved to: "), true);
        LOG_DEBUGLN_SP(resolvedIP, false);
        return true;
    } else {
        // Resolution failed, will retry later
        LOG_DEBUGLN(F("DNS resolution failed, will retry later"));
        return false;
    }
}
