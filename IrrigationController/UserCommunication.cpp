// UserCommunication.cpp - Handles all user communication (SMS, BLE, Serial)
#include "UserCommunication.h"
#include "ModemMQTT.h"

extern ModemMQTT mqtt;
extern bool loraInitialized;

UserCommunication::UserCommunication() : smsComm(nullptr), bleComm(nullptr), nodeComm(nullptr) {}

void UserCommunication::init(ModemSMS* sms, BLEComm* ble, NodeCommunication* node, const String &adminPhoneNum) {
  smsComm = sms;
  bleComm = ble;
  nodeComm = node;
  adminPhone = adminPhoneNum;
  Serial.println("[UserComm] ✓ Initialized");
}

// ========== Command Handlers ==========

CommandResult UserCommunication::handleStatusCommand() {
  CommandResult result;
  result.success = true;
  result.commandType = "STATUS";
  result.response = "System OK. ";
  result.response += "MQTT: " + String(mqtt.isConnected() ? "ON" : "OFF") + ", ";
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

  // Execute command
  if (nodeId > 0 && nodeId <= 255 && nodeCmd.length() > 0) {
    if (nodeComm != nullptr && nodeComm->isInitialized()) {
      Serial.println("[UserComm] Sending to Node " + String(nodeId) + ": " + nodeCmd);
      bool success = nodeComm->sendCommand(nodeId, nodeCmd);

      result.success = success;
      if (success) {
        result.response = "Node " + String(nodeId) + " OK: " + nodeCmd;
      } else {
        result.response = "Node " + String(nodeId) + " TIMEOUT";
      }
    } else {
      result.success = false;
      result.response = "LoRa not available";
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

    String cmd = msg.message;
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

// ========== Process BLE Commands ==========

void UserCommunication::processBLECommand(int nodeId, const String &command) {
  Serial.println("[UserComm:BLE] Node=" + String(nodeId) + ", Command=" + command);

  if (nodeComm != nullptr && nodeComm->isInitialized()) {
    bool result = nodeComm->sendCommand(nodeId, command);

    String response;
    if (result) {
      response = "OK|Node " + String(nodeId) + " responded";
    } else {
      response = "FAIL|Node " + String(nodeId) + " timeout";
    }

    sendBLENotification(response);
  } else {
    sendBLENotification("ERROR|LoRa not initialized");
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

      if (nodeComm != nullptr && nodeComm->isInitialized()) {
        bool result = nodeComm->sendCommand(nodeId, cmd);

        if (result) {
          Serial.println("[UserComm:Serial] ✓✓✓ SUCCESS ✓✓✓");
        } else {
          Serial.println("[UserComm:Serial] ✗✗✗ FAILED ✗✗✗");
        }
      } else {
        Serial.println("[UserComm:Serial] ✗ LoRa not initialized");
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

// ========== Send Notifications ==========

void UserCommunication::sendNotification(const String &message, const String &channel) {
  if (channel == "sms" || channel == "all") {
    sendSMSNotification(message);
  }

  if (channel == "ble" || channel == "all") {
    sendBLENotification(message);
  }
}

void UserCommunication::sendSMSNotification(const String &message) {
  #if ENABLE_SMS_ALERTS
  if (smsComm != nullptr && smsComm->isReady() && adminPhone.length() > 0) {
    smsComm->sendSMS(adminPhone, message);
    Serial.println("[UserComm] SMS sent to: " + adminPhone);
  }
  #endif
}

void UserCommunication::sendBLENotification(const String &message) {
  #if ENABLE_BLE
  if (bleComm != nullptr && bleComm->isConnected()) {
    bleComm->notify(message);
    Serial.println("[UserComm] BLE notification sent");
  }
  #endif
}
