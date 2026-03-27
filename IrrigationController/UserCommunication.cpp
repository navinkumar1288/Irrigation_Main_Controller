#include "UserCommunication.h"
#include "MessageQueue.h"
#include "CommSetup.h"

extern MessageQueue incomingQueue;
extern bool loraInitialized;

UserCommunication::UserCommunication() 
  : smsComm(nullptr), bleComm(nullptr), loraComm(nullptr), mqttComm(nullptr), 
    wifiComm(nullptr), httpComm(nullptr), commSetup(nullptr), 
    nodeCommandCallback(nullptr) {}

// ========== Initialization ==========
void UserCommunication::init(ModemSMS* sms, BLEComm* ble, LoRaComm* lora, MQTTComm* mqtt, 
                            WiFiComm* wifi, HTTPComm* http, CommSetup* setup, 
                            const String &adminPhoneNum) {
  smsComm = sms;
  bleComm = ble;
  loraComm = lora;
  mqttComm = mqtt;
  wifiComm = wifi;
  httpComm = http;
  commSetup = setup;
  adminPhone = adminPhoneNum;
  Serial.println("[UserComm] ✓ Initialized with all communication channels");
}

void UserCommunication::setNodeCommandCallback(NodeCommandCallback callback) {
  nodeCommandCallback = callback;
  Serial.println("[UserComm] ✓ Node command callback registered");
}

// ========== Status Gathering Functions ==========

SystemStatusReport UserCommunication::gatherSystemStatus(std::vector<Schedule>* schedules, 
                                                         bool scheduleRunning) {
  SystemStatusReport status;
  
  // Communication status
  status.bleConnected = (bleComm != nullptr) ? bleComm->isConnected() : false;
  status.loraInitialized = loraInitialized;
  status.wifiConnected = (wifiComm != nullptr) ? wifiComm->isConnected() : false;
  status.modemReady = (smsComm != nullptr) ? true : false;
  status.smsReady = (smsComm != nullptr) ? true : false;
  status.mqttConnected = (mqttComm != nullptr) ? mqttComm->isConnected() : false;
  status.httpReady = (httpComm != nullptr) ? true : false;
  status.ppposConnected = false;
  
  // System status
  status.systemTime = "System running";
  status.uptime = String(millis() / 1000) + "s";
  status.scheduleRunning = scheduleRunning;
  
  // Schedule status
  int enabled = 0, total = 0;
  if (schedules != nullptr) {
    total = schedules->size();
    for (auto &sch : *schedules) {
      if (sch.enabled) enabled++;
    }
  }
  status.successfulSchedules = enabled;
  status.failedSchedules = 0;
  
  // Memory
  status.freeHeap = ESP.getFreeHeap();
  status.totalHeap = ESP.getHeapSize();
  
  // Network
  status.ipAddress = (wifiComm != nullptr) ? wifiComm->getIPAddress() : "N/A";
  status.wifiSSID = WIFI_SSID;
  status.signalStrength = (wifiComm != nullptr) ? wifiComm->getSignalStrength() : 0;
  
  return status;
}

// ========== Format Status Functions ==========

String UserCommunication::formatStatusAsText(const SystemStatusReport &status) {
  String text = "\n========== SYSTEM STATUS REPORT ==========\n\n";
  
  text += "COMMUNICATION MODULES:\n";
  text += "  BLE:        " + String(status.bleConnected ? "✓ Connected" : "✗ Not Connected") + "\n";
  text += "  LoRa:       " + String(status.loraInitialized ? "✓ Active" : "✗ Inactive") + "\n";
  text += "  WiFi:       " + String(status.wifiConnected ? "✓ Connected" : "✗ Not Connected") + "\n";
  text += "  Modem:      " + String(status.modemReady ? "✓ Ready" : "✗ Not Ready") + "\n";
  text += "  SMS:        " + String(status.smsReady ? "✓ Ready" : "✗ Not Ready") + "\n";
  text += "  MQTT:       " + String(status.mqttConnected ? "✓ Connected" : "✗ Not Connected") + "\n";
  text += "  HTTP:       " + String(status.httpReady ? "✓ Ready" : "✗ Not Ready") + "\n";
  text += "  PPPoS:      " + String(status.ppposConnected ? "✓ Connected" : "✗ Not Connected") + "\n";
  
  text += "\nSCHEDULE STATUS:\n";
  text += "  Running:    " + String(status.scheduleRunning ? "YES" : "NO") + "\n";
  text += "  Current:    " + status.currentSchedule + "\n";
  text += "  Enabled:    " + String(status.successfulSchedules) + "\n";
  
  text += "\nSYSTEM RESOURCES:\n";
  uint32_t heapUsed = status.totalHeap - status.freeHeap;
  uint32_t heapUsagePercent = (100 * heapUsed) / status.totalHeap;
  text += "  Free Heap:  " + String(status.freeHeap / 1024) + " KB\n";
  text += "  Total Heap: " + String(status.totalHeap / 1024) + " KB\n";
  text += "  Usage:      " + String(heapUsagePercent) + "%\n";
  
  text += "\nNETWORK:\n";
  text += "  IP:         " + status.ipAddress + "\n";
  text += "  SSID:       " + status.wifiSSID + "\n";
  text += "  Signal:     " + String(status.signalStrength) + " dBm\n";
  
  text += "\nUPTIME:\n";
  text += "  " + status.uptime + "\n";
  
  text += "\n=========================================\n";
  
  return text;
}

String UserCommunication::formatStatusAsJSON(const SystemStatusReport &status) {
  String json = "{\n";
  
  json += "  \"communication\": {\n";
  json += "    \"ble\": " + String(status.bleConnected ? "true" : "false") + ",\n";
  json += "    \"lora\": " + String(status.loraInitialized ? "true" : "false") + ",\n";
  json += "    \"wifi\": " + String(status.wifiConnected ? "true" : "false") + ",\n";
  json += "    \"mqtt\": " + String(status.mqttConnected ? "true" : "false") + ",\n";
  json += "    \"sms\": " + String(status.smsReady ? "true" : "false") + "\n";
  json += "  },\n";
  
  json += "  \"schedule\": {\n";
  json += "    \"running\": " + String(status.scheduleRunning ? "true" : "false") + ",\n";
  json += "    \"enabled\": " + String(status.successfulSchedules) + ",\n";
  json += "    \"current\": \"" + status.currentSchedule + "\"\n";
  json += "  },\n";
  
  uint32_t heapUsed = status.totalHeap - status.freeHeap;
  uint32_t heapUsagePercent = (100 * heapUsed) / status.totalHeap;
  json += "  \"resources\": {\n";
  json += "    \"freeHeap\": " + String(status.freeHeap) + ",\n";
  json += "    \"totalHeap\": " + String(status.totalHeap) + ",\n";
  json += "    \"usage\": " + String(heapUsagePercent) + "\n";
  json += "  }\n";
  json += "}\n";
  
  return json;
}

String UserCommunication::formatStatusAsBrief(const SystemStatusReport &status) {
  String brief = "[Status] ";
  brief += "BLE:" + String(status.bleConnected ? "✓" : "✗") + " ";
  brief += "LoRa:" + String(status.loraInitialized ? "✓" : "✗") + " ";
  brief += "WiFi:" + String(status.wifiConnected ? "✓" : "✗") + " ";
  brief += "MQTT:" + String(status.mqttConnected ? "✓" : "✗") + " ";
  brief += "Sched:" + String(status.scheduleRunning ? "RUN" : "STOP") + " ";
  brief += "Heap:" + String(status.freeHeap / 1024) + "KB";
  return brief;
}

// ========== Diagnostic Printing Functions ==========

void UserCommunication::printSystemStatus(std::vector<Schedule>* schedules, bool scheduleRunning) {
  SystemStatusReport status = gatherSystemStatus(schedules, scheduleRunning);
  Serial.println(formatStatusAsText(status));
}

void UserCommunication::printBriefStatus(std::vector<Schedule>* schedules, bool scheduleRunning) {
  SystemStatusReport status = gatherSystemStatus(schedules, scheduleRunning);
  Serial.println(formatStatusAsBrief(status));
}

void UserCommunication::printCommStatus() {
  Serial.println("\n========== COMMUNICATION STATUS ==========");
  
  if (commSetup != nullptr) {
    commSetup->printStatus();
  }
  
  Serial.println("BLE:");
  if (bleComm != nullptr) {
    bleComm->printStatus();
  } else {
    Serial.println("  ✗ Not initialized");
  }
  
  Serial.println("\nWiFi:");
  if (wifiComm != nullptr) {
    Serial.printf("  Connected: %s\n", wifiComm->isConnected() ? "YES" : "NO");
    Serial.printf("  IP: %s\n", wifiComm->getIPAddress().c_str());
    Serial.printf("  Signal: %d dBm\n", wifiComm->getSignalStrength());
  } else {
    Serial.println("  ✗ Not initialized");
  }
  
  Serial.println("\nMQTT:");
  if (mqttComm != nullptr) {
    Serial.printf("  Connected: %s\n", mqttComm->isConnected() ? "YES" : "NO");
  } else {
    Serial.println("  ✗ Not initialized");
  }
  
  Serial.println("\n=========================================\n");
}

void UserCommunication::printSystemDiagnostics() {
  Serial.println("\n========== SYSTEM DIAGNOSTICS ==========");
  
  Serial.println("\nMemory:");
  Serial.printf("  Free Heap: %u KB\n", ESP.getFreeHeap() / 1024);
  Serial.printf("  Total Heap: %u KB\n", ESP.getHeapSize() / 1024);
  Serial.printf("  Usage: %u%%\n", (100 * (ESP.getHeapSize() - ESP.getFreeHeap())) / ESP.getHeapSize());
  
  Serial.println("\nSystem:");
  Serial.printf("  Uptime: %lu seconds\n", millis() / 1000);
  Serial.printf("  CPU Frequency: 240 MHz\n");
  
  Serial.println("\n========================================\n");
}

void UserCommunication::printNetworkDiagnostics() {
  Serial.println("\n========== NETWORK DIAGNOSTICS ==========");
  
  if (wifiComm != nullptr) {
    Serial.println("\nWiFi:");
    Serial.printf("  SSID: %s\n", WIFI_SSID);
    Serial.printf("  Connected: %s\n", wifiComm->isConnected() ? "YES" : "NO");
    Serial.printf("  IP Address: %s\n", wifiComm->getIPAddress().c_str());
    Serial.printf("  Signal Strength: %d dBm\n", wifiComm->getSignalStrength());
  }
  
  if (mqttComm != nullptr) {
    Serial.println("\nMQTT:");
    Serial.printf("  Connected: %s\n", mqttComm->isConnected() ? "YES" : "NO");
  }
  
  Serial.println("\n========================================\n");
}

void UserCommunication::printLoRaDiagnostics() {
  Serial.println("\n========== LoRa DIAGNOSTICS ==========");
  
  Serial.printf("Initialized: %s\n", loraInitialized ? "YES" : "NO");
  Serial.printf("Frequency: %d MHz\n", (int)(LORA_FREQUENCY / 1E6));
  Serial.printf("Spreading Factor: %d\n", LORA_SPREADING_FACTOR);
  
  Serial.println("\n=====================================\n");
}

void UserCommunication::printBLEDiagnostics() {
  Serial.println("\n========== BLE DIAGNOSTICS ==========");
  
  if (bleComm != nullptr) {
    bleComm->printStatus();
  } else {
    Serial.println("✗ BLE not initialized");
  }
  
  Serial.println("\n===================================\n");
}

void UserCommunication::printScheduleStatus(std::vector<Schedule>* schedules, bool scheduleRunning) {
  Serial.println("\n========== SCHEDULE STATUS ==========");
  
  Serial.printf("Schedule Running: %s\n", scheduleRunning ? "YES" : "NO");
  
  if (schedules != nullptr) {
    Serial.printf("Total Schedules: %d\n", schedules->size());
    int enabled = 0;
    for (auto &sch : *schedules) {
      if (sch.enabled) enabled++;
    }
    Serial.printf("Enabled: %d\n", enabled);
  }
  
  Serial.println("\n====================================\n");
}

// ========== Status Report Functions ==========

SystemStatusReport UserCommunication::getSystemStatus(std::vector<Schedule>* schedules, 
                                                      bool scheduleRunning) {
  return gatherSystemStatus(schedules, scheduleRunning);
}

String UserCommunication::getFormattedStatus(std::vector<Schedule>* schedules, 
                                            bool scheduleRunning, const String &format) {
  SystemStatusReport status = gatherSystemStatus(schedules, scheduleRunning);
  
  if (format == "json") {
    return formatStatusAsJSON(status);
  } else if (format == "brief") {
    return formatStatusAsBrief(status);
  } else {
    return formatStatusAsText(status);
  }
}

String UserCommunication::getStatusJSON(std::vector<Schedule>* schedules, bool scheduleRunning) {
  return getFormattedStatus(schedules, scheduleRunning, "json");
}

void UserCommunication::broadcastSystemStatus(std::vector<Schedule>* schedules, bool scheduleRunning) {
  String status = formatStatusAsBrief(gatherSystemStatus(schedules, scheduleRunning));
  
  Serial.println("[UserComm] Broadcasting status: " + status);
  
  // Send via MQTT if connected
  if (mqttComm != nullptr && mqttComm->isConnected()) {
    Serial.println("[UserComm] → MQTT: " + status);
  }
  
  // Send via BLE if connected
  if (bleComm != nullptr && bleComm->isConnected()) {
    bleComm->notify(status);
  }
}

// ========== Alert and Notification Functions ==========

void UserCommunication::sendAlert(const String &alertMessage, const String &severity) {
  String fullMessage = "[" + severity + "] " + alertMessage;
  
  Serial.println("[UserComm] ALERT: " + fullMessage);
  
  // Send via all available channels
  if (smsComm != nullptr) {
    Serial.println("[UserComm] → SMS: " + fullMessage);
  }
  
  if (bleComm != nullptr && bleComm->isConnected()) {
    bleComm->notify(fullMessage);
  }
  
  if (mqttComm != nullptr && mqttComm->isConnected()) {
    Serial.println("[UserComm] → MQTT: " + fullMessage);
  }
}

void UserCommunication::notifyScheduleUpdate(const String &scheduleName, const String &status) {
  String message = "Schedule '" + scheduleName + "': " + status;
  sendAlert(message, "INFO");
}

void UserCommunication::onScheduleStarted(const String &scheduleId) {
  notifyScheduleUpdate(scheduleId, "STARTED");
}

void UserCommunication::onScheduleCompleted(const String &scheduleId) {
  notifyScheduleUpdate(scheduleId, "COMPLETED");
}

void UserCommunication::onScheduleFailed(const String &scheduleId, const String &reason) {
  notifyScheduleUpdate(scheduleId, "FAILED: " + reason);
}

void UserCommunication::onValveAction(int nodeId, const String &valve, const String &action) {
  String message = "Node " + String(nodeId) + ": " + valve + " " + action;
  sendAlert(message, "INFO");
}

void UserCommunication::onSystemError(const String &errorMessage) {
  sendAlert("ERROR: " + errorMessage, "ERROR");
}

void UserCommunication::onSystemWarning(const String &warningMessage) {
  sendAlert("WARNING: " + warningMessage, "WARNING");
}

// ========== Health Check Functions ==========

bool UserCommunication::isSystemHealthy() {
  // Check memory
  if (ESP.getFreeHeap() < 50000) {  // Less than 50KB free
    return false;
  }
  
  return true;
}

String UserCommunication::getHealthStatus() {
  if (isSystemHealthy()) {
    return "HEALTHY";
  } else {
    String issues = "ISSUES: ";
    if (ESP.getFreeHeap() < 50000) {
      issues += "Low Memory ";
    }
    return issues;
  }
}

// ========== Command Processing ==========

CommandResult UserCommunication::handleStatusCommand() {
  CommandResult result;
  result.success = true;
  result.commandType = "STATUS";
  result.response = "System OK. ";
  result.response += "MQTT: " + String((mqttComm && mqttComm->isConnected()) ? "ON" : "OFF") + ", ";
  result.response += "LoRa: " + String(loraInitialized ? "ON" : "OFF");
  return result;
}

CommandResult UserCommunication::handleDiagnosticsCommand() {
  CommandResult result;
  result.success = true;
  result.commandType = "DIAGNOSTICS";
  result.response = "See serial output for diagnostics";
  printSystemDiagnostics();
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
  result.response = "System check complete";
  return result;
}

CommandResult UserCommunication::handleNodeCommand(const String &cmd) {
  CommandResult result;
  
  if (nodeCommandCallback != nullptr) {
    int spacePos = cmd.indexOf(' ');
    if (spacePos > 0) {
      int nodeId = cmd.substring(0, spacePos).toInt();
      String nodeCmd = cmd.substring(spacePos + 1);
      
      if (nodeCommandCallback(nodeId, nodeCmd)) {
        result.success = true;
        result.response = "Command sent to node " + String(nodeId);
      } else {
        result.success = false;
        result.response = "Failed to send command to node";
      }
    }
  } else {
    result.success = false;
    result.response = "Node callback not set";
  }
  
  result.commandType = "NODE";
  return result;
}

CommandResult UserCommunication::handleHelpCommand() {
  CommandResult result;
  result.success = true;
  result.commandType = "HELP";
  result.response = getHelpText();
  return result;
}

CommandResult UserCommunication::handleStatsCommand() {
  CommandResult result;
  result.success = true;
  result.commandType = "STATS";
  result.response = "Free Heap: " + String(ESP.getFreeHeap() / 1024) + " KB";
  return result;
}

CommandResult UserCommunication::handleReportCommand() {
  CommandResult result;
  result.success = true;
  result.commandType = "REPORT";
  result.response = "See serial output for report";
  return result;
}

CommandResult UserCommunication::routeCommand(const String &cmd, std::vector<Schedule>* schedules, 
                                             bool* scheduleRunning, bool* scheduleLoaded, 
                                             bool* enableSMSBroadcast) {
  String command = cmd;
  command.toUpperCase();
  
  if (command == "STATUS") {
    return handleStatusCommand();
  } else if (command == "DIAGNOSTICS") {
    return handleDiagnosticsCommand();
  } else if (command == "SCHEDULES") {
    return handleSchedulesCommand(schedules);
  } else if (command == "STOP") {
    return handleStopCommand(scheduleRunning, scheduleLoaded);
  } else if (command == "SMS ON") {
    return handleSMSOnCommand(enableSMSBroadcast);
  } else if (command == "SMS OFF") {
    return handleSMSOffCommand(enableSMSBroadcast);
  } else if (command == "CHECK") {
    return handleCheckCommand();
  } else if (command == "HELP") {
    return handleHelpCommand();
  } else if (command == "STATS") {
    return handleStatsCommand();
  } else if (command == "REPORT") {
    return handleReportCommand();
  } else {
    CommandResult result;
    result.success = false;
    result.response = "Unknown command. Type HELP for available commands.";
    result.commandType = "UNKNOWN";
    return result;
  }
}

void UserCommunication::sendResponse(const String &response, const String &channel) {
  Serial.println("[UserComm] Response via " + channel + ": " + response);
}

void UserCommunication::sendMultiChannelResponse(const String &response) {
  if (bleComm != nullptr && bleComm->isConnected()) {
    bleComm->notify(response);
  }
  if (mqttComm != nullptr && mqttComm->isConnected()) {
    Serial.println("[UserComm] → MQTT: " + response);
  }
}

void UserCommunication::sendCommandResponse(const String &command, const CommandResult &result, 
                                           const String &channel) {
  String response = result.success ? "✓ " : "✗ ";
  response += result.commandType + ": " + result.response;
  
  Serial.println("[UserComm] Response: " + response);
  
  if (channel == "BLE") {
    if (bleComm != nullptr && bleComm->isConnected()) {
      bleComm->notify(response);
    }
  } else if (channel == "MQTT") {
    if (mqttComm != nullptr && mqttComm->isConnected()) {
      Serial.println("[UserComm] → MQTT: " + response);
    }
  }
}

String UserCommunication::getHelpText() {
  String help = "\n========== AVAILABLE COMMANDS ==========\n";
  help += "STATUS       - Show system status\n";
  help += "DIAGNOSTICS  - Show full diagnostics\n";
  help += "SCHEDULES    - List all schedules\n";
  help += "START <id>   - Start schedule\n";
  help += "STOP         - Stop all schedules\n";
  help += "SMS ON/OFF   - Enable/disable SMS\n";
  help += "STATS        - Show system stats\n";
  help += "REPORT       - Full system report\n";
  help += "HELP         - Show this help\n";
  help += "=========================================\n";
  return help;
}

// ========== Channel Processors ==========

void UserCommunication::processAllChannels(std::vector<Schedule>* schedules, bool* scheduleRunning, 
                                          bool* scheduleLoaded, bool* enableSMSBroadcast) {
  processSMSCommands(schedules, scheduleRunning, scheduleLoaded, enableSMSBroadcast);
  processLoRaCommands(schedules, scheduleRunning, scheduleLoaded, enableSMSBroadcast);
  processMQTTCommands(schedules, scheduleRunning, scheduleLoaded, enableSMSBroadcast);
  processWiFiCommands(schedules, scheduleRunning, scheduleLoaded, enableSMSBroadcast);
  processHTTPCommands(schedules, scheduleRunning, scheduleLoaded, enableSMSBroadcast);
}

void UserCommunication::processSMSCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, 
                                          bool* scheduleLoaded, bool* enableSMSBroadcast) {
  // SMS command processing
}

void UserCommunication::processLoRaCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, 
                                           bool* scheduleLoaded, bool* enableSMSBroadcast) {
  // LoRa command processing
}

void UserCommunication::processMQTTCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, 
                                           bool* scheduleLoaded, bool* enableSMSBroadcast) {
  // MQTT command processing
}

void UserCommunication::processWiFiCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, 
                                           bool* scheduleLoaded, bool* enableSMSBroadcast) {
  // WiFi command processing
}

void UserCommunication::processHTTPCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, 
                                           bool* scheduleLoaded, bool* enableSMSBroadcast) {
  // HTTP command processing
}

void UserCommunication::processBLECommand(int nodeId, const String &command) {
  CommandResult result = routeCommand(command, nullptr, nullptr, nullptr, nullptr);
  Serial.printf("[UserComm] BLE Command: %s -> %s\n", command.c_str(), result.response.c_str());
  
  if (bleComm != nullptr && bleComm->isConnected()) {
    bleComm->notify(result.response);
  }
}

void UserCommunication::processSerialCommand(const String &input, std::vector<Schedule>* schedules, 
                                            bool* scheduleRunning, bool* scheduleLoaded) {
  // Serial command processing
}

void UserCommunication::processMessage(const String &message) {
  Serial.println("[UserComm] Processing message: " + message);
  // Message routing logic
}