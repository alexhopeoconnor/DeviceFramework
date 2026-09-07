#pragma once

// Board-free builds use these non-secret defaults. Physical HA E2E builds
// receive a private, session-scoped header through the compiler's -include
// flag. Keeping credentials outside the checkout lets parallel workers share
// one immutable configuration without racing on a generated source file.
#ifndef DEVICEFRAMEWORK_HA_E2E_CONFIG_INCLUDED
  #define TEST_WIFI_SSID ""
  #define TEST_WIFI_PASSWORD ""
  #define TEST_MQTT_SERVER ""
  #define TEST_MQTT_USER ""
  #define TEST_MQTT_PASSWORD ""
#endif
