#ifndef MDNS_MANAGER_H
#define MDNS_MANAGER_H

#include <Arduino.h>
#include "../DeviceFrameworkDebug.h"
#include <DeviceFrameworkPlatform.h>  // Platform abstraction

#ifdef DF_PLATFORM_ESP8266
    // ESP8266 uses mDNSResolver library
    #include <WiFiUdp.h>
    #include <mDNSResolver.h>
#endif

class DeviceFrameworkMDNS {
private:
    #ifdef DF_PLATFORM_ESP8266
        // ESP8266: Use mDNSResolver library (ESP8266 MDNS doesn't have native resolution)
        static WiFiUDP udp;
        static mDNSResolver::Resolver resolver;
    #endif
    // ESP32: Uses native ESPmDNS which handles resolution internally - no resolver needed
    static IPAddress currentResolverIP;
    static bool initialized;

    // Non-blocking resolution state
    static IPAddress cachedIP;
    static String lastResolvedHostname;
    static unsigned long lastResolutionAttempt;
    static bool isResolving;

    // Packet draining state (to avoid calling resolver.loop() every iteration)
    static unsigned long lastPacketDrainTime;

    // Internal methods
    static void updateResolverIP();

public:
    // Initialization
    static void setup(const char* hostname);
    static void loop();

    // mDNS resolution
    static IPAddress resolve(const char* hostname);
    static bool isResolvable(const char* hostname);
    static bool resolveCached(const char* hostname, IPAddress& result);

    // Utility methods
    static void addService(const char* service, const char* protocol, uint16_t port, const char* txt = nullptr);
    static void removeService(const char* service, const char* protocol);

    // Status
    static bool isInitialized();
};

#endif // MDNS_MANAGER_H
