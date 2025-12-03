// Config_Fast.h - Minimal build configuration for fast compilation during development
// Use this during active development, switch back to Config.h for full builds
//
// To use: Change #include "Config.h" to #include "Config_Fast.h" in .ino file
//
// This disables most features for 2-3x faster compilation

#ifndef CONFIG_FAST_H
#define CONFIG_FAST_H

#include <Arduino.h>
#include <vector>
#include <cstdint>
#include <ctime>

// ========== Feature Enables (MINIMAL for fast compile) ==========
#define ENABLE_LORA 0          // Disabled - saves ~5 seconds
#define ENABLE_MODEM 1         // Enabled (testing SMS)
#define ENABLE_MQTT 0          // Disabled - saves ~3 seconds
#define ENABLE_BLE 0           // Disabled - saves ~4 seconds
#define ENABLE_WIFI 1          // Enabled (needed for testing)
#define ENABLE_HTTP 0          // Disabled - saves ~2 seconds
#define ENABLE_DISPLAY 0       // Disabled - saves ~3 seconds
#define ENABLE_RTC 1           // Enabled (lightweight)
#define ENABLE_PPPOS 0         // Disabled (not working)

// ========== Communication Channel Commands ==========
#define ENABLE_BLE_COMMANDS 0
#define ENABLE_LORA_COMMANDS 0
#define ENABLE_WIFI_COMMANDS 1
#define ENABLE_HTTP_COMMANDS 0

// ========== SMS Settings ==========
#define ENABLE_SMS 1
#define ENABLE_SMS_COMMANDS 1
#define ENABLE_SMS_ALERTS 1

// ========== Copy essential settings from Config.h ==========
// (Only include what you're actively testing)

// Pin Definitions (keep for hardware setup)
#define MODEM_RX 45
#define MODEM_TX 46
#define MODEM_PWRKEY 4
#define MODEM_RESET 15
#define PUMP_PIN 25
#define PUMP_ACTIVE_HIGH true
#define RTC_SDA 41
#define RTC_SCL 42

// Modem/SMS Settings
#define MODEM_APN "airtelgprs.com"
#define SMS_ALERT_PHONE_1 "+919944272647"
#define SMS_ALERT_PHONE_2 ""
#define DEFAULT_ADMIN_PHONE SMS_ALERT_PHONE_1
#define DEFAULT_COUNTRY_CODE "+91"

// WiFi Settings
#define WIFI_SSID "sekaranfarm"
#define WIFI_PASS "welcome123"
#define WIFI_CONNECT_TIMEOUT_MS 15000

// Timing defaults
#define PUMP_ON_LEAD_DEFAULT_MS 3000
#define PUMP_OFF_DELAY_DEFAULT_MS 2000
#define LAST_CLOSE_DELAY_MS_DEFAULT 60000

// Storage
#define MAX_SCHEDULES 10
#define MAX_STEPS_PER_SCHEDULE 20

// Buffer sizes
#define LORA_BUFFER_SIZE 256
#define INCOMING_QUEUE_SIZE 10

#endif

/*
COMPILATION TIME SAVINGS:
- Full build (all features): ~45-60 seconds
- Fast build (minimal):      ~15-25 seconds
- Savings: ~30-40 seconds per compile!

HOW TO USE:
1. During development: #include "Config_Fast.h"
2. For production/testing: #include "Config.h"
3. Switch back and forth as needed
*/
