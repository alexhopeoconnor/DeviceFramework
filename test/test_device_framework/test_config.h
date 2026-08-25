#pragma once

// Board-free builds use these non-secret defaults. The hardware runner creates
// the ignored test_config.generated.h from test/.env for the duration of a run.
#if defined(__has_include)
  #if __has_include("test_config.generated.h")
    #include "test_config.generated.h"
  #else
    #define DEVICEFRAMEWORK_TEST_CONFIG_DEFAULTS 1
  #endif
#else
  #define DEVICEFRAMEWORK_TEST_CONFIG_DEFAULTS 1
#endif

#ifdef DEVICEFRAMEWORK_TEST_CONFIG_DEFAULTS
  #define TEST_WIFI_SSID ""
  #define TEST_WIFI_PASSWORD ""
  #define TEST_MQTT_SERVER ""
  #define TEST_MQTT_USER ""
  #define TEST_MQTT_PASSWORD ""
#endif
