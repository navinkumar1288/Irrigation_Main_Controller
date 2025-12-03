// IrrigationController.ino - Refactored with UserCommunication and NodeCommunication modules
// Clean separation: User interaction, Node communication, and Business logic
#include "Config.h"
#include "Utils.h"
#include "MessageQueue.h"
#include "StorageManager.h"
#include "TimeManager.h"
#include "DisplayManager.h"
#include "LoRaComm.h"
#include "MQTTComm.h"          // MQTT module (ESP32 native, works with PPPoS/WiFi)
#include "ModemSMS.h"         // SMS module
#include "BLEComm.h"
#include "WiFiComm.h"         // WiFi module
#include "HTTPComm.h"         // HTTP API module
#include "ScheduleManager.h"
#include "NodeCommunication.h"  // NEW: Node communication module
#include "UserCommunication.h"  // NEW: User communication module
#include "PPPoSManager.h"       // PPPoS cellular data module
#include "IrrigationNetworkManager.h"  // Unified network manager (PPPoS/WiFi fallback)
#include "MessageFormats.h"     // Compact message formats for SMS/data efficiency

// ========== Global Variable Definitions ==========
SystemConfig sysConfig;
std::vector<Schedule> schedules;
String currentScheduleId = "";
std::vector<SeqStep> seq;
int currentStepIndex = -1;
unsigned long stepStartMillis = 0;
bool scheduleLoaded = false;
bool scheduleRunning = false;
time_t scheduleStartEpoch = 0;
uint32_t pumpOnBeforeMs = PUMP_ON_LEAD_DEFAULT_MS;
uint32_t pumpOffAfterMs = PUMP_OFF_DELAY_DEFAULT_MS;
uint32_t LAST_CLOSE_DELAY_MS = LAST_CLOSE_DELAY_MS_DEFAULT;
uint32_t DRIFT_THRESHOLD_S = 300;
uint32_t SYNC_CHECK_INTERVAL_MS = 3600000UL;
bool ENABLE_SMS_BROADCAST = true;

// ========== Module Instances ==========
Preferences prefs;
MessageQueue incomingQueue;
StorageManager storage;
TimeManager timeManager;
DisplayManager displayMgr;
LoRaComm loraComm;
MQTTComm mqtt;                // MQTT instance (ESP32 native)
ModemSMS sms;                 // SMS instance
BLEComm bleComm;
WiFiComm wifiComm;            // WiFi instance
HTTPComm httpComm;            // HTTP API instance
ScheduleManager scheduleMgr;
NodeCommunication nodeComm;   // NEW: Node communication module
UserCommunication userComm;   // NEW: User communication module
PPPoSManager pppos;           // PPPoS cellular data module
IrrigationNetworkManager networkMgr;  // Unified network manager (PPPoS/WiFi fallback)

TwoWire WireRTC = TwoWire(1);
RTC_DS3231 rtc;
bool rtcAvailable = false;
bool loraInitialized = false;

// ========== BLE Command Handler Callback (Delegated to UserCommunication) ==========
void handleBLECommand(int node, String command) {
  // Delegate to UserCommunication module
  userComm.processBLECommand(node, command);
}

// ========== Setup ==========
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("  Irrigation Controller v2.0");
  Serial.println("  Event-Driven Status (No Heartbeat)");
  Serial.println("  MQTT + SMS Modules (Refactored)");
  Serial.println("========================================\n");
  
  // Initialize storage
  Serial.println("[1/9] Storage...");
  if (storage.init()) {
    Serial.println("      ✓ Storage OK");
  }
  
  // Initialize preferences
  Serial.println("[2/9] Preferences...");
  prefs.begin("irrig", false);
  Serial.println("      ✓ Prefs OK");
  
  // Load config
  Serial.println("[3/9] Config...");
  storage.loadSystemConfig(sysConfig);
  storage.loadAllSchedules(schedules);
  Serial.println("      ✓ Config loaded");
  
  // Initialize display
  Serial.println("[4/9] Display...");
  #if ENABLE_DISPLAY
  if (displayMgr.init()) {
    displayMgr.showMessage("Irrigation", "Controller v2.0", "Initializing...", "");
    Serial.println("      ✓ Display OK");
  }
  #endif
  
  // Initialize RTC
  Serial.println("[5/9] RTC...");
  #if ENABLE_RTC
  rtcAvailable = timeManager.init(&WireRTC);
  if (rtcAvailable) {
    Serial.println("      ✓ RTC OK");
  }
  #endif
  
  // Initialize LoRa
  Serial.println("[6/9] LoRa...");
  #if ENABLE_LORA
  delay(500);
  if (loraComm.init()) {
    loraInitialized = true;
    Serial.println("      ✓ LoRa OK");

    // Initialize NodeCommunication module (depends on LoRa)
    if (nodeComm.init(&loraComm)) {
      Serial.println("      ✓ NodeComm initialized");
    }
  } else {
    Serial.println("      ❌ LoRa FAILED");
  }
  #endif
  
  // Initialize Modem/Network
  Serial.println("[7/9] Modem/Network...");
  #if ENABLE_MODEM
  bool modemInitialized = false;
  bool networkAvailable = false;

  // Initialize modem base (for SMS and AT commands)
  #if ENABLE_SMS
  if (sms.init()) {
    Serial.println("      ✓ Modem initialized");
    modemInitialized = true;

    // Configure SMS
    Serial.println("      → Configuring SMS...");
    if (sms.configure()) {
      Serial.println("      ✓ SMS configured");
    } else {
      Serial.println("      ❌ SMS configuration failed");
    }
  } else {
    Serial.println("      ❌ Modem initialization failed");
  }
  #endif

  // Configure network connectivity with automatic fallback (PPPoS → WiFi)
  #if ENABLE_PPPOS || ENABLE_WIFI
  // Note: For PPPoS, we need to initialize modem first (even without SMS)
  #if ENABLE_PPPOS && !ENABLE_SMS
  if (sms.init()) {  // Use SMS module to initialize modem base
    Serial.println("      ✓ Modem initialized for network connectivity");
    modemInitialized = true;
  }
  #endif

  // Initialize NetworkManager with PPPoS and WiFi fallback
  Serial.println("      → Initializing Network Manager...");
  networkMgr.init(&pppos, &wifiComm, &SerialAT);
  networkMgr.setReconnectInterval(NETWORK_RECONNECT_INTERVAL_MS);

  // Attempt connection with automatic fallback (PPPoS → WiFi)
  Serial.println("      → Connecting to network (PPPoS → WiFi fallback)...");
  if (networkMgr.connect(PPPOS_CONNECT_TIMEOUT_MS, WIFI_CONNECT_TIMEOUT_MS)) {
    networkAvailable = true;
    Serial.println("      ✓ Network connected!");
    Serial.println("      ✓ Connection: " +
                   String(networkMgr.getConnectionType() == ConnectionType::PPPOS ? "PPPoS (Cellular)" : "WiFi"));
    Serial.println("      ✓ IP: " + networkMgr.getLocalIP());
  } else {
    Serial.println("      ❌ All network connections failed");
    Serial.println("      ℹ Check: SIM card, WiFi credentials, network availability");
  }
  #endif

  #endif // ENABLE_MODEM

  // Initialize BLE
  Serial.println("[8/11] BLE...");
  #if ENABLE_BLE
  if (bleComm.init()) {
    bleComm.setCommandCallback(handleBLECommand);
    Serial.println("      ✓ BLE OK");
  }
  #endif

  // Initialize HTTP API (requires WiFi - local network access)
  Serial.println("[10/11] HTTP API...");
  #if ENABLE_HTTP
  #if ENABLE_WIFI
  // HTTP API only works with WiFi (needs local network, not cellular)
  if (networkAvailable && networkMgr.getConnectionType() == ConnectionType::WIFI) {
    if (httpComm.init(HTTP_SERVER_PORT)) {
      Serial.println("      ✓ HTTP API started on port " + String(HTTP_SERVER_PORT));
      Serial.println("      ✓ Access at: http://" + networkMgr.getLocalIP() + ":" + String(HTTP_SERVER_PORT));
    } else {
      Serial.println("      ❌ HTTP API failed");
    }
  } else {
    Serial.println("      ⚠ HTTP API skipped (WiFi not active)");
  }
  #else
  Serial.println("      ⚠ HTTP API requires WiFi (ENABLE_WIFI=0)");
  #endif
  #endif

  // Initialize MQTT (if enabled and network available)
  Serial.println("[10.5/11] MQTT...");
  #if ENABLE_MQTT
  if (networkAvailable && networkMgr.isConnected()) {
    Serial.print("      → MQTT over ");
    Serial.println(networkMgr.getConnectionType() == ConnectionType::PPPOS ? "PPPoS..." : "WiFi...");

    if (mqtt.init()) {
      if (mqtt.configure()) {
        Serial.println("      ✓ MQTT connected to broker");
        mqtt.subscribe(MQTT_TOPIC_COMMANDS);
        Serial.println("      ✓ Subscribed to commands");
      } else {
        Serial.println("      ❌ MQTT broker connection failed");
      }
    } else {
      Serial.println("      ❌ MQTT initialization failed");
    }
  } else {
    Serial.println("      ⚠ MQTT skipped (no network connection)");
  }
  #endif

  // Initialize UserCommunication module (depends on SMS, BLE, LoRa, MQTT, WiFi, HTTP)
  String adminPhone = "";
  #ifdef SMS_ALERT_PHONE_1
  adminPhone = String(SMS_ALERT_PHONE_1);
  #endif
  userComm.init(&sms, &bleComm, &loraComm, &mqtt, &wifiComm, &httpComm, adminPhone);
  Serial.println("      ✓ UserComm initialized");

  // Set up UserCommunication callback for node commands (business logic)
  userComm.setNodeCommandCallback([](int nodeId, const String& command) -> bool {
    // ========== Business Logic for User Node Commands ==========
    Serial.printf("[Business] User requested Node %d command: %s\n", nodeId, command.c_str());

    #if ENABLE_LORA
    if (loraInitialized && nodeComm.isInitialized()) {
      bool result = nodeComm.sendCommand(nodeId, command);

      if (result) {
        // Publish command success (business logic)
        userComm.publishStatus(MessageFormatter::formatCommandSuccess(nodeId, command));
      } else {
        // Publish command failure (business logic) - compact format saves SMS/data
        userComm.publishStatus(MessageFormatter::formatCommandFail(nodeId, command));
      }

      return result;
    }
    #endif

    return false;  // LoRa not available
  });

  // Set up NodeCommunication callback for node messages (business logic)
  nodeComm.setMessageCallback([](const NodeMessage& msg) {
    // ========== Business Logic for Node Messages ==========
    if (msg.type == NodeMessageType::TELEMETRY) {
      Serial.printf("[Business] Node %d Telemetry: BATT=%d%%, BV=%.2fV, SOLV=%.2fV\n",
                    msg.nodeId, msg.batteryPercent, msg.batteryVoltage, msg.solarVoltage);

      // Low battery alert (business logic) - compact format saves SMS/data
      if (msg.batteryPercent < 20) {
        userComm.publishStatus(MessageFormatter::formatLowBattery(msg.nodeId, msg.batteryPercent));
      }
    } else if (msg.type == NodeMessageType::AUTO_CLOSE) {
      Serial.printf("[Business] Node %d Auto-Close: %s\n", msg.nodeId, msg.reason.c_str());
      userComm.publishStatus(MessageFormatter::formatAutoClose(msg.nodeId));
    }
  });

  // Initialize Scheduler
  Serial.println("[11/11] Scheduler...");
  Serial.println("      ✓ Scheduler ready");
  Serial.printf("      ✓ %d schedules loaded\n", schedules.size());

  // Final status
  Serial.println("\n========================================");
  Serial.println("✓ SETUP COMPLETE");
  Serial.println("========================================");
  Serial.println("LoRa:    " + String(loraInitialized ? "OK" : "FAILED"));
  #if ENABLE_PPPOS
  Serial.println("PPPoS:   " + String(pppos.isConnected() ? "CONNECTED (" + pppos.getLocalIP() + ")" : "DISCONNECTED"));
  #endif
  #if ENABLE_MQTT
  Serial.println("MQTT:    " + String(mqtt.isConnected() ? "CONNECTED" : "DISCONNECTED"));
  Serial.println("SMS:     DISABLED (MQTT mode)");
  #elif ENABLE_SMS
  Serial.println("MQTT:    DISABLED (SMS mode)");
  Serial.println("SMS:     " + String(sms.isReady() ? "READY" : "NOT READY"));
  #endif
  Serial.println("========================================\n");
  
  if (loraInitialized) {
    Serial.println("Ready for commands!");
    Serial.println("Serial Commands:");
    Serial.println("  <node> <command>");
    Serial.println("Examples:");
    Serial.println("  1 PING");
    Serial.println("  1 STATUS");
    Serial.println("  1 OPEN");
    Serial.println("  1 CLOSE");
    Serial.println();
  }
  
  Serial.println("SMS Commands:");
  Serial.println("  STATUS - Get system status");
  Serial.println("  SCHEDULES - List schedules");
  Serial.println("  STOP - Stop all schedules");
  Serial.println("  SMS ON/OFF - Enable/disable SMS alerts");
  Serial.println("  CHECK - Manually scan for messages");
  Serial.println("  <id> <cmd> - Send LoRa command (e.g., 1 PING)");
  Serial.println("  HELP - Show commands");
  Serial.println();

  // Send boot notification via enabled communication method - compact format saves SMS/data
  #if ENABLE_MQTT
  bool mqttOk = mqtt.isConnected();
  #else
  bool mqttOk = false;
  #endif

  #if ENABLE_PPPOS || ENABLE_WIFI
  bool netOk = networkMgr.isConnected();
  #else
  bool netOk = false;
  #endif

  userComm.publishStatus(MessageFormatter::formatBoot(loraInitialized, mqttOk, netOk));
}

// ========== Main Loop ==========
unsigned long lastSchedulerCheck = 0;
unsigned long lastSMSCheck = 0;
unsigned long lastHeartbeat = 0;  // For debug heartbeat

void loop() {
  // Process network connectivity (CRITICAL for PPPoS and auto-reconnection)
  #if ENABLE_PPPOS || ENABLE_WIFI
  networkMgr.processBackground();  // Feeds PPP stack & handles auto-reconnection
  #endif

  // Debug heartbeat - print every 30 seconds to confirm loop is running
  if (millis() - lastHeartbeat > 30000) {
    lastHeartbeat = millis();
    Serial.println("[Loop] ❤ Heartbeat - Loop running");
    #if ENABLE_SMS
    Serial.println("[Loop] SMS Status: Ready=" + String(sms.isReady() ? "YES" : "NO") +
                   ", Queued=" + String(sms.getUnreadCount()));
    #endif
    #if ENABLE_PPPOS
    Serial.println("[Loop] PPPoS Status: Connected=" + String(pppos.isConnected() ? "YES" : "NO") +
                   ", IP=" + pppos.getLocalIP());
    #endif
  }

  // Process LoRa incoming (via NodeCommunication module - low-level receive)
  #if ENABLE_LORA
  if (loraInitialized) {
    nodeComm.processIncoming();  // Receives LoRa packets and queues them
    nodeComm.processNodeMessages();  // Process node-specific messages (STAT, AUTO_CLOSE)
  }
  #endif

  // Process all user communication (background, commands, all channels)
  userComm.processBackground();  // MQTT/SMS auto-reconnect, message scanning

  #if ENABLE_SMS_COMMANDS || ENABLE_LORA || ENABLE_MQTT
  static unsigned long lastUserCommCheck = 0;
  if (millis() - lastUserCommCheck > 1000) {  // Check every second
    lastUserCommCheck = millis();
    userComm.processAllChannels(&schedules, &scheduleRunning, &scheduleLoaded, &ENABLE_SMS_BROADCAST);
  }
  #endif

  // ========== Process Serial Commands (Delegated to UserCommunication) ==========
  userComm.processSerialInput(&schedules, &scheduleRunning, &scheduleLoaded);
  
  // ========== Process Queued Messages (Schedules only - node messages handled by NodeCommunication) ==========
  String msg;
  if (incomingQueue.dequeue(msg)) {
    Serial.println("\n[Queue] ==================");
    Serial.println("[Queue] Processing: " + msg);

    // Handle schedules (sent via LoRa or other channels)
    if (msg.indexOf("SCH|") >= 0 || msg.startsWith("{")) {
      Serial.println("[Queue] Schedule message");
      if (scheduleMgr.validateAndLoad(msg)) {
        Serial.println("[Queue] ✓ Schedule loaded");
        // Publish schedule load success - compact format saves SMS/data
        userComm.publishStatus(MessageFormatter::formatScheduleLoaded(currentScheduleId));
      } else {
        Serial.println("[Queue] ✗ Schedule invalid");
        // Publish schedule load failure - compact format saves SMS/data
        userComm.publishStatus(MessageFormatter::formatScheduleInvalid());
      }
    }
    // Unknown
    else {
      Serial.println("[Queue] Unknown message type");
      Serial.println("[Queue] " + msg);
    }
    
    Serial.println("[Queue] ==================\n");
  }
  
  // ========== Run Scheduler ==========
  scheduleMgr.runLoop();
  
  // ========== Check Schedule Triggers ==========
  if (millis() - lastSchedulerCheck > 5000) {
    time_t now = time(nullptr);

    // Only check for new schedules if none is currently running
    if (!scheduleRunning && !scheduleLoaded) {
      for (auto &sch : schedules) {
        if (!sch.enabled) continue;

        if (sch.next_run_epoch == 0) {
          sch.next_run_epoch = scheduleMgr.computeNextRun(sch, now);
        }

        if (sch.next_run_epoch > 0 && now >= sch.next_run_epoch) {
          Serial.println("[Scheduler] Triggering: " + sch.id);

          currentScheduleId = sch.id;
          seq.clear();
          for (auto &st : sch.seq) seq.push_back(st);
          pumpOnBeforeMs = sch.pump_on_before_ms;
          pumpOffAfterMs = sch.pump_off_after_ms;
          scheduleStartEpoch = sch.next_run_epoch;
          scheduleLoaded = true;
          currentStepIndex = -1;

          // Publish schedule start - compact format saves SMS/data
          userComm.publishStatus(MessageFormatter::formatScheduleStart(sch.id));

          if (sch.rec == 'O') {
            sch.enabled = false;
          }

          sch.next_run_epoch = scheduleMgr.computeNextRun(sch, now + 1);
          break;  // Only trigger one schedule at a time
        }
      }
    }

    lastSchedulerCheck = millis();
  }
  
  // ========== Check RTC Drift ==========
  #if ENABLE_RTC
  timeManager.checkDrift();
  #endif
  
  // ========== HEARTBEAT REMOVED ==========
  // Periodic heartbeat publishing has been DISABLED to save cellular data
  // Only important events (errors, schedule triggers, low battery, etc.) are published
  
  // ========== Update Display ==========
  #if ENABLE_DISPLAY
  displayMgr.update();
  #endif
  
  delay(10);
}
