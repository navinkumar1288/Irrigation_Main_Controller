// Config.h - Complete configuration with all constants
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <vector>
#include <cstdint>
#include <ctime>

// ========== Feature Enables ==========
#define ENABLE_LORA 1
#define ENABLE_MODEM 1
#define ENABLE_MQTT 1
#define ENABLE_SMS 1
#define ENABLE_SMS_COMMANDS 1
#define ENABLE_SMS_ALERTS 1
#define ENABLE_BLE 1
#define ENABLE_DISPLAY 1
#define ENABLE_RTC 1

// ========== Buffer Sizes ==========
#define LORA_BUFFER_SIZE 256
#define INCOMING_QUEUE_SIZE 10

// ========== Pin Definitions ==========
// LoRa SX1276
#define LORA_SCK 9
#define LORA_MISO 11
#define LORA_MOSI 10
#define LORA_SS 8
#define LORA_RST 12
#define LORA_DIO0 14

// Modem EC200U
#define MODEM_RX 45
#define MODEM_TX 46
#define MODEM_PWRKEY 4
#define MODEM_RESET 15  // Changed from 5 to avoid conflict with LORA_SS

// Pump Control
#define PUMP_PIN 25
#define PUMP_ACTIVE_HIGH true

// RTC DS3231
#define RTC_SDA 41
#define RTC_SCL 42

// Display (if enabled)
#define DISPLAY_SDA 21
#define DISPLAY_SCL 22

// ========== LoRa Settings ==========
// India uses 865-867 MHz ISM band for LoRa
#define LORA_FREQUENCY 865E6          // 865 MHz for India (legal frequency)
#define RF_FREQUENCY 865000000        // Same as above in Hz
#define LORA_SYNC_WORD 0x12
#define LORA_SPREADING_FACTOR 10
#define LORA_BANDWIDTH 0         // 125 kHz
#define LORA_CODINGRATE 1             // 4/5
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define TX_OUTPUT_POWER 14            // dBm (14-20 dBm allowed in India, use 14 for safety)
#define LORA_TX_POWER 14

#define LORA_GATEWAY_ID 255           // This device's ID
#define LORA_BROADCAST_ID 0           // Broadcast address
#define MAX_RETRIES 3
#define LORA_MAX_RETRIES 3
#define ACK_TIMEOUT_MS 5000
#define LORA_ACK_TIMEOUT_MS 5000

// ========== WiFi Settings ==========
#define WIFI_SSID "sekarfarm"
#define WIFI_PASS "welcome123"
#define WIFI_CONNECT_TIMEOUT_MS 15000

// ========== NTP Settings ==========
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC (5.5 * 3600)   // IST = UTC+5:30
#define DAYLIGHT_OFFSET_SEC 0
#define NTP_TIMEOUT_MS 10000
#define NTP_TIMEZONE_OFFSET 0

// ========== MQTT Settings ==========
#define MQTT_BROKER "39aff691b9b5421ab98adc2addedbd83.s1.eu.hivemq.cloud"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "irrigation_controller_001"
#define MQTT_USER "navin"
#define MQTT_PASS "HaiNavin33"

#define MQTT_TOPIC_STATUS "irrigation/status"
#define MQTT_TOPIC_COMMANDS "irrigation/commands"
#define MQTT_TOPIC_TELEMETRY "irrigation/telemetry"
#define MQTT_TOPIC_ALERTS "irrigation/alerts"

// Default MQTT values for storage
#define DEFAULT_MQTT_SERVER MQTT_BROKER
#define DEFAULT_MQTT_PORT MQTT_PORT
#define DEFAULT_MQTT_USER MQTT_USER
#define DEFAULT_MQTT_PASS MQTT_PASS

// ========== Modem Settings ==========
#define MODEM_APN "airtelgprs.com"
#define DEFAULT_SIM_APN MODEM_APN

// ========== SMS Settings ==========
#define SMS_ALERT_PHONE_1 "+919944272647"
#define SMS_ALERT_PHONE_2 "+0987654321"
#define DEFAULT_ADMIN_PHONE SMS_ALERT_PHONE_1
#define DEFAULT_RECOV_TOK "RECOVERY123"
#define DEFAULT_COUNTRY_CODE "+91"  // Default country code for phone normalization

#define SMS_ALERT_ON_BOOT true
#define SMS_ALERT_ON_LOW_BATTERY true
#define SMS_ALERT_ON_SCHEDULE_FAIL true
#define SMS_ALERT_ON_COMMAND_FAIL true
#define SMS_CHECK_INTERVAL_MS 10000  // Check for SMS every 10 seconds
#define SMS_ALERT_RATE_LIMIT_MS 300000  // Minimum 5 minutes between duplicate alerts

// ========== BLE Settings ==========
#define BLE_DEVICE_NAME "IrrigationController"
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

// BLE Connection Parameters (units of 1.25ms)
#define BLE_MIN_CONN_INTERVAL 0x06  // 7.5ms (6 * 1.25ms)
#define BLE_MAX_CONN_INTERVAL 0x12  // 22.5ms (18 * 1.25ms)
#define BLE_MTU_SIZE 512            // Maximum Transmission Unit size

// ========== Display Settings ==========
#define DISPLAY_REFRESH_MS 1000

// ========== Storage Settings ==========
#define MAX_SCHEDULES 10
#define MAX_SEQUENCE_STEPS 20

// ========== Timing Defaults ==========
#define PUMP_ON_LEAD_DEFAULT_MS 3000
#define PUMP_OFF_DELAY_DEFAULT_MS 3000
#define LAST_CLOSE_DELAY_MS_DEFAULT 2000
#define VALVE_OPEN_DELAY_MS 500
#define SAVE_PROGRESS_INTERVAL_MS 60000  // Save schedule progress every 60 seconds

// ========== System Settings ==========
#define SERIAL_BAUD 115200
#define QUEUE_SIZE 10

// ========== Structures ==========
struct SystemConfig {
  char device_id[32];
  uint32_t lora_frequency;
  uint8_t lora_sf;
  bool enable_sms_broadcast;
  char timezone[32];
  
  // Additional fields required by StorageManager
  String mqttServer;
  int mqttPort;
  String mqttUser;
  String mqttPass;
  String adminPhones;
  String simApn;
  String sharedTok;
  String recoveryTok;
};

struct SeqStep {
  uint8_t node_id;
  uint8_t valve_id;
  uint32_t duration_ms;
};

struct Schedule {
  String id;
  char rec;             // 'O'=Once, 'D'=Daily, 'W'=Weekly, 'M'=Monthly
  int interval;
  time_t start_time;
  time_t start_epoch;   // Required by ScheduleManager
  time_t next_run_epoch;
  String timeStr;       // Required by ScheduleManager for parsing time
  uint8_t weekday_mask; // Required by ScheduleManager for weekly schedules
  uint32_t ts;          // Timestamp - required by ScheduleManager
  bool enabled;
  std::vector<SeqStep> seq;
  uint32_t pump_on_before_ms;
  uint32_t pump_off_after_ms;
};


// ========== Global State (extern in Config.h, defined in main .ino) ==========
extern SystemConfig sysConfig;
extern std::vector<Schedule> schedules;
extern String currentScheduleId;
extern std::vector<SeqStep> seq;
extern int currentStepIndex;
extern unsigned long stepStartMillis;
extern bool scheduleLoaded;
extern bool scheduleRunning;
extern time_t scheduleStartEpoch;
extern uint32_t pumpOnBeforeMs;
extern uint32_t pumpOffAfterMs;
extern uint32_t LAST_CLOSE_DELAY_MS;
extern uint32_t DRIFT_THRESHOLD_S;
extern uint32_t SYNC_CHECK_INTERVAL_MS;
extern bool ENABLE_SMS_BROADCAST;


// ========== Debug Settings ==========
// #define DEBUG_LORA
// #define DEBUG_MQTT
// #define DEBUG_SMS
// #define DEBUG_SCHEDULER

#ifdef DEBUG_LORA
  #define DEBUG_LORA_PRINT(x) Serial.print(x)
  #define DEBUG_LORA_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_LORA_PRINT(x)
  #define DEBUG_LORA_PRINTLN(x)
#endif

#ifdef DEBUG_MQTT
  #define DEBUG_MQTT_PRINT(x) Serial.print(x)
  #define DEBUG_MQTT_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_MQTT_PRINT(x)
  #define DEBUG_MQTT_PRINTLN(x)
#endif

#ifdef DEBUG_SMS
  #define DEBUG_SMS_PRINT(x) Serial.print(x)
  #define DEBUG_SMS_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_SMS_PRINT(x)
  #define DEBUG_SMS_PRINTLN(x)
#endif

#ifdef DEBUG_SCHEDULER
  #define DEBUG_SCH_PRINT(x) Serial.print(x)
  #define DEBUG_SCH_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_SCH_PRINT(x)
  #define DEBUG_SCH_PRINTLN(x)
#endif

// ========== Network Settings ==========
#define MQTT_CONNECT_TIMEOUT_MS 10000
#define SMS_SEND_TIMEOUT_MS 30000
#define NETWORK_REGISTRATION_TIMEOUT_S 120  // Increased from 60 to 120 seconds

#define MQTT_MAX_RECONNECT_ATTEMPTS 5
#define MQTT_RECONNECT_DELAY_MS 5000

// ========== Forward Declarations ==========
class MessageQueue;
class StorageManager;
class LoRaComm;
class Preferences;

extern MessageQueue incomingQueue;
extern StorageManager storage;
extern LoRaComm loraComm;
extern Preferences prefs;

#endif // CONFIG_H
