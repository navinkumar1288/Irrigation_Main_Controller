// IrrigationController.ino - Refactored with UserCommunication and NodeCommunication modules
// Clean separation: User interaction, Node communication, and Business logic
#include <map>  // For SMS rate limiting
#include "Config.h"
#include "Utils.h"
#include "MessageQueue.h"
#include "StorageManager.h"
#include "TimeManager.h"
#include "DisplayManager.h"
#include "LoRaComm.h"
#include "ModemMQTT.h"        // MQTT module
#include "ModemSMS.h"         // SMS module
#include "BLEComm.h"
#include "ScheduleManager.h"
#include "NodeCommunication.h"  // NEW: Node communication module
#include "UserCommunication.h"  // NEW: User communication module

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
ModemMQTT mqtt;               // MQTT instance
ModemSMS sms;                 // SMS instance
BLEComm bleComm;
ScheduleManager scheduleMgr;
NodeCommunication nodeComm;   // NEW: Node communication module
UserCommunication userComm;   // NEW: User communication module

TwoWire WireRTC = TwoWire(1);
RTC_DS3231 rtc;
bool rtcAvailable = false;
bool loraInitialized = false;

// ========== SMS Rate Limiting ==========
// Track last SMS alert time by key to prevent spam
std::map<String, unsigned long> lastSMSAlertTime;

bool shouldSendSMSAlert(const String &alertKey) {
  unsigned long now = millis();

  if (lastSMSAlertTime.find(alertKey) == lastSMSAlertTime.end()) {
    // First time seeing this alert
    lastSMSAlertTime[alertKey] = now;
    return true;
  }

  unsigned long timeSinceLastAlert = now - lastSMSAlertTime[alertKey];

  if (timeSinceLastAlert >= SMS_ALERT_RATE_LIMIT_MS) {
    lastSMSAlertTime[alertKey] = now;
    return true;
  }

  Serial.println("[SMS] Alert rate-limited: " + alertKey + " (sent " +
                 String(timeSinceLastAlert/1000) + "s ago)");
  return false;
}

// ========== Status Publishing ==========
void publishStatus(const String &msg) {
  Serial.println("[Status] " + msg);

  #if ENABLE_MQTT
  // MQTT enabled - publish to MQTT
  if (mqtt.isConnected()) {
    mqtt.publish(MQTT_TOPIC_STATUS, msg);
    Serial.println("[Status] → Published to MQTT");
  }
  #elif ENABLE_SMS
  // MQTT disabled, SMS enabled - send important status via SMS
  // Only send critical events to avoid SMS flooding
  if (msg.indexOf("EVT|") >= 0 || msg.indexOf("BOOT") >= 0 ||
      msg.indexOf("ERROR") >= 0 || msg.indexOf("FAIL") >= 0) {
    sendSMSNotification("Status: " + msg, "");
    Serial.println("[Status] → Sent via SMS (MQTT disabled)");
  }
  #endif

  #if ENABLE_BLE
  if (bleComm.isConnected()) {
    bleComm.notify("STAT|" + msg);
  }
  #endif
}

// ========== SMS Notification Function ==========
void sendSMSNotification(const String &message, const String &alertKey = "") {
  #if ENABLE_SMS_ALERTS
  if (!sms.isReady() || !ENABLE_SMS_BROADCAST) {
    return;
  }

  // Check rate limiting if alert key provided
  if (alertKey.length() > 0 && !shouldSendSMSAlert(alertKey)) {
    return;  // Skip sending - rate limited
  }

  // Send to configured phone numbers (defined in Config.h)
  #ifdef SMS_ALERT_PHONE_1
  if (String(SMS_ALERT_PHONE_1).length() > 0) {
    sms.sendSMS(SMS_ALERT_PHONE_1, message);
    Serial.println("[SMS] Sent to: " + String(SMS_ALERT_PHONE_1));
  }
  #endif

  #ifdef SMS_ALERT_PHONE_2
  if (String(SMS_ALERT_PHONE_2).length() > 0) {
    sms.sendSMS(SMS_ALERT_PHONE_2, message);
    Serial.println("[SMS] Sent to: " + String(SMS_ALERT_PHONE_2));
  }
  #endif
  #endif
}

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
  
  // Initialize Modem (base initialization)
  Serial.println("[7/9] Modem...");
  #if ENABLE_MODEM
  bool modemInitialized = false;

  // Initialize modem base - use MQTT if enabled, otherwise use SMS
  #if ENABLE_MQTT
  if (mqtt.init()) {
    Serial.println("      ✓ Modem initialized via MQTT");
    modemInitialized = true;
  }
  #elif ENABLE_SMS
  if (sms.init()) {
    Serial.println("      ✓ Modem initialized via SMS");
    modemInitialized = true;
  }
  #endif

  if (modemInitialized) {
    // Configure SMS FIRST (if enabled)
    // SMS configuration is fast and critical for receiving commands
    // Must happen before MQTT to avoid losing SMS during MQTT setup delays
    #if ENABLE_SMS
    Serial.println("      → Configuring SMS...");
    if (sms.configure()) {
      Serial.println("      ✓ SMS configured");
    } else {
      Serial.println("      ❌ SMS configuration failed");
    }
    #endif

    // Configure MQTT SECOND (if enabled and modem initialized via MQTT)
    // MQTT takes longer due to network/broker connection
    // SMS URCs arriving during MQTT setup will be forwarded to SMS handler
    #if ENABLE_MQTT
    Serial.println("      → Configuring MQTT...");
    if (mqtt.configure()) {
      Serial.println("      ✓ MQTT configured");
      // Subscribe to command topics
      mqtt.subscribe(MQTT_TOPIC_COMMANDS);
      Serial.println("      ✓ Subscribed to commands");
    } else {
      Serial.println("      ❌ MQTT configuration failed");
    }
    #endif
  } else {
    Serial.println("      ❌ Modem initialization failed");
  }
  #endif
  
  // Initialize BLE
  Serial.println("[8/9] BLE...");
  #if ENABLE_BLE
  if (bleComm.init()) {
    bleComm.setCommandCallback(handleBLECommand);
    Serial.println("      ✓ BLE OK");
  }
  #endif

  // Initialize UserCommunication module (depends on SMS, BLE, LoRa, MQTT, NodeComm)
  String adminPhone = "";
  #ifdef SMS_ALERT_PHONE_1
  adminPhone = String(SMS_ALERT_PHONE_1);
  #endif
  userComm.init(&sms, &bleComm, &loraComm, &mqtt, &nodeComm, adminPhone);
  Serial.println("      ✓ UserComm initialized");

  // Set up NodeCommunication callback for business logic
  nodeComm.setMessageCallback([](const NodeMessage& msg) {
    // ========== Business Logic for Node Messages ==========
    if (msg.type == NodeMessageType::TELEMETRY) {
      Serial.printf("[Business] Node %d Telemetry: BATT=%d%%, BV=%.2fV, SOLV=%.2fV\n",
                    msg.nodeId, msg.batteryPercent, msg.batteryVoltage, msg.solarVoltage);

      // Low battery alert (business logic)
      if (msg.batteryPercent < 20) {
        publishStatus("WARN|LOW_BATT|N=" + String(msg.nodeId) + "|BATT=" + String(msg.batteryPercent));
        sendSMSNotification("WARN: Low battery on Node " + String(msg.nodeId) +
                            " - " + String(msg.batteryPercent) + "%",
                            "LOW_BATT_N" + String(msg.nodeId));
      }
    } else if (msg.type == NodeMessageType::AUTO_CLOSE) {
      Serial.printf("[Business] Node %d Auto-Close: %s\n", msg.nodeId, msg.reason.c_str());
      publishStatus("EVT|AUTO_CLOSE|N=" + String(msg.nodeId));
    }
  });

  // Initialize Scheduler
  Serial.println("[9/9] Scheduler...");
  Serial.println("      ✓ Scheduler ready");
  Serial.printf("      ✓ %d schedules loaded\n", schedules.size());

  // Final status
  Serial.println("\n========================================");
  Serial.println("✓ SETUP COMPLETE");
  Serial.println("========================================");
  Serial.println("LoRa:    " + String(loraInitialized ? "OK" : "FAILED"));
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
  
  // Publish boot event (important event - keep this)
  publishStatus("EVT|BOOT|OK|V2.0");

  // Send boot notification via enabled communication method
  #if ENABLE_MQTT
  // MQTT mode - boot notification already sent via publishStatus
  #elif ENABLE_SMS
  // SMS mode - send boot notification
  sendSMSNotification("Irrigation Controller v2.0 Started (SMS Mode). LoRa: " +
                      String(loraInitialized ? "ON" : "OFF"), "");
  #endif
}

// ========== Main Loop ==========
unsigned long lastSchedulerCheck = 0;
unsigned long lastSMSCheck = 0;
unsigned long lastHeartbeat = 0;  // For debug heartbeat

void loop() {
  // Debug heartbeat - print every 30 seconds to confirm loop is running
  if (millis() - lastHeartbeat > 30000) {
    lastHeartbeat = millis();
    Serial.println("[Loop] ❤ Heartbeat - Loop running");
    #if ENABLE_SMS
    Serial.println("[Loop] SMS Status: Ready=" + String(sms.isReady() ? "YES" : "NO") +
                   ", Queued=" + String(sms.getUnreadCount()));
    #endif
  }

  // Process LoRa incoming (via NodeCommunication module - low-level receive)
  #if ENABLE_LORA
  if (loraInitialized) {
    nodeComm.processIncoming();  // Receives LoRa packets and queues them
    nodeComm.processNodeMessages();  // Process node-specific messages (STAT, AUTO_CLOSE)
  }
  #endif

  // Process all user communication channels (SMS, BLE, LoRa, MQTT)
  #if ENABLE_SMS_COMMANDS || ENABLE_LORA || ENABLE_MQTT
  static unsigned long lastUserCommCheck = 0;
  if (millis() - lastUserCommCheck > 1000) {  // Check every second
    lastUserCommCheck = millis();
    userComm.processAllChannels(&schedules, &scheduleRunning, &scheduleLoaded, &ENABLE_SMS_BROADCAST);
  }
  #endif

  // Process MQTT background (handles auto-reconnect, URCs)
  #if ENABLE_MQTT
  mqtt.processBackground();

  // Check if MQTT needs reconfiguration after modem restart
  // Note: needsReconfiguration() now handles throttling and attempt limiting
  if (mqtt.needsReconfiguration()) {
    Serial.println("[Main] ⚠ MQTT needs reconfiguration, waiting for modem...");
    // Wait for modem to be fully initialized (detected via +QIND: SMS DONE)
    // This typically takes 5-6 seconds after RDY
    delay(6000);
    if (mqtt.configure()) {
      Serial.println("[Main] ✓ MQTT reconfigured successfully");
    } else {
      Serial.println("[Main] ❌ MQTT reconfiguration failed (will retry with backoff)");
      // Don't block here - let SMS reconfigure too
    }
  }
  #endif

  // Process SMS background (handles new messages, URCs)
  #if ENABLE_SMS
  sms.processBackground();

  // Auto-reconfigure SMS if modem restarted
  // This is simple: if SMS becomes not ready, reconfigure it
  if (!sms.isReady()) {
    static unsigned long lastReconfigAttempt = 0;
    // Only try once per 5 seconds to avoid spam
    if (millis() - lastReconfigAttempt > 5000) {
      lastReconfigAttempt = millis();
      Serial.println("[Main] ⚠ SMS not ready - attempting reconfiguration...");
      if (sms.configure()) {
        Serial.println("[Main] ✓ SMS reconfigured successfully");
      } else {
        Serial.println("[Main] ❌ SMS reconfiguration failed (will retry)");
      }
    }
  }
  #endif
  
  // Periodically scan for messages (bypasses URC system)
  // This is a workaround if +CMTI URCs are not being received
  #if ENABLE_SMS
  static unsigned long lastMessageScan = 0;
  if (millis() - lastMessageScan > 30000) {  // Every 30 seconds
    lastMessageScan = millis();
    if (sms.isReady()) {
      Serial.println("[Loop] → Periodic message scan (URC bypass)");
      sms.scanForNewMessages();
    }
  }
  #endif

  // ========== Process Serial Commands ==========
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    
    if (line.length() > 0) {
      Serial.println("\n[Serial] ==================");
      Serial.println("[Serial] Input: " + line);
      
      // Check for special SMS diagnostic command
      if (line.equalsIgnoreCase("SMSDIAG") || line.equalsIgnoreCase("SMS DIAG")) {
        Serial.println("[Serial] Running SMS diagnostics...");
        #if ENABLE_SMS
        sms.printSMSDiagnostics();
        Serial.println("\n[Serial] Forcing message scan...");
        sms.scanForNewMessages();
        #else
        Serial.println("[Serial] SMS is disabled");
        #endif
      }
      // Delete all messages (useful for clearing old PDU messages)
      else if (line.equalsIgnoreCase("SMSCLEAN") || line.equalsIgnoreCase("SMS CLEAN")) {
        Serial.println("[Serial] Deleting all SMS messages...");
        #if ENABLE_SMS
        if (sms.deleteAllSMS()) {
          Serial.println("[Serial] ✓ All messages deleted");
          Serial.println("[Serial] ℹ Please resend your SMS in text format");
        } else {
          Serial.println("[Serial] ❌ Failed to delete messages");
        }
        #else
        Serial.println("[Serial] SMS is disabled");
        #endif
      }
      // Reconfigure SMS (useful after cleaning)
      else if (line.equalsIgnoreCase("SMSCONFIG") || line.equalsIgnoreCase("SMS CONFIG")) {
        Serial.println("[Serial] Reconfiguring SMS...");
        #if ENABLE_SMS
        if (sms.configure()) {
          Serial.println("[Serial] ✓ SMS reconfigured");
        } else {
          Serial.println("[Serial] ❌ SMS configuration failed");
        }
        #else
        Serial.println("[Serial] SMS is disabled");
        #endif
      }
      // Check if it's a schedule
      else if (line.startsWith("SCH|") || line.startsWith("{")) {
        Serial.println("[Serial] Schedule detected, queuing...");
        if (line.indexOf("SRC=") < 0) line += ",SRC=SERIAL";
        incomingQueue.enqueue(line);
      }
      // It's a simple command: <node> <command>
      else {
        int space = line.indexOf(' ');
        if (space > 0) {
          int node = line.substring(0, space).toInt();
          String cmd = line.substring(space + 1);
          cmd.toUpperCase();
          cmd.trim();
          
          if (node > 0 && node <= 255 && cmd.length() > 0) {
            Serial.printf("[Serial] Node: %d, Command: %s\n", node, cmd.c_str());

            #if ENABLE_LORA
            if (loraInitialized && nodeComm.isInitialized()) {
              Serial.println("[Serial] Sending via NodeComm...");
              bool result = nodeComm.sendCommand(node, cmd);

              if (result) {
                Serial.println("[Serial] ✓✓✓ SUCCESS ✓✓✓");
                // Publish manual command success (important event)
                publishStatus("EVT|CMD|N=" + String(node) + "|C=" + cmd + "|OK");
              } else {
                Serial.println("[Serial] ✗✗✗ FAILED ✗✗✗");
                // Publish manual command failure (important event)
                publishStatus("ERR|CMD|N=" + String(node) + "|C=" + cmd + "|FAIL");

                // Send SMS alert for failed commands (with rate limiting)
                sendSMSNotification("ALERT: LoRa command failed. Node: " +
                                    String(node) + ", Cmd: " + cmd,
                                    "LORA_FAIL_N" + String(node));
              }
            } else {
              Serial.println("[Serial] ✗ LoRa/NodeComm not initialized");
            }
            #else
            Serial.println("[Serial] ✗ LoRa disabled");
            #endif
          } else {
            Serial.println("[Serial] ✗ Invalid format");
            Serial.println("[Serial] Use: <node> <command>");
            Serial.println("[Serial] Example: 1 PING");
          }
        } else {
          Serial.println("[Serial] ✗ Invalid format");
          Serial.println("[Serial] Use: <node> <command>");
        }
      }
      
      Serial.println("[Serial] ==================\n");
    }
  }
  
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
        // Publish schedule load success (important event - keep this)
        publishStatus("EVT|SCH|LOADED");
        // Send SMS notification
        sendSMSNotification("Schedule loaded successfully", "");
      } else {
        Serial.println("[Queue] ✗ Schedule invalid");
        // Publish schedule load failure (important event - keep this)
        publishStatus("ERR|SCH|INVALID");
        // Send SMS alert
        sendSMSNotification("ERROR: Invalid schedule format", "");
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

          // Publish schedule trigger (important event - keep this)
          publishStatus("EVT|SCH|TRIGGER|S=" + sch.id);

          // Send SMS notification
          sendSMSNotification("Schedule started: " + sch.id, "");

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
