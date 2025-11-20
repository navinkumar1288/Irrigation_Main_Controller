// ModemMQTT.h - MQTT communication for Quectel EC200U
#ifndef MODEM_MQTT_H
#define MODEM_MQTT_H

#include <Arduino.h>
#include <vector>
#include "ModemBase.h"
#include "Config.h"

class ModemMQTT : public ModemBase {
private:
  bool mqttConnected;
  bool needsReconfigure;
  unsigned long lastMqttCheck;
  unsigned long mqttCheckInterval;

  bool openMQTTConnection();
  bool connectMQTTBroker();
  String escapeATString(const String &input);  // Escape quotes for AT commands

public:
  ModemMQTT();
  bool configure();
  bool publish(const String &topic, const String &payload);
  bool subscribe(const String &topic);
  bool isConnected();
  void reconnect();
  void processBackground();  // Override base class method
  bool needsReconfiguration();  // Check if reconfiguration is needed after modem restart

  // Shared URC buffer access for SMS handler
  static std::vector<String>& getSharedURCBuffer();
};

extern ModemMQTT modemMQTT;

#endif
