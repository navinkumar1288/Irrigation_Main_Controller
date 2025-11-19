// ModemSMS.cpp - SMS communication for Quectel EC200U
#include "ModemSMS.h"

ModemSMS::ModemSMS() : smsReady(false), lastSMSCheck(0), smsCheckInterval(10000) {}

bool ModemSMS::configure() {
  if (!modemReady) {
    Serial.println("[SMS] ❌ Modem not ready for SMS");
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

bool ModemSMS::sendSMS(const String &phoneNumber, const String &message) {
  if (!smsReady) {
    Serial.println("[SMS] ❌ SMS not ready");
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
          default: Serial.println("[SMS] Error: Code " + errorCode); break;
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

bool ModemSMS::checkNewMessages() {
  if (!smsReady) {
    return false;
  }

  // List all unread messages
  // Use numeric status (0=unread) instead of string for better compatibility
  // AT+CMGL=0 works in both text and PDU modes
  String resp = sendCommand("AT+CMGL=0", 3000);

  if (resp.indexOf("+CMGL:") >= 0) {
    Serial.println("[SMS] 📨 New messages detected");
    return true;
  }

  return false;
}

int ModemSMS::getUnreadCount() {
  if (!smsReady) {
    return 0;
  }

  // Use numeric status (0=unread) for better compatibility
  String resp = sendCommand("AT+CMGL=0", 3000);

  int count = 0;
  int pos = 0;

  while ((pos = resp.indexOf("+CMGL:", pos)) >= 0) {
    count++;
    pos += 6;
  }

  return count;
}

std::vector<int> ModemSMS::getUnreadIndices() {
  std::vector<int> indices;

  if (!smsReady) {
    return indices;
  }

  // Use numeric status (0=unread) for better compatibility
  String resp = sendCommand("AT+CMGL=0", 3000);

  // Parse response to extract message indices
  // Format: +CMGL: <index>,"<stat>","<oa>",,"<scts>"
  // Example: +CMGL: 34,"REC UNREAD","+1234567890",,"21/11/17,10:30:45+00"
  int pos = 0;
  while ((pos = resp.indexOf("+CMGL:", pos)) >= 0) {
    // Move past "+CMGL: "
    pos += 7;

    // Extract index number
    int commaPos = resp.indexOf(',', pos);
    if (commaPos > pos) {
      String indexStr = resp.substring(pos, commaPos);
      indexStr.trim();
      int index = indexStr.toInt();
      if (index > 0) {
        indices.push_back(index);
      }
    }

    pos = commaPos;
  }

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
  // +CMGR: "REC UNREAD","+1234567890","","21/11/17,10:30:45+00"
  // Message text here
  
  int cmgrPos = resp.indexOf("+CMGR:");
  if (cmgrPos < 0) {
    Serial.println("[SMS] ❌ Failed to read SMS");
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
  // Call base class method first
  ModemBase::processBackground();
  
  // Process SMS-specific URCs
  while (SerialAT.available()) {
    String urc = SerialAT.readStringUntil('\n');
    urc.trim();
    
    if (urc.length() > 0) {
      Serial.println("[SMS] URC: " + urc);
      
      // Handle new SMS notification
      // +CMTI: "SM",<index>
      if (urc.indexOf("+CMTI:") >= 0) {
        Serial.println("[SMS] 📨 New SMS received!");
        
        // Extract index
        int indexPos = urc.lastIndexOf(",");
        if (indexPos >= 0) {
          String indexStr = urc.substring(indexPos + 1);
          int index = indexStr.toInt();
          
          Serial.println("[SMS] Message index: " + String(index));
          
          // You can add a callback mechanism here to handle new messages
          // For now, just log it
          SMSMessage sms;
          if (readSMS(index, sms)) {
            // Process the message here
            // e.g., add to queue, trigger action, etc.
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
  }
  
  // Periodic check for new messages
  if (smsReady && (millis() - lastSMSCheck > smsCheckInterval)) {
    lastSMSCheck = millis();
    
    int unreadCount = getUnreadCount();
    if (unreadCount > 0) {
      Serial.println("[SMS] 📨 " + String(unreadCount) + " unread message(s)");
    }
  }
}
