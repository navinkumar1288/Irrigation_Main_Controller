// ModemSMS.h - SMS communication for Quectel EC200U
#ifndef MODEM_SMS_H
#define MODEM_SMS_H

#include <Arduino.h>
#include <map>
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
  std::vector<int> pendingMessageIndices;  // Queue of unread message indices from URCs
  std::map<String, unsigned long> lastAlertTime;  // Rate limiting for alerts

  bool waitForPrompt(char ch, unsigned long timeout = 5000);
  String readSMSByIndex(int index, String &sender, String &timestamp);
  bool configureTextMode();
  bool isValidPhoneNumber(const String &phoneNumber);
  void handleNewMessageURC(int index);  // Handle +CMTI URC
  void processURC(const String& urc);  // Process a single URC (from buffer or serial)
  bool isNetworkProviderMessage(const String &sender);  // Check if message is from network provider
  bool shouldSendAlert(const String &alertKey);  // Rate limiting check

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
  void printSMSDiagnostics();  // Print SMS configuration and status
  void scanForNewMessages();  // Actively poll modem for new messages (bypasses URCs)
  std::vector<SMSMessage> processIncomingMessages(const String &adminPhone);  // Process incoming messages, return command messages

  // Notification functions
  bool sendNotification(const String &message, const String &alertKey = "");  // Send SMS with rate limiting
  bool sendNotificationToPhones(const String &message, const std::vector<String> &phoneNumbers, const String &alertKey = "");  // Send to multiple phones
};

extern ModemSMS modemSMS;

#endif
