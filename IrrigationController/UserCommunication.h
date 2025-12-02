// UserCommunication.h - Handles all user communication (SMS, BLE, LoRa, MQTT, Serial)
#ifndef USER_COMMUNICATION_H
#define USER_COMMUNICATION_H

#include <Arduino.h>
#include <vector>
#include "Config.h"
#include "ModemSMS.h"
#include "BLEComm.h"
#include "LoRaComm.h"
#include "ModemMQTT.h"

// Forward declarations
class ScheduleManager;
class NodeCommunication;
struct Schedule;

// Command result structure
struct CommandResult {
  bool success;
  String response;
  String commandType;
};

class UserCommunication {
private:
  ModemSMS* smsComm;
  BLEComm* bleComm;
  LoRaComm* loraComm;
  ModemMQTT* mqttComm;
  NodeCommunication* nodeComm;
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

  // Initialize with module pointers
  void init(ModemSMS* sms, BLEComm* ble, LoRaComm* lora, ModemMQTT* mqtt, NodeCommunication* node, const String &adminPhoneNum);

  // Unified channel processing - checks all enabled communication channels
  void processAllChannels(std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded, bool* enableSMSBroadcast);

  // Individual channel processors (called by processAllChannels)
  void processSMSCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded, bool* enableSMSBroadcast);
  void processLoRaCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded, bool* enableSMSBroadcast);
  void processMQTTCommands(std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded, bool* enableSMSBroadcast);
  void processBLECommand(int nodeId, const String &command);
  void processSerialCommand(const String &input, std::vector<Schedule>* schedules, bool* scheduleRunning, bool* scheduleLoaded);

  // Send notifications
  void sendNotification(const String &message, const String &channel = "all");
  void sendSMSNotification(const String &message);
  void sendBLENotification(const String &message);
};

extern UserCommunication userComm;

#endif
