// UserCommunication.h - Handles all user communication (SMS, BLE, LoRa, MQTT, Serial)
#ifndef USER_COMMUNICATION_H
#define USER_COMMUNICATION_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include "Config.h"
#include "ModemSMS.h"
#include "BLEComm.h"
#include "LoRaComm.h"
#include "ModemMQTT.h"

// Forward declarations
class ScheduleManager;
struct Schedule;

// Command result structure
struct CommandResult {
  bool success;
  String response;
  String commandType;
};

// Node command callback - called when user sends a node command
// Returns true if command succeeded, false if failed
typedef std::function<bool(int nodeId, const String& command)> NodeCommandCallback;

class UserCommunication {
private:
  ModemSMS* smsComm;
  BLEComm* bleComm;
  LoRaComm* loraComm;
  ModemMQTT* mqttComm;
  NodeCommandCallback nodeCommandCallback;
  String adminPhone;

  // Command handlers
  CommandResult handleStatusCommand();
  CommandResult handleSchedulesCommand(std::vector<Schedule>* schedules);
  CommandResult handleStopCommand(bool* scheduleRunning, bool* scheduleLoaded);
  CommandResult handleStartCommand(const String &schedId);
  CommandResult handleSMSOnCommand(bool* enableSMSBroadcast);
  CommandResult handleSMSOffCommand(bool* enableSMSBroadcast);
  CommandResult handleCheckCommand();
  CommandResult handleNodeCommand(const String &cmd);
  CommandResult handleHelpCommand();

  // Internal command routing
  CommandResult routeCommand(const String &cmd, std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded, bool* enableSMSBroadcast);
  void sendResponse(const String &response, const String &channel);

public:
  UserCommunication();

  // Initialize with module pointers (NO NodeCommunication - uses callback instead)
  void init(ModemSMS* sms, BLEComm* ble, LoRaComm* lora, ModemMQTT* mqtt, const String &adminPhoneNum);

  // Set callback for node commands (business logic in .ino file)
  void setNodeCommandCallback(NodeCommandCallback callback);

  // Unified channel processing - checks all enabled communication channels
  void processAllChannels(std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded, bool* enableSMSBroadcast);

  // Individual channel processors (called by processAllChannels)
  void processSMSCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded, bool* enableSMSBroadcast);
  void processLoRaCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded, bool* enableSMSBroadcast);
  void processMQTTCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded, bool* enableSMSBroadcast);
  void processBLECommand(int nodeId, const String &command);
  void processSerialCommand(const String &input, std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded);

  // Send notifications
  void sendNotification(const String &message, const String &alertKey = "");  // Unified notification to all enabled channels
  void sendSMSNotification(const String &message);  // SMS only (legacy)
  void sendBLENotification(const String &message);  // BLE only (legacy)

  // Publish status to all available channels (MQTT/SMS/BLE)
  void publishStatus(const String &msg);

  // Process serial input (handles all serial commands)
  void processSerialInput(std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded);

  // Process background tasks for all communication modules (MQTT/SMS auto-reconnect, message scanning)
  void processBackground();
};

extern UserCommunication userComm;

#endif
