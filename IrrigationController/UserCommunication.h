// UserCommunication.h - Centralized user communication and diagnostics
// Channels: SMS, BLE, LoRa, MQTT, WiFi, HTTP, Serial
// Diagnostics: Status reporting, health monitoring, event notifications

#ifndef USER_COMMUNICATION_H
#define USER_COMMUNICATION_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include "Config.h"
#include "ModemSMS.h"
#include "BLEComm.h"
#include "LoRaComm.h"
#include "MQTTComm.h"
#include "WiFiComm.h"
#include "HTTPComm.h"

// Forward declarations
class ScheduleManager;
class CommSetup;
struct Schedule;

// Command result structure
struct CommandResult {
  bool success;
  String response;
  String commandType;
};

// System status structure
struct SystemStatusReport {
  // Communication Modules
  bool bleConnected;
  bool loraInitialized;
  bool wifiConnected;
  bool modemReady;
  bool smsReady;
  bool mqttConnected;
  bool httpReady;
  bool ppposConnected;
  
  // System Status
  String systemTime;
  String uptime;
  int successfulSchedules;
  int failedSchedules;
  bool scheduleRunning;
  String currentSchedule;
  
  // Memory
  uint32_t freeHeap;
  uint32_t totalHeap;
  
  // Network
  String ipAddress;
  String wifiSSID;
  int signalStrength;
};

// Node command callback
typedef std::function<bool(int nodeId, const String& command)> NodeCommandCallback;

/**
 * UserCommunication Class
 * 
 * Centralized manager for:
 * - All user communication channels
 * - System diagnostics and status reporting
 * - User-friendly notifications
 * - Command processing and routing
 */
class UserCommunication {
private:
  ModemSMS* smsComm;
  BLEComm* bleComm;
  LoRaComm* loraComm;
  MQTTComm* mqttComm;
  WiFiComm* wifiComm;
  HTTPComm* httpComm;
  CommSetup* commSetup;
  NodeCommandCallback nodeCommandCallback;
  String adminPhone;

  // Status gathering helpers
  SystemStatusReport gatherSystemStatus(std::vector<Schedule>* schedules, bool scheduleRunning);
  String formatStatusAsText(const SystemStatusReport &status);
  String formatStatusAsJSON(const SystemStatusReport &status);
  String formatStatusAsBrief(const SystemStatusReport &status);

  // Command handlers
  CommandResult handleStatusCommand();
  CommandResult handleDiagnosticsCommand();
  CommandResult handleSchedulesCommand(std::vector<Schedule>* schedules);
  CommandResult handleStopCommand(bool* scheduleRunning, bool* scheduleLoaded);
  CommandResult handleStartCommand(const String &schedId);
  CommandResult handleSMSOnCommand(bool* enableSMSBroadcast);
  CommandResult handleSMSOffCommand(bool* enableSMSBroadcast);
  CommandResult handleCheckCommand();
  CommandResult handleNodeCommand(const String &cmd);
  CommandResult handleHelpCommand();
  CommandResult handleStatsCommand();
  CommandResult handleReportCommand();

  // Internal routing
  CommandResult routeCommand(const String &cmd, std::vector<Schedule>* schedules, 
                            bool* scheduleRunning, bool* scheduleLoaded, 
                            bool* enableSMSBroadcast);
  void sendResponse(const String &response, const String &channel);
  void sendMultiChannelResponse(const String &response);

public:
  UserCommunication();

  // ========== Initialization ==========
  void init(ModemSMS* sms, BLEComm* ble, LoRaComm* lora, MQTTComm* mqtt, 
            WiFiComm* wifi, HTTPComm* http, CommSetup* setup, const String &adminPhoneNum);
  void setNodeCommandCallback(NodeCommandCallback callback);

  // ========== Diagnostics - Printing Functions ==========
  void printSystemStatus(std::vector<Schedule>* schedules, bool scheduleRunning);
  void printBriefStatus(std::vector<Schedule>* schedules, bool scheduleRunning);
  void printCommStatus();
  void printSystemDiagnostics();
  void printNetworkDiagnostics();
  void printLoRaDiagnostics();
  void printBLEDiagnostics();
  void printScheduleStatus(std::vector<Schedule>* schedules, bool scheduleRunning);

  // ========== Diagnostics - Status Retrieval ==========
  SystemStatusReport getSystemStatus(std::vector<Schedule>* schedules, bool scheduleRunning);
  String getFormattedStatus(std::vector<Schedule>* schedules, bool scheduleRunning, 
                           const String &format = "text");
  String getStatusJSON(std::vector<Schedule>* schedules, bool scheduleRunning);

  // ========== Diagnostics - Broadcasting ==========
  void broadcastSystemStatus(std::vector<Schedule>* schedules, bool scheduleRunning);
  void sendAlert(const String &alertMessage, const String &severity = "INFO");

  // ========== Event Notifications ==========
  void notifyScheduleUpdate(const String &scheduleName, const String &status);
  void onScheduleStarted(const String &scheduleId);
  void onScheduleCompleted(const String &scheduleId);
  void onScheduleFailed(const String &scheduleId, const String &reason);
  void onValveAction(int nodeId, const String &valve, const String &action);
  void onSystemError(const String &errorMessage);
  void onSystemWarning(const String &warningMessage);

  // ========== Health Monitoring ==========
  bool isSystemHealthy();
  String getHealthStatus();

  // ========== Command Processing ==========
  void processAllChannels(std::vector<Schedule>* schedules, bool* scheduleRunning, 
                         bool* scheduleLoaded, bool* enableSMSBroadcast);
  void processSMSCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, 
                         bool* scheduleLoaded, bool* enableSMSBroadcast);
  void processLoRaCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, 
                          bool* scheduleLoaded, bool* enableSMSBroadcast);
  void processMQTTCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, 
                          bool* scheduleLoaded, bool* enableSMSBroadcast);
  void processWiFiCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, 
                          bool* scheduleLoaded, bool* enableSMSBroadcast);
  void processHTTPCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, 
                          bool* scheduleLoaded, bool* enableSMSBroadcast);
  void processBLECommand(int nodeId, const String &command);
  void processSerialCommand(const String &input, std::vector<Schedule>* schedules, 
                           bool* scheduleRunning, bool* scheduleLoaded);
  void processMessage(const String &message);

  // ========== Utility Functions ==========
  void sendCommandResponse(const String &command, const CommandResult &result, 
                          const String &channel = "AUTO");
  String getHelpText();
};

#endif