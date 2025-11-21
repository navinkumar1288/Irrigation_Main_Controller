// IrrigationController.ino - Updated with ModemMQTT and ModemSMS modules
// Heartbeat DISABLED to save cellular data - only important events published
#include <map>  // For SMS rate limiting
#include "Config.h"
#include "Utils.h"
#include "MessageQueue.h"
#include "StorageManager.h"
#include "TimeManager.h"
#include "DisplayManager.h"
#include "LoRaComm.h"
#include "ModemMQTT.h"        // NEW: MQTT module
#include "ModemSMS.h"         // NEW: SMS module
#include "BLEComm.h"
#include "ScheduleManager.h"

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
ModemMQTT mqtt;               // NEW: MQTT instance
ModemSMS sms;                 // NEW: SMS instance
BLEComm bleComm;
ScheduleManager scheduleMgr;

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
  if (mqtt.isConnected()) {
    mqtt.publish(MQTT_TOPIC_STATUS, msg);
    Serial.println("[Status] → Published to MQTT");
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

// ========== Process SMS Commands ==========
void processSMSCommands() {
  #if ENABLE_SMS_COMMANDS
  if (!sms.isReady()) return;

  // Don't process messages if reconfiguration is needed (e.g., after modem restart)
  // This prevents trying to read PDU-mode messages before text mode is restored
  if (sms.needsReconfiguration()) {
    Serial.println("[SMS] ⏸ Skipping message processing - reconfiguration pending");
    return;
  }

  // Check for new messages
  if (sms.checkNewMessages()) {
    // Get actual message indices (not sequential like 1,2,3 but actual indices like 34,35,etc)
    std::vector<int> indices = sms.getUnreadIndices();
    Serial.println("[SMS] 📨 " + String(indices.size()) + " unread message(s)");

    // Process each message by actual index
    for (int index : indices) {
      SMSMessage msg;

      // Try to read the message
      if (sms.readSMS(index, msg)) {
        Serial.println("\n[SMS] ==================");
        Serial.println("[SMS] From: " + msg.sender);
        Serial.println("[SMS] Time: " + msg.timestamp);
        Serial.println("[SMS] Message: " + msg.message);
        
        // Process command
        String cmd = msg.message;
        cmd.trim();
        cmd.toUpperCase();
        
        String response = "";
        
        // STATUS command
        if (cmd == "STATUS") {
          response = "System OK. ";
          response += "MQTT: " + String(mqtt.isConnected() ? "ON" : "OFF") + ", ";
          response += "LoRa: " + String(loraInitialized ? "ON" : "OFF");
          if (scheduleRunning) {
            response += ", Schedule: RUNNING";
          }
        }
        // SCHEDULES command
        else if (cmd == "SCHEDULES") {
          response = "Schedules: ";
          int enabledCount = 0;
          for (auto &sch : schedules) {
            if (sch.enabled) enabledCount++;
          }
          response += String(enabledCount) + "/" + String(schedules.size()) + " enabled";
        }
        // START command (for testing)
        else if (cmd.startsWith("START ")) {
          String schedId = cmd.substring(6);
          schedId.trim();
          response = "Starting schedule: " + schedId;
          // Trigger schedule logic here
        }
        // STOP command
        else if (cmd == "STOP") {
          scheduleRunning = false;
          scheduleLoaded = false;
          response = "All schedules stopped";
          publishStatus("EVT|SMS_CMD|STOP");
        }
        // ENABLE SMS
        else if (cmd == "SMS ON") {
          ENABLE_SMS_BROADCAST = true;
          response = "SMS alerts enabled";
        }
        // DISABLE SMS
        else if (cmd == "SMS OFF") {
          ENABLE_SMS_BROADCAST = false;
          response = "SMS alerts disabled";
        }
        // NODE command - send LoRa command
        // Supports two formats:
        // 1. "NODE <id> <command>" - e.g., "NODE 1 PING"
        // 2. "<id> <command>" - e.g., "1 PING" (same as serial commands)
        else if (cmd.startsWith("NODE ") || (cmd.length() > 0 && isdigit(cmd.charAt(0)))) {
          int nodeId = 0;
          String nodeCmd = "";

          // Parse command format
          if (cmd.startsWith("NODE ")) {
            // Format: NODE <id> <command>
            int space1 = cmd.indexOf(' ', 5);
            if (space1 > 0) {
              String nodeStr = cmd.substring(5, space1);
              nodeCmd = cmd.substring(space1 + 1);
              nodeId = nodeStr.toInt();
            }
          } else {
            // Format: <id> <command>
            int space1 = cmd.indexOf(' ');
            if (space1 > 0) {
              String nodeStr = cmd.substring(0, space1);
              nodeCmd = cmd.substring(space1 + 1);
              nodeId = nodeStr.toInt();
            }
          }

          // Execute command if valid
          if (nodeId > 0 && nodeId <= 255 && nodeCmd.length() > 0) {
            #if ENABLE_LORA
            if (loraInitialized) {
              Serial.println("[SMS] ==================");
              Serial.println("[SMS] ✓ Command parsed successfully");
              Serial.println("[SMS]   Node ID: " + String(nodeId));
              Serial.println("[SMS]   Command: " + nodeCmd);
              Serial.println("[SMS] → Sending via LoRa...");

              bool result = loraComm.sendWithAck(nodeCmd, nodeId, "", 0, 0);

              if (result) {
                Serial.println("[SMS] ✓✓✓ LoRa SUCCESS ✓✓✓");
                response = "Node " + String(nodeId) + " OK: " + nodeCmd;
              } else {
                Serial.println("[SMS] ✗✗✗ LoRa TIMEOUT ✗✗✗");
                response = "Node " + String(nodeId) + " TIMEOUT";
              }
            } else {
              Serial.println("[SMS] ❌ LoRa NOT initialized!");
              Serial.println("[SMS]   loraInitialized = false");
              response = "LoRa not available";
            }
            #else
            Serial.println("[SMS] ❌ LoRa DISABLED in Config.h");
            Serial.println("[SMS]   ENABLE_LORA is not set");
            response = "LoRa disabled";
            #endif
          } else {
            Serial.println("[SMS] ❌ Invalid command parameters:");
            Serial.println("[SMS]   NodeID: " + String(nodeId) + " (valid: 1-255)");
            Serial.println("[SMS]   Command: '" + nodeCmd + "' (length: " + String(nodeCmd.length()) + ")");
            response = "Format: <id> <cmd> OR NODE <id> <cmd>";
          }
        }
        // HELP command
        else if (cmd == "HELP") {
          response = "Commands: STATUS, SCHEDULES, STOP, SMS ON/OFF, <id> <cmd> (e.g., 1 PING), HELP";
        }
        // Unknown command
        else {
          response = "Unknown command. Send HELP for list.";
        }
        
        // Send response
        if (response.length() > 0) {
          sms.sendSMS(msg.sender, response);
          Serial.println("[SMS] Response: " + response);
        }
        
        // Delete processed message
        sms.deleteSMS(msg.index);
        
        Serial.println("[SMS] ==================\n");

        // Publish SMS command event
        publishStatus("EVT|SMS_CMD|" + cmd);
      } else {
        // Failed to read message (likely PDU mode or other error)
        Serial.println("[SMS] ⚠ Failed to read message at index " + String(index));

        // If reconfiguration is needed, re-queue for retry after reconfiguration
        if (sms.needsReconfiguration()) {
          sms.requeueMessage(index);
          Serial.println("[SMS] → Message will be retried after reconfiguration");
        } else {
          // Unknown error - still try to delete to avoid infinite loop
          Serial.println("[SMS] ⚠ Deleting unreadable message");
          sms.deleteSMS(index);
        }
      }
    }
  }
  #endif
}

// ========== BLE Command Handler Callback ==========
void handleBLECommand(int node, String command) {
  Serial.printf("[BLE Handler] Node=%d, Command=%s\n", node, command.c_str());
  
  #if ENABLE_LORA
  if (loraInitialized) {
    bool result = loraComm.sendWithAck(command, node, "", 0, 0);
    
    String response;
    if (result) {
      response = "OK|Node " + String(node) + " responded";
      Serial.println("[BLE Handler] ✓ Success");
    } else {
      response = "FAIL|Node " + String(node) + " timeout";
      Serial.println("[BLE Handler] ✗ Failed");
    }
    
    #if ENABLE_BLE
    bleComm.notify(response);
    #endif
  } else {
    #if ENABLE_BLE
    bleComm.notify("ERROR|LoRa not initialized");
    #endif
  }
  #else
  #if ENABLE_BLE
  bleComm.notify("ERROR|LoRa disabled");
  #endif
  #endif
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

  // Initialize Scheduler
  Serial.println("[9/9] Scheduler...");
  Serial.println("      ✓ Scheduler ready");
  Serial.printf("      ✓ %d schedules loaded\n", schedules.size());

  // Final status
  Serial.println("\n========================================");
  Serial.println("✓ SETUP COMPLETE");
  Serial.println("========================================");
  Serial.println("LoRa:    " + String(loraInitialized ? "OK" : "FAILED"));
  Serial.println("MQTT:    " + String(mqtt.isConnected() ? "CONNECTED" : "DISCONNECTED"));
  Serial.println("SMS:     " + String(sms.isReady() ? "READY" : "NOT READY"));
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
  Serial.println("  <id> <cmd> - Send LoRa command (e.g., 1 PING)");
  Serial.println("  HELP - Show commands");
  Serial.println();
  
  // Publish boot event (important event - keep this)
  publishStatus("EVT|BOOT|OK|V2.0");
  
  // Send SMS boot notification
  sendSMSNotification("Irrigation Controller v2.0 Started. MQTT: " + 
                      String(mqtt.isConnected() ? "ON" : "OFF") + 
                      ", LoRa: " + String(loraInitialized ? "ON" : "OFF"));
}

// ========== Main Loop ==========
unsigned long lastSchedulerCheck = 0;
unsigned long lastSMSCheck = 0;

void loop() {
  // Process LoRa incoming
  #if ENABLE_LORA
  if (loraInitialized) {
    loraComm.processIncoming();
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

  // Check if SMS needs reconfiguration after modem restart
  // SMS reconfiguration happens independently of MQTT status
  if (sms.needsReconfiguration()) {
    Serial.println("[Main] ⚠ SMS needs reconfiguration, waiting for modem...");
    // Wait for modem to be fully initialized (detected via +QIND: SMS DONE)
    // This typically takes 5-6 seconds after RDY
    delay(6000);
    if (sms.configure()) {
      Serial.println("[Main] ✓ SMS reconfigured successfully");
    } else {
      Serial.println("[Main] ❌ SMS reconfiguration failed");
      // Don't block here - continue with other tasks
    }
  }
  #endif
  
  // Check and process SMS commands periodically
  #if ENABLE_SMS_COMMANDS
  if (millis() - lastSMSCheck > SMS_CHECK_INTERVAL_MS) {  // Configurable interval
    lastSMSCheck = millis();
    processSMSCommands();
  }
  #endif
  
  // ========== Process Serial Commands ==========
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    
    if (line.length() > 0) {
      Serial.println("\n[Serial] ==================");
      Serial.println("[Serial] Input: " + line);
      
      // Check if it's a schedule
      if (line.startsWith("SCH|") || line.startsWith("{")) {
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
            if (loraInitialized) {
              Serial.println("[Serial] Sending via LoRa...");
              bool result = loraComm.sendWithAck(cmd, node, "", 0, 0);
              
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
              Serial.println("[Serial] ✗ LoRa not initialized");
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
  
  // ========== Process Queued Messages ==========
  String msg;
  if (incomingQueue.dequeue(msg)) {
    Serial.println("\n[Queue] ==================");
    Serial.println("[Queue] Processing: " + msg);
    
    // Handle STAT messages from nodes
    if (msg.startsWith("STAT|")) {
      Serial.println("[Queue] ✓✓✓ TELEMETRY ✓✓✓");
      
      int nPos = msg.indexOf("N=");
      if (nPos >= 0) {
        int comma = msg.indexOf(',', nPos);
        String nodeIdStr = msg.substring(nPos + 2, comma > 0 ? comma : msg.length());
        int nodeId = nodeIdStr.toInt();
        
        Serial.printf("[Queue] Node %d Telemetry:\n", nodeId);
        
        // Parse battery
        if (msg.indexOf("BATT=") >= 0) {
          int battPos = msg.indexOf("BATT=");
          int battEnd = msg.indexOf(',', battPos);
          String battStr = msg.substring(battPos + 5, battEnd > 0 ? battEnd : msg.length());
          Serial.println("[Queue]   Battery: " + battStr + "%");
          
          // Publish low battery warning (important event)
          int battPct = battStr.toInt();
          if (battPct < 20) {
            publishStatus("WARN|LOW_BATT|N=" + String(nodeId) + "|BATT=" + battStr);
            // Send SMS alert for low battery (with rate limiting to avoid spam)
            sendSMSNotification("WARN: Low battery on Node " + String(nodeId) +
                                " - " + battStr + "%",
                                "LOW_BATT_N" + String(nodeId));
          }
        }
        
        // Parse battery voltage
        if (msg.indexOf("BV=") >= 0) {
          int bvPos = msg.indexOf("BV=");
          int bvEnd = msg.indexOf(',', bvPos);
          String bvStr = msg.substring(bvPos + 3, bvEnd > 0 ? bvEnd : msg.length());
          Serial.println("[Queue]   Batt Voltage: " + bvStr + "V");
        }
        
        // Parse solar
        if (msg.indexOf("SOLV=") >= 0) {
          int solPos = msg.indexOf("SOLV=");
          int solEnd = msg.indexOf(',', solPos);
          String solStr = msg.substring(solPos + 5, solEnd > 0 ? solEnd : msg.length());
          Serial.println("[Queue]   Solar: " + solStr + "V");
        }
        
        // Parse valve states
        for (int i = 1; i <= 4; i++) {
          String vKey = "V" + String(i) + "=";
          if (msg.indexOf(vKey) >= 0) {
            int vPos = msg.indexOf(vKey);
            int vEnd = msg.indexOf(',', vPos);
            String vStr = msg.substring(vPos + vKey.length(), vEnd > 0 ? vEnd : msg.length());
            Serial.println("[Queue]   Valve " + String(i) + ": " + vStr);
          }
        }
        
        // Parse moisture sensors
        for (int i = 1; i <= 4; i++) {
          String mKey = "M" + String(i) + "=";
          if (msg.indexOf(mKey) >= 0) {
            int mPos = msg.indexOf(mKey);
            int mEnd = msg.indexOf(',', mPos);
            String mStr = msg.substring(mPos + mKey.length(), mEnd > 0 ? mEnd : msg.length());
            Serial.println("[Queue]   Moisture " + String(i) + ": " + mStr + "%");
          }
        }
      }
    }
    // Handle AUTO_CLOSE
    else if (msg.startsWith("AUTO_CLOSE|")) {
      Serial.println("[Queue] ✓✓✓ AUTO_CLOSE ✓✓✓");
      Serial.println("[Queue] " + msg);
      
      // Parse node ID
      int nPos = msg.indexOf("N=");
      String nodeStr = "";
      if (nPos >= 0) {
        int comma = msg.indexOf(',', nPos);
        nodeStr = msg.substring(nPos + 2, comma > 0 ? comma : msg.length());
      }
      
      // Publish auto-close event (important event - keep this)
      publishStatus("EVT|AUTO_CLOSE|N=" + nodeStr);
    }
    // Handle schedules
    else if (msg.indexOf("SCH|") >= 0 || msg.startsWith("{")) {
      Serial.println("[Queue] Schedule message");
      if (scheduleMgr.validateAndLoad(msg)) {
        Serial.println("[Queue] ✓ Schedule loaded");
        // Publish schedule load success (important event - keep this)
        publishStatus("EVT|SCH|LOADED");
        // Send SMS notification
        sendSMSNotification("Schedule loaded successfully");
      } else {
        Serial.println("[Queue] ✗ Schedule invalid");
        // Publish schedule load failure (important event - keep this)
        publishStatus("ERR|SCH|INVALID");
        // Send SMS alert
        sendSMSNotification("ERROR: Invalid schedule format");
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
          sendSMSNotification("Schedule started: " + sch.id);

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
