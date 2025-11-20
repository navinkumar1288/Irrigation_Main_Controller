// ModemSMS.cpp - SMS communication for Quectel EC200U
#include "ModemSMS.h"
#include "ModemMQTT.h"  // For accessing shared URC buffer

ModemSMS::ModemSMS() : smsReady(false), needsReconfigure(false), lastSMSCheck(0), smsCheckInterval(10000) {
  pendingMessageIndices.clear();
}

bool ModemSMS::configure() {
  if (!modemReady) {
    Serial.println("[SMS] ❌ Modem not ready for SMS");
    needsReconfigure = false;  // Clear flag to prevent infinite loop
    return false;
  }

  Serial.println("[SMS] Configuring...");

  // CRITICAL: Configure Quectel modem to route URCs to UART1 (not USB)
  // Without this, +CMTI notifications won't be received on the ESP32's UART
  sendCommand("AT+QURCCFG=\"urcport\",\"uart1\"", 2000);
  Serial.println("[SMS] ✓ URCs routed to UART1");

  // Configure Ring Indicator for incoming SMS
  // This enables proper SMS notification signaling
  sendCommand("AT+QCFG=\"urc/ri/smsincoming\",\"pulse\",120", 2000);
  Serial.println("[SMS] ✓ SMS RI configured");

  // Configure SMS text mode
  if (!configureTextMode()) {
    needsReconfigure = false;  // Clear flag even on failure to prevent infinite loop
    return false;
  }

  // Set SMS storage to SIM card
  String resp = sendCommand("AT+CPMS=\"SM\",\"SM\",\"SM\"", 2000);
  if (resp.indexOf("OK") < 0) {
    Serial.println("[SMS] ⚠ Failed to set storage, trying ME");
    sendCommand("AT+CPMS=\"ME\",\"ME\",\"ME\"", 2000);
  }

  // Enable new SMS notification
  // AT+CNMI=<mode>,<mt>,<bm>,<ds>,<bfr>
  // mode=2: buffer URCs in TA when link is reserved
  // mt=1: SMS-DELIVER indications to TE
  sendCommand("AT+CNMI=2,1,0,0,0", 2000);

  // Set character set to GSM
  sendCommand("AT+CSCS=\"GSM\"", 2000);

  // Check SMSC address - this is CRITICAL for sending SMS
  String csca = sendCommand("AT+CSCA?", 2000);
  Serial.println("[SMS] SMSC Check: " + csca);
  if (csca.indexOf("ERROR") >= 0 || csca.indexOf("\"\"") >= 0 || csca.indexOf("+CSCA: \"\"") >= 0) {
    Serial.println("[SMS] ⚠ WARNING: SMSC address not configured!");
    Serial.println("[SMS] ⚠ SMS sending will fail without SMSC!");
    Serial.println("[SMS] ℹ Get SMSC from your carrier and set with AT+CSCA=\"+number\"");
  } else {
    Serial.println("[SMS] ✓ SMSC configured");
  }

  smsReady = true;
  needsReconfigure = false;  // Clear reconfiguration flag
  Serial.println("[SMS] ✓ Configuration complete");

  return true;
}

bool ModemSMS::configureTextMode() {
  // Set SMS format to text mode (easier to work with)
  String resp = sendCommand("AT+CMGF=1", 2000);
  if (resp.indexOf("OK") >= 0) {
    Serial.println("[SMS] ✓ Text mode enabled");
    return true;
  } else {
    Serial.println("[SMS] ❌ Failed to set text mode");
    return false;
  }
}

bool ModemSMS::isValidPhoneNumber(const String &phoneNumber) {
  // Basic validation for phone numbers
  if (phoneNumber.length() < 7) {
    return false;  // Too short
  }

  // Must start with + (international format)
  if (phoneNumber.charAt(0) != '+') {
    return false;
  }

  // Check for obviously invalid patterns like +0987654321
  // Valid international numbers start with + followed by country code (1-3 digits)
  // Then area code and number (total should be reasonable length)

  // Count digits (excluding +)
  int digitCount = 0;
  for (unsigned int i = 1; i < phoneNumber.length(); i++) {
    if (isdigit(phoneNumber.charAt(i))) {
      digitCount++;
    } else if (phoneNumber.charAt(i) != ' ' && phoneNumber.charAt(i) != '-') {
      // Invalid character
      return false;
    }
  }

  // Valid phone numbers have 7-15 digits (E.164 standard)
  if (digitCount < 7 || digitCount > 15) {
    return false;
  }

  // Check for obviously fake patterns (all zeros after +, etc.)
  if (phoneNumber.startsWith("+0000") || phoneNumber.startsWith("+0987")) {
    Serial.println("[SMS] ⚠ Detected test/invalid number pattern");
    return false;
  }

  return true;
}

bool ModemSMS::sendSMS(const String &phoneNumber, const String &message) {
  if (!smsReady) {
    Serial.println("[SMS] ❌ SMS not ready");
    return false;
  }

  // Validate phone number before attempting to send
  if (!isValidPhoneNumber(phoneNumber)) {
    Serial.println("[SMS] ❌ Invalid phone number: " + phoneNumber);
    Serial.println("[SMS] ℹ Use international format: +<country><area><number>");
    Serial.println("[SMS] ℹ Example: +919944272647");
    return false;
  }

  Serial.println("[SMS] Sending to: " + phoneNumber);
  Serial.println("[SMS] Message: " + message);
  
  // Clear buffer
  clearSerialBuffer();
  
  // Start SMS
  String cmd = "AT+CMGS=\"" + phoneNumber + "\"";
  SerialAT.println(cmd);
  Serial.println("[SMS] TX: " + cmd);
  
  // Wait for '>' prompt
  if (!waitForPrompt('>', 5000)) {
    Serial.println("[SMS] ❌ No prompt received");
    return false;
  }
  
  // Send message text
  SerialAT.print(message);
  delay(100);
  
  // Send Ctrl+Z to send SMS
  SerialAT.write(0x1A);
  Serial.println("[SMS] Message sent, waiting for response...");
  
  // Wait for response
  unsigned long start = millis();
  String response = "";
  bool success = false;
  bool errorDetected = false;

  while (millis() - start < 30000) {  // 30 second timeout for SMS
    if (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;

      // Check for success
      if (response.indexOf("+CMGS:") >= 0 && response.indexOf("OK") >= 0) {
        success = true;
        break;
      }

      // Check for error - but keep reading to get the full error code
      if (response.indexOf("ERROR") >= 0) {
        errorDetected = true;
        // Wait a bit more to capture the complete error message
        delay(500);
        // Read any remaining characters
        while (SerialAT.available()) {
          response += (char)SerialAT.read();
        }
        break;
      }
    }
    delay(10);
  }

  Serial.println("[SMS] Response: " + response);

  if (success) {
    Serial.println("[SMS] ✓ SMS sent successfully");
    return true;
  } else {
    Serial.println("[SMS] ❌ SMS send failed");

    // Parse and display CMS ERROR code if present
    if (errorDetected) {
      int cmsPos = response.indexOf("+CMS ERROR:");
      if (cmsPos >= 0) {
        // Extract error code
        int codeStart = cmsPos + 12; // Length of "+CMS ERROR: "
        int codeEnd = response.indexOf("\n", codeStart);
        if (codeEnd < 0) codeEnd = response.length();
        String errorCode = response.substring(codeStart, codeEnd);
        errorCode.trim();

        Serial.println("[SMS] CMS Error Code: " + errorCode);

        // Provide helpful error descriptions
        int code = errorCode.toInt();
        switch (code) {
          case 300: Serial.println("[SMS] Error: ME failure"); break;
          case 301: Serial.println("[SMS] Error: SMS service of ME reserved"); break;
          case 302: Serial.println("[SMS] Error: Operation not allowed"); break;
          case 303: Serial.println("[SMS] Error: Operation not supported"); break;
          case 304: Serial.println("[SMS] Error: Invalid PDU mode parameter"); break;
          case 305: Serial.println("[SMS] Error: Invalid text mode parameter"); break;
          case 310: Serial.println("[SMS] Error: SIM not inserted"); break;
          case 311: Serial.println("[SMS] Error: SIM PIN required"); break;
          case 312: Serial.println("[SMS] Error: PH-SIM PIN required"); break;
          case 313: Serial.println("[SMS] Error: SIM failure"); break;
          case 314: Serial.println("[SMS] Error: SIM busy"); break;
          case 315: Serial.println("[SMS] Error: SIM wrong"); break;
          case 316: Serial.println("[SMS] Error: SIM PUK required"); break;
          case 317: Serial.println("[SMS] Error: SIM PIN2 required"); break;
          case 318: Serial.println("[SMS] Error: SIM PUK2 required"); break;
          case 320: Serial.println("[SMS] Error: Memory failure"); break;
          case 321: Serial.println("[SMS] Error: Invalid memory index"); break;
          case 322: Serial.println("[SMS] Error: Memory full"); break;
          case 330: Serial.println("[SMS] Error: SMSC address unknown"); break;
          case 331: Serial.println("[SMS] Error: No network service"); break;
          case 332: Serial.println("[SMS] Error: Network timeout"); break;
          case 340: Serial.println("[SMS] Error: No +CNMA acknowledgement expected"); break;
          case 500: Serial.println("[SMS] Error: Unknown error"); break;
          case 512: Serial.println("[SMS] Error: User abort"); break;
          case 513: Serial.println("[SMS] Error: Unable to store"); break;
          case 514: Serial.println("[SMS] Error: Invalid status"); break;
          case 515: Serial.println("[SMS] Error: Invalid character in address string"); break;
          case 516: Serial.println("[SMS] Error: Invalid length"); break;
          case 517: Serial.println("[SMS] Error: Invalid character in PDU"); break;
          case 518: Serial.println("[SMS] Error: Invalid parameter"); break;
          case 519: Serial.println("[SMS] Error: Invalid length or character"); break;
          case 520: Serial.println("[SMS] Error: Invalid input value"); break;
          case 521: Serial.println("[SMS] Error: No service center address"); break;
          case 522: Serial.println("[SMS] Error: Memory failure"); break;
          case 528: Serial.println("[SMS] Error: Invalid PDU mode"); break;
          case 529: Serial.println("[SMS] Error: Device busy"); break;
          case 530: Serial.println("[SMS] Error: Invalid destination address / No phone number"); break;
          case 531: Serial.println("[SMS] Error: Not supported"); break;
          case 532: Serial.println("[SMS] Error: Invalid format (text)"); break;
          default: Serial.println("[SMS] Error: Code " + errorCode); break;
        }

        // Special guidance for common issues
        if (code == 330 || code == 521) {
          Serial.println("[SMS] ℹ Get SMSC from carrier: AT+CSCA=\"+number\"");
        } else if (code == 530) {
          Serial.println("[SMS] ℹ Check phone number format - use full international format");
        } else if (code == 331) {
          Serial.println("[SMS] ℹ Check network registration: AT+CREG?");
        }
      } else {
        // Generic error without CMS code
        Serial.println("[SMS] Error: Generic modem error (check AT command syntax)");
      }
    }

    return false;
  }
}

bool ModemSMS::waitForPrompt(char ch, unsigned long timeout) {
  unsigned long start = millis();
  
  while (millis() - start < timeout) {
    if (SerialAT.available()) {
      char c = SerialAT.read();
      Serial.print(c);  // Debug
      if (c == ch) {
        return true;
      }
    }
    delay(10);
  }
  
  return false;
}

void ModemSMS::handleNewMessageURC(int index) {
  // Add to pending queue if not already there
  for (int idx : pendingMessageIndices) {
    if (idx == index) {
      return;  // Already in queue
    }
  }
  pendingMessageIndices.push_back(index);
  Serial.println("[SMS] 📨 New message at index " + String(index) + " added to queue");
}

bool ModemSMS::checkNewMessages() {
  if (!smsReady) {
    return false;
  }

  // Check if we have pending messages from URCs
  return !pendingMessageIndices.empty();
}

int ModemSMS::getUnreadCount() {
  if (!smsReady) {
    return 0;
  }

  return pendingMessageIndices.size();
}

std::vector<int> ModemSMS::getUnreadIndices() {
  if (!smsReady) {
    std::vector<int> empty;
    return empty;
  }

  // Return a copy of pending indices and clear the queue
  std::vector<int> indices = pendingMessageIndices;
  pendingMessageIndices.clear();

  return indices;
}

bool ModemSMS::readSMS(int index, SMSMessage &sms) {
  if (!smsReady) {
    return false;
  }
  
  Serial.println("[SMS] Reading message at index: " + String(index));
  
  String sender, timestamp;
  String message = readSMSByIndex(index, sender, timestamp);
  
  if (message.length() > 0) {
    sms.index = index;
    sms.sender = sender;
    sms.timestamp = timestamp;
    sms.message = message;
    
    Serial.println("[SMS] ✓ Message read");
    Serial.println("[SMS] From: " + sender);
    Serial.println("[SMS] Time: " + timestamp);
    Serial.println("[SMS] Message: " + message);
    
    return true;
  }
  
  return false;
}

String ModemSMS::readSMSByIndex(int index, String &sender, String &timestamp) {
  String cmd = "AT+CMGR=" + String(index);
  String resp = sendCommand(cmd, 3000);

  // Parse response
  // TEXT MODE: +CMGR: "REC UNREAD","+1234567890","","21/11/17,10:30:45+00"
  //           Message text here
  // PDU MODE:  +CMGR: 0,,29
  //           0791198904109116...

  int cmgrPos = resp.indexOf("+CMGR:");
  if (cmgrPos < 0) {
    Serial.println("[SMS] ❌ Failed to read SMS");
    return "";
  }

  // Check if response is in PDU mode (no quotes after +CMGR:)
  // PDU format: "+CMGR: 0,,29" or "+CMGR: 1,,29"
  // Text format: "+CMGR: "REC UNREAD",..."
  int firstQuoteCheck = resp.indexOf("\"", cmgrPos + 7);
  int firstCommaCheck = resp.indexOf(",", cmgrPos + 7);

  if (firstCommaCheck >= 0 && (firstQuoteCheck < 0 || firstCommaCheck < firstQuoteCheck)) {
    // This is PDU mode - modem lost text mode configuration!
    Serial.println("[SMS] ⚠ WARNING: Message in PDU mode!");
    Serial.println("[SMS] ⚠ This means modem restarted and lost text mode config");
    Serial.println("[SMS] → Triggering SMS reconfiguration...");

    // Mark for reconfiguration
    smsReady = false;
    needsReconfigure = true;

    // Don't try to parse PDU format - message will be retried after reconfiguration
    Serial.println("[SMS] ℹ Message will be processed after reconfiguration");
    return "";
  }

  // Extract sender (phone number)
  int firstQuote = resp.indexOf("\"", cmgrPos + 7);
  int secondQuote = resp.indexOf("\"", firstQuote + 1);
  int thirdQuote = resp.indexOf("\"", secondQuote + 1);
  int fourthQuote = resp.indexOf("\"", thirdQuote + 1);

  if (firstQuote >= 0 && secondQuote >= 0 && thirdQuote >= 0 && fourthQuote >= 0) {
    sender = resp.substring(thirdQuote + 1, fourthQuote);
  }

  // Extract timestamp
  int fifthQuote = resp.indexOf("\"", fourthQuote + 1);
  int sixthQuote = resp.indexOf("\"", fifthQuote + 1);
  
  if (fifthQuote >= 0 && sixthQuote >= 0) {
    timestamp = resp.substring(fifthQuote + 1, sixthQuote);
  }
  
  // Extract message text (after first newline after +CMGR)
  int msgStart = resp.indexOf("\n", cmgrPos);
  if (msgStart >= 0) {
    msgStart++;
    int msgEnd = resp.indexOf("\n\nOK", msgStart);
    if (msgEnd < 0) {
      msgEnd = resp.indexOf("\nOK", msgStart);
    }
    if (msgEnd >= 0) {
      String message = resp.substring(msgStart, msgEnd);
      message.trim();
      return message;
    }
  }
  
  return "";
}

bool ModemSMS::deleteSMS(int index) {
  String cmd = "AT+CMGD=" + String(index);
  String resp = sendCommand(cmd, 2000);
  
  if (resp.indexOf("OK") >= 0) {
    Serial.println("[SMS] ✓ Message deleted");
    return true;
  }
  
  Serial.println("[SMS] ❌ Failed to delete message");
  return false;
}

bool ModemSMS::deleteAllSMS() {
  // Delete all read messages
  // AT+CMGD=<index>,<delflag>
  // delflag=4: delete all messages
  String resp = sendCommand("AT+CMGD=1,4", 3000);
  
  if (resp.indexOf("OK") >= 0) {
    Serial.println("[SMS] ✓ All messages deleted");
    return true;
  }
  
  Serial.println("[SMS] ❌ Failed to delete all messages");
  return false;
}

bool ModemSMS::isReady() {
  return smsReady;
}

void ModemSMS::printSMSDiagnostics() {
  Serial.println("\n[SMS] === SMS Diagnostics ===");

  // Check SMS ready status
  Serial.println("[SMS] SMS Ready: " + String(smsReady ? "Yes" : "No"));

  if (!modemReady) {
    Serial.println("[SMS] ⚠ Modem not ready - cannot run diagnostics");
    return;
  }

  // Check network registration
  String creg = sendCommand("AT+CREG?", 2000);
  Serial.println("[SMS] Network Registration: " + creg);

  // Check signal quality
  String csq = sendCommand("AT+CSQ", 2000);
  Serial.println("[SMS] Signal Quality: " + csq);

  // Check SMS format
  String cmgf = sendCommand("AT+CMGF?", 2000);
  Serial.println("[SMS] SMS Format: " + cmgf);

  // Check SMS storage
  String cpms = sendCommand("AT+CPMS?", 2000);
  Serial.println("[SMS] Storage: " + cpms);

  // Check SMSC (SMS Center) address
  String csca = sendCommand("AT+CSCA?", 2000);
  Serial.println("[SMS] SMSC Address: " + csca);
  if (csca.indexOf("ERROR") >= 0 || csca.indexOf("\"\"") >= 0) {
    Serial.println("[SMS] ⚠ SMSC not configured! This is likely the problem.");
    Serial.println("[SMS] To fix: Get SMSC number from your carrier and set with:");
    Serial.println("[SMS]   AT+CSCA=\"+<carrier_smsc_number>\"");
  }

  // Check character set
  String cscs = sendCommand("AT+CSCS?", 2000);
  Serial.println("[SMS] Character Set: " + cscs);

  // Check URC configuration
  String qurccfg = sendCommand("AT+QURCCFG=\"urcport\"", 2000);
  Serial.println("[SMS] URC Port Config: " + qurccfg);

  // Check SMS notification settings
  String cnmi = sendCommand("AT+CNMI?", 2000);
  Serial.println("[SMS] SMS Notification: " + cnmi);

  Serial.println("[SMS] === End Diagnostics ===\n");
}

void ModemSMS::processBackground() {
  // Process SMS-specific URCs
  // Note: Don't call ModemBase::processBackground() because it consumes
  // all SerialAT data, leaving nothing for us to process!

  // FIRST: Process any URCs forwarded from MQTT handler
  std::vector<String>& sharedBuffer = ModemMQTT::getSharedURCBuffer();
  while (!sharedBuffer.empty()) {
    String urc = sharedBuffer.front();
    sharedBuffer.erase(sharedBuffer.begin());  // Remove from buffer

    if (urc.length() > 0) {
      Serial.println("[SMS] Processing buffered URC: " + urc);
      processURC(urc);  // Process the buffered URC
    }
  }

  // SECOND: Process new URCs from serial
  while (SerialAT.available()) {
    String urc = SerialAT.readStringUntil('\n');
    urc.trim();

    if (urc.length() > 0) {
      Serial.println("[SMS] URC: " + urc);
      processURC(urc);  // Process the new URC
    }
  }
}

// Helper function to process a single URC
void ModemSMS::processURC(const String& urc) {
  // Handle modem restart/reboot
  // When modem restarts, all configuration is lost (including text mode)
  if (urc.indexOf("RDY") >= 0 || urc.indexOf("POWERED DOWN") >= 0) {
    Serial.println("[SMS] ⚠ Modem restart detected!");

    // Reset state and mark for reconfiguration
    smsReady = false;
    needsReconfigure = true;

    Serial.println("[SMS] → SMS marked for reconfiguration");
  }

  // Handle new SMS notification
  // +CMTI: "SM",<index> or +CMTI: "ME",<index>
  if (urc.indexOf("+CMTI:") >= 0) {
    Serial.println("[SMS] 📨 New SMS received!");

    // Extract index
    int indexPos = urc.lastIndexOf(",");
    if (indexPos >= 0) {
      String indexStr = urc.substring(indexPos + 1);
      indexStr.trim();
      int index = indexStr.toInt();

      if (index > 0) {
        // Add to pending queue for processing by main loop
        handleNewMessageURC(index);
      }
    }
  }

  // Handle SMS delivery report
  if (urc.indexOf("+CDS:") >= 0) {
    Serial.println("[SMS] 📬 Delivery report received");
  }

  // Handle SMS send acknowledgement
  if (urc.indexOf("+CMGS:") >= 0) {
    Serial.println("[SMS] ✓ SMS send acknowledged");
  }
}

bool ModemSMS::needsReconfiguration() {
  return needsReconfigure;
}

void ModemSMS::requeueMessage(int index) {
  // Re-add message to queue for retry (e.g., after reconfiguration)
  for (int idx : pendingMessageIndices) {
    if (idx == index) {
      Serial.println("[SMS] ℹ Message index " + String(index) + " already in queue");
      return;  // Already in queue
    }
  }

  pendingMessageIndices.push_back(index);
  Serial.println("[SMS] ♻ Message index " + String(index) + " re-queued for retry");
}
