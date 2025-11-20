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
  bool needsReconfigure;
  unsigned long lastSMSCheck;
  unsigned long smsCheckInterval;
  std::vector<int> pendingMessageIndices;  // Queue of unread message indices from URCs

  bool waitForPrompt(char ch, unsigned long timeout = 5000);
  String readSMSByIndex(int index, String &sender, String &timestamp);
  bool configureTextMode();
  bool isValidPhoneNumber(const String &phoneNumber);
  void handleNewMessageURC(int index);  // Handle +CMTI URC

public:
  ModemSMS();
  bool configure();
  bool sendSMS(const String &phoneNumber, const String &message);
  bool checkNewMessages();
  int getUnreadCount();
  std::vector<int> getUnreadIndices();  // Get list of unread message indices
  bool readSMS(int index, SMSMessage &sms);
  bool deleteSMS(int index);
  bool deleteAllSMS();
  void processBackground();  // Override base class method
  bool isReady();
  bool needsReconfiguration();  // Check if reconfiguration is needed after modem restart
  void printSMSDiagnostics();  // Print SMS configuration and status
};

extern ModemSMS modemSMS;

#endif
