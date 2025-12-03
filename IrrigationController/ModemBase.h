// ModemBase.h - Base modem functionality for Quectel EC200U
#ifndef MODEM_BASE_H
#define MODEM_BASE_H

#include <Arduino.h>
#include "Config.h"

class ModemBase {
protected:
  HardwareSerial *serial;
  static bool modemReady;  // Shared across all modem instances (only one physical modem)

  String sendCommand(const String &cmd, uint32_t timeout = 2000);
  void clearSerialBuffer();

public:
  ModemBase();
  bool init();
  bool isReady();
  void processBackground();
  String getSignalQuality();
  String getOperator();
};

extern HardwareSerial SerialAT;

#endif
