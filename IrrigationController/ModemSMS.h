// ModemSMS.h - SMS communication for Quectel EC200U
#ifndef MODEM_SMS_H
#define MODEM_SMS_H

#include <Arduino.h>
#include "ModemBase.h"
#include "Config.h"

struct SMSMessage {
  int index;
  String sender;
  String timestamp;
  String message;
};

class ModemSMS : public ModemBase {
private:
  bool smsReady;
  unsigned long lastSMSCheck;
  unsigned long smsCheckInterval;
  
  bool waitForPrompt(char ch, unsigned long timeout = 5000);
  String readSMSByIndex(int index, String &sender, String &timestamp);
  bool configureTextMode();

public:
  ModemSMS();
  bool configure();
  bool sendSMS(const String &phoneNumber, const String &message);
  bool checkNewMessages();
  int getUnreadCount();
  bool readSMS(int index, SMSMessage &sms);
  bool deleteSMS(int index);
  bool deleteAllSMS();
  void processBackground();  // Override base class method
  bool isReady();
  void printSMSDiagnostics();  // Print SMS configuration and status
};

extern ModemSMS modemSMS;

#endif
