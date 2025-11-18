// ModemMQTT.h - MQTT communication for Quectel EC200U
#ifndef MODEM_MQTT_H
#define MODEM_MQTT_H

#include <Arduino.h>
#include "ModemBase.h"
#include "Config.h"

class ModemMQTT : public ModemBase {
private:
  bool mqttConnected;
  unsigned long lastMqttCheck;
  unsigned long mqttCheckInterval;
  
  bool openMQTTConnection();
  bool connectMQTTBroker();

public:
  ModemMQTT();
  bool configure();
  bool publish(const String &topic, const String &payload);
  bool subscribe(const String &topic);
  bool isConnected();
  void reconnect();
  void processBackground();  // Override base class method
};

extern ModemMQTT modemMQTT;

#endif
