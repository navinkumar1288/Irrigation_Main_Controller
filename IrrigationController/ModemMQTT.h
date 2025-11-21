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
  unsigned long lastReconfigAttempt;  // Track last reconfiguration attempt
  int reconfigAttempts;  // Track consecutive reconfiguration attempts
  unsigned long cooldownStartTime;  // When MQTT entered cooldown after max failures
  bool inCooldown;  // True when MQTT is in 1-hour cooldown period

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
