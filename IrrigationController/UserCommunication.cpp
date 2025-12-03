// UserCommunication.cpp - Handles all user communication (SMS, BLE, LoRa, MQTT, WiFi, HTTP, Serial)
#include "UserCommunication.h"
#include "MessageQueue.h"

extern MessageQueue incomingQueue;
extern bool loraInitialized;

UserCommunication::UserCommunication() : smsComm(nullptr), bleComm(nullptr), loraComm(nullptr), mqttComm(nullptr), wifiComm(nullptr), httpComm(nullptr), nodeCommandCallback(nullptr) {}

void UserCommunication::init(ModemSMS* sms, BLEComm* ble, LoRaComm* lora, MQTTComm* mqtt, WiFiComm* wifi, HTTPComm* http, const String &adminPhoneNum) {
  smsComm = sms;
  bleComm = ble;
  loraComm = lora;
  mqttComm = mqtt;
  wifiComm = wifi;
  httpComm = http;
  adminPhone = adminPhoneNum;
  Serial.println("[UserComm] ✓ Initialized with all channels (SMS, BLE, LoRa, MQTT, WiFi, HTTP)");
}

void UserCommunication::setNodeCommandCallback(NodeCommandCallback callback) {
  nodeCommandCallback = callback;
  Serial.println("[UserComm] ✓ Node command callback set");
}

// ========== Command Handlers ==========

CommandResult UserCommunication::handleStatusCommand() {
  CommandResult result;
  result.success = true;
  result.commandType = "STATUS";
  result.response = "System OK. ";
  result.response += "MQTT: " + String((mqttComm && mqttComm->isConnected()) ? "ON" : "OFF") + ", ";
  result.response += "LoRa: " + String(loraInitialized ? "ON" : "OFF");
  return result;
}

CommandResult UserCommunication::handleSchedulesCommand(std::vector<Schedule>* schedules) {
  CommandResult result;
  result.success = true;
  result.commandType = "SCHEDULES";
  result.response = "Schedules: ";

  int enabledCount = 0;
  if (schedules != nullptr) {
    for (auto &sch : *schedules) {
      if (sch.enabled) enabledCount++;
    }
    result.response += String(enabledCount) + "/" + String(schedules->size()) + " enabled";
  } else {
    result.response += "0/0 enabled";
  }

  return result;
}

CommandResult UserCommunication::handleStopCommand(bool* scheduleRunning, bool* scheduleLoaded) {
  CommandResult result;
  result.success = true;
  result.commandType = "STOP";
  result.response = "All schedules stopped";

  if (scheduleRunning != nullptr) *scheduleRunning = false;
  if (scheduleLoaded != nullptr) *scheduleLoaded = false;

  return result;
}

CommandResult UserCommunication::handleStartCommand(const String &schedId) {
  CommandResult result;
  result.success = true;
  result.commandType = "START";
  result.response = "Starting schedule: " + schedId;
  // Trigger schedule logic would go here
  return result;
}

CommandResult UserCommunication::handleSMSOnCommand(bool* enableSMSBroadcast) {
  CommandResult result;
  result.success = true;
  result.commandType = "SMS_ON";
  result.response = "SMS alerts enabled";

  if (enableSMSBroadcast != nullptr) *enableSMSBroadcast = true;

  return result;
}

CommandResult UserCommunication::handleSMSOffCommand(bool* enableSMSBroadcast) {
  CommandResult result;
  result.success = true;
  result.commandType = "SMS_OFF";
  result.response = "SMS alerts disabled";

  if (enableSMSBroadcast != nullptr) *enableSMSBroadcast = false;

  return result;
}

CommandResult UserCommunication::handleCheckCommand() {
  CommandResult result;
  result.success = true;
  result.commandType = "CHECK";
  result.response = "Scanning complete. Check logs.";

  if (smsComm != nullptr && smsComm->isReady()) {
    smsComm->scanForNewMessages();
    smsComm->printSMSDiagnostics();
  }

  return result;
}

CommandResult UserCommunication::handleNodeCommand(const String &cmd) {
  CommandResult result;
  result.commandType = "NODE";

  // Parse node command: "NODE 1 PING" or "1 PING"
  int nodeId = 0;
  String nodeCmd = "";

  if (cmd.startsWith("NODE ")) {
    int space1 = cmd.indexOf(' ', 5);
    if (space1 > 0) {
      String nodeStr = cmd.substring(5, space1);
      nodeCmd = cmd.substring(space1 + 1);
      nodeId = nodeStr.toInt();
    }
  } else {
    int space1 = cmd.indexOf(' ');
    if (space1 > 0) {
      String nodeStr = cmd.substring(0, space1);
      nodeCmd = cmd.substring(space1 + 1);
      nodeId = nodeStr.toInt();
    }
  }

  // Parse and validate
  if (nodeId > 0 && nodeId <= 255 && nodeCmd.length() > 0) {
    // Call business logic callback (set by .ino file)
    if (nodeCommandCallback) {
      Serial.println("[UserComm] Requesting node command: Node " + String(nodeId) + ", Cmd: " + nodeCmd);
      bool success = nodeCommandCallback(nodeId, nodeCmd);

      result.success = success;
      if (success) {
        result.response = "Node " + String(nodeId) + " OK: " + nodeCmd;
      } else {
        result.response = "Node " + String(nodeId) + " TIMEOUT";
      }
    } else {
      result.success = false;
      result.response = "Node commands not available";
      Serial.println("[UserComm] ⚠ No node command callback set");
    }
  } else {
    result.success = false;
    result.response = "Format: <id> <cmd> OR NODE <id> <cmd>";
  }

  return result;
}

CommandResult UserCommunication::handleHelpCommand() {
  CommandResult result;
  result.success = true;
  result.commandType = "HELP";
  result.response = "Commands: STATUS, SCHEDULES, STOP, SMS ON/OFF, CHECK, <id> <cmd> (e.g., 1 PING), HELP";
  return result;
}

// ========== Command Routing ==========

CommandResult UserCommunication::routeCommand(const String &cmdInput, std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded, bool* enableSMSBroadcast) {
  String cmd = cmdInput;
  cmd.trim();
  cmd.toUpperCase();

  CommandResult result;

  // Route command to appropriate handler
  if (cmd == "STATUS") {
    result = handleStatusCommand();
  } else if (cmd == "SCHEDULES") {
    result = handleSchedulesCommand(schedules);
  } else if (cmd.startsWith("START ")) {
    String schedId = cmd.substring(6);
    schedId.trim();
    result = handleStartCommand(schedId);
  } else if (cmd == "STOP") {
    result = handleStopCommand(scheduleRunning, scheduleLoaded);
  } else if (cmd == "SMS ON") {
    result = handleSMSOnCommand(enableSMSBroadcast);
  } else if (cmd == "SMS OFF") {
    result = handleSMSOffCommand(enableSMSBroadcast);
  } else if (cmd == "CHECK" || cmd == "REFRESH") {
    result = handleCheckCommand();
  } else if (cmd.startsWith("NODE ") || (cmd.length() > 0 && isdigit(cmd.charAt(0)))) {
    result = handleNodeCommand(cmd);
  } else if (cmd == "HELP") {
    result = handleHelpCommand();
  } else {
    result.success = false;
    result.response = "Unknown command. Send HELP for list.";
    result.commandType = "UNKNOWN";
  }

  return result;
}

// ========== Unified Channel Processing ==========

void UserCommunication::processAllChannels(std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded, bool* enableSMSBroadcast) {
  // Process all enabled communication channels
  #if ENABLE_SMS_COMMANDS
  processSMSCommands(schedules, scheduleRunning, scheduleLoaded, enableSMSBroadcast);
  #endif

  #if ENABLE_LORA_COMMANDS
  processLoRaCommands(schedules, scheduleRunning, scheduleLoaded, enableSMSBroadcast);
  #endif

  #if ENABLE_MQTT
  processMQTTCommands(schedules, scheduleRunning, scheduleLoaded, enableSMSBroadcast);
  #endif

  #if ENABLE_WIFI_COMMANDS
  processWiFiCommands(schedules, scheduleRunning, scheduleLoaded, enableSMSBroadcast);
  #endif

  #if ENABLE_HTTP_COMMANDS
  processHTTPCommands(schedules, scheduleRunning, scheduleLoaded, enableSMSBroadcast);
  #endif
}

// ========== Process SMS Commands ==========

void UserCommunication::processSMSCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded, bool* enableSMSBroadcast) {
  #if ENABLE_SMS_COMMANDS
  if (smsComm == nullptr || !smsComm->isReady()) {
    return;
  }

  // Process incoming messages (network messages handled automatically)
  std::vector<SMSMessage> commandMessages = smsComm->processIncomingMessages(adminPhone);

  // Process each command message
  for (SMSMessage &msg : commandMessages) {
    Serial.println("\n[UserComm:SMS] ==================");
    Serial.println("[UserComm:SMS] From: " + msg.sender);
    Serial.println("[UserComm:SMS] Command: " + msg.message);

    // Route command through unified handler
    CommandResult result = routeCommand(msg.message, schedules, scheduleRunning, scheduleLoaded, enableSMSBroadcast);

    // Send response
    if (result.response.length() > 0) {
      smsComm->sendSMS(msg.sender, result.response);
      Serial.println("[UserComm:SMS] Response: " + result.response);
    }

    // Delete processed message
    smsComm->deleteSMS(msg.index);

    Serial.println("[UserComm:SMS] ==================\n");
  }
  #endif
}

// ========== Process LoRa Commands ==========

void UserCommunication::processLoRaCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded, bool* enableSMSBroadcast) {
  #if ENABLE_LORA
  if (loraComm == nullptr || !loraInitialized) {
    return;
  }

  // Check incoming LoRa queue for user commands (not node messages)
  // User commands are typically single-line commands from a LoRa device
  String msg;
  while (incomingQueue.dequeue(msg)) {
    // Check if this is a user command (not a node message)
    // Node messages start with "STAT|" or "AUTO_CLOSE|" - skip those, they're handled by NodeCommunication
    if (msg.startsWith("STAT|") || msg.startsWith("AUTO_CLOSE|")) {
      // Put it back for NodeCommunication to process
      incomingQueue.enqueue(msg);
      break;  // Stop processing, let NodeCommunication handle it
    }

    // Check if it's a schedule message
    if (msg.indexOf("SCH|") >= 0 || msg.startsWith("{")) {
      Serial.println("\n[UserComm:LoRa] ==================");
      Serial.println("[UserComm:LoRa] Schedule received via LoRa");
      Serial.println("[UserComm:LoRa] " + msg);
      // Schedule handling would go here - for now just log
      Serial.println("[UserComm:LoRa] ==================\n");
      continue;
    }

    // Otherwise, treat it as a user command
    Serial.println("\n[UserComm:LoRa] ==================");
    Serial.println("[UserComm:LoRa] Command: " + msg);

    // Route command through unified handler
    CommandResult result = routeCommand(msg, schedules, scheduleRunning, scheduleLoaded, enableSMSBroadcast);

    Serial.println("[UserComm:LoRa] Result: " + result.response);
    Serial.println("[UserComm:LoRa] ==================\n");

    // Note: No response sent back via LoRa for now (could be added if needed)
  }
  #endif
}

// ========== Process MQTT Commands ==========

void UserCommunication::processMQTTCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded, bool* enableSMSBroadcast) {
  #if ENABLE_MQTT
  if (mqttComm == nullptr || !mqttComm->isConnected()) {
    return;
  }

  // Check if there are any MQTT commands (implementation would depend on how MQTT queues messages)
  // For now, this is a placeholder as MQTT typically uses callbacks
  // The actual implementation would process messages from an MQTT command queue
  #endif
}

// ========== Process WiFi Commands ==========

void UserCommunication::processWiFiCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded, bool* enableSMSBroadcast) {
  #if ENABLE_WIFI_COMMANDS
  if (wifiComm == nullptr || !wifiComm->isReady()) {
    return;
  }

  // Check if there are any WiFi commands
  if (!wifiComm->hasCommands()) {
    return;
  }

  // Get all pending commands
  std::vector<WiFiCommand> commands = wifiComm->getCommands();

  // Process each command
  for (const WiFiCommand &cmd : commands) {
    Serial.println("\n[UserComm:WiFi] ==================");
    Serial.println("[UserComm:WiFi] From: " + cmd.source);
    Serial.println("[UserComm:WiFi] Command: " + cmd.command);

    // Route command through unified handler
    CommandResult result = routeCommand(cmd.command, schedules, scheduleRunning, scheduleLoaded, enableSMSBroadcast);

    // Log result
    if (result.success) {
      Serial.println("[UserComm:WiFi] ✓ Success: " + result.response);
    } else {
      Serial.println("[UserComm:WiFi] ✗ Failed: " + result.response);
    }

    Serial.println("[UserComm:WiFi] ==================\n");
  }

  // Clear processed commands
  wifiComm->clearCommands();
  #endif
}

// ========== Process HTTP Commands ==========

void UserCommunication::processHTTPCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded, bool* enableSMSBroadcast) {
  #if ENABLE_HTTP_COMMANDS
  if (httpComm == nullptr || !httpComm->isReady()) {
    return;
  }

  // Check if there are any HTTP commands
  if (!httpComm->hasCommands()) {
    return;
  }

  // Get all pending commands
  std::vector<HTTPCommand> commands = httpComm->getCommands();

  // Process each command
  for (const HTTPCommand &cmd : commands) {
    Serial.println("\n[UserComm:HTTP] ==================");
    Serial.println("[UserComm:HTTP] From: " + cmd.source);
    Serial.println("[UserComm:HTTP] Command: " + cmd.command);

    // Route command through unified handler
    CommandResult result = routeCommand(cmd.command, schedules, scheduleRunning, scheduleLoaded, enableSMSBroadcast);

    // Log result (HTTP response already sent in HTTPComm::handleCommand)
    if (result.success) {
      Serial.println("[UserComm:HTTP] ✓ Success: " + result.response);
    } else {
      Serial.println("[UserComm:HTTP] ✗ Failed: " + result.response);
    }

    Serial.println("[UserComm:HTTP] ==================\n");
  }

  // Clear processed commands
  httpComm->clearCommands();
  #endif
}

// ========== Process BLE Commands ==========

void UserCommunication::processBLECommand(int nodeId, const String &command) {
  Serial.println("[UserComm:BLE] Node=" + String(nodeId) + ", Command=" + command);

  if (nodeCommandCallback) {
    bool result = nodeCommandCallback(nodeId, command);

    String response;
    if (result) {
      response = "OK|Node " + String(nodeId) + " responded";
    } else {
      response = "FAIL|Node " + String(nodeId) + " timeout";
    }

    sendBLENotification(response);
  } else {
    sendBLENotification("ERROR|Node commands not available");
    Serial.println("[UserComm:BLE] ⚠ No node command callback set");
  }
}

// ========== Process Serial Commands ==========

void UserCommunication::processSerialCommand(const String &input, std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded) {
  Serial.println("\n[UserComm:Serial] ==================");
  Serial.println("[UserComm:Serial] Input: " + input);

  // Check for node command format: <node> <command>
  int space = input.indexOf(' ');
  if (space > 0) {
    int nodeId = input.substring(0, space).toInt();
    String cmd = input.substring(space + 1);
    cmd.toUpperCase();
    cmd.trim();

    if (nodeId > 0 && nodeId <= 255 && cmd.length() > 0) {
      Serial.println("[UserComm:Serial] Node: " + String(nodeId) + ", Command: " + cmd);

      if (nodeCommandCallback) {
        bool result = nodeCommandCallback(nodeId, cmd);

        if (result) {
          Serial.println("[UserComm:Serial] ✓✓✓ SUCCESS ✓✓✓");
        } else {
          Serial.println("[UserComm:Serial] ✗✗✗ FAILED ✗✗✗");
        }
      } else {
        Serial.println("[UserComm:Serial] ✗ Node commands not available");
      }
    } else {
      Serial.println("[UserComm:Serial] ✗ Invalid format");
      Serial.println("[UserComm:Serial] Use: <node> <command>");
      Serial.println("[UserComm:Serial] Example: 1 PING");
    }
  } else {
    Serial.println("[UserComm:Serial] ✗ Invalid format");
    Serial.println("[UserComm:Serial] Use: <node> <command>");
  }

  Serial.println("[UserComm:Serial] ==================\n");
}

// ========== Publish Status ==========
// NOTE: Use publishStatus() for all notifications and status updates
// This method handles compact message formatting for SMS/cellular data efficiency

// Publish status to all available channels (MQTT/SMS/BLE)
void UserCommunication::publishStatus(const String &msg) {
  Serial.println("[Status] " + msg);

  #if ENABLE_MQTT
  // MQTT enabled - publish to MQTT
  if (mqttComm != nullptr && mqttComm->isConnected()) {
    mqttComm->publish(MQTT_TOPIC_STATUS, msg);
    Serial.println("[Status] → Published to MQTT");
  }
  #elif ENABLE_SMS
  // MQTT disabled, SMS enabled - send important status via SMS
  // Only send critical events to avoid SMS flooding
  if (msg.indexOf("EVT|") >= 0 || msg.indexOf("BOOT") >= 0 ||
      msg.indexOf("ERROR") >= 0 || msg.indexOf("FAIL") >= 0) {
    sendNotification("Status: " + msg, "");
    Serial.println("[Status] → Sent via SMS (MQTT disabled)");
  }
  #endif

  #if ENABLE_BLE
  if (bleComm != nullptr && bleComm->isConnected()) {
    bleComm->notify("STAT|" + msg);
  }
  #endif
}

// ========== Process Serial Input ==========

void UserCommunication::processSerialInput(std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded) {
  if (!Serial.available()) {
    return;
  }

  String line = Serial.readStringUntil('\n');
  line.trim();

  if (line.length() == 0) {
    return;
  }

  Serial.println("\n[Serial] ==================");
  Serial.println("[Serial] Input: " + line);

  // Check for special SMS diagnostic command
  if (line.equalsIgnoreCase("SMSDIAG") || line.equalsIgnoreCase("SMS DIAG")) {
    Serial.println("[Serial] Running SMS diagnostics...");
    #if ENABLE_SMS
    if (smsComm != nullptr) {
      smsComm->printSMSDiagnostics();
      Serial.println("\n[Serial] Forcing message scan...");
      smsComm->scanForNewMessages();
    }
    #else
    Serial.println("[Serial] SMS is disabled");
    #endif
  }
  // Delete all messages (useful for clearing old PDU messages)
  else if (line.equalsIgnoreCase("SMSCLEAN") || line.equalsIgnoreCase("SMS CLEAN")) {
    Serial.println("[Serial] Deleting all SMS messages...");
    #if ENABLE_SMS
    if (smsComm != nullptr && smsComm->deleteAllSMS()) {
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
    if (smsComm != nullptr && smsComm->configure()) {
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

    // Queue to incoming message queue
    extern MessageQueue incomingQueue;
    incomingQueue.enqueue(line);
  }
  // It's a simple command: <node> <command> - process as node command
  else {
    processSerialCommand(line, schedules, scheduleRunning, scheduleLoaded);
  }

  Serial.println("[Serial] ==================\n");
}

// ========== Process Background Tasks ==========

void UserCommunication::processBackground() {
  // Process MQTT background (handles auto-reconnect, URCs)
  #if ENABLE_MQTT
  if (mqttComm != nullptr) {
    // Process MQTT background tasks (handles auto-reconnection)
    mqttComm->processBackground();
  }
  #endif

  // Process SMS background (handles new messages, URCs)
  #if ENABLE_SMS
  if (smsComm != nullptr) {
    smsComm->processBackground();

    // Auto-reconfigure SMS if modem restarted
    // This is simple: if SMS becomes not ready, reconfigure it
    if (!smsComm->isReady()) {
      static unsigned long lastReconfigAttempt = 0;
      // Only try once per 5 seconds to avoid spam
      if (millis() - lastReconfigAttempt > 5000) {
        lastReconfigAttempt = millis();
        Serial.println("[UserComm] ⚠ SMS not ready - attempting reconfiguration...");
        if (smsComm->configure()) {
          Serial.println("[UserComm] ✓ SMS reconfigured successfully");
        } else {
          Serial.println("[UserComm] ❌ SMS reconfiguration failed (will retry)");
        }
      }
    }
  }
  #endif

  // Periodically scan for messages (bypasses URC system)
  // This is a workaround if +CMTI URCs are not being received
  #if ENABLE_SMS
  if (smsComm != nullptr) {
    static unsigned long lastMessageScan = 0;
    if (millis() - lastMessageScan > 30000) {  // Every 30 seconds
      lastMessageScan = millis();
      if (smsComm->isReady()) {
        Serial.println("[UserComm] → Periodic message scan (URC bypass)");
        smsComm->scanForNewMessages();
      }
    }
  }
  #endif

  // Process WiFi background (handles reconnection, status checks)
  #if ENABLE_WIFI
  if (wifiComm != nullptr) {
    wifiComm->processBackground();
  }
  #endif

  // Process HTTP background (handles incoming requests)
  #if ENABLE_HTTP
  if (httpComm != nullptr && httpComm->isReady()) {
    httpComm->processBackground();
  }
  #endif
}
