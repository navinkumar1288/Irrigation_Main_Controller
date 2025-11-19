// ModemBase.cpp - Base modem functionality for Quectel EC200U
#include "ModemBase.h"

HardwareSerial SerialAT(1);  // Use Serial1 for modem

// Define static member variable (shared across all instances)
bool ModemBase::modemReady = false;

ModemBase::ModemBase() {
  serial = &SerialAT;
}

bool ModemBase::init() {
  Serial.println("[Modem] Initializing EC200U...");
  
  // Power on EC200U
  pinMode(MODEM_PWRKEY, OUTPUT);
  pinMode(MODEM_RESET, OUTPUT);
  
  // Reset modem
  digitalWrite(MODEM_RESET, HIGH);
  delay(100);
  digitalWrite(MODEM_RESET, LOW);
  delay(100);
  digitalWrite(MODEM_RESET, HIGH);
  delay(2000);
  
  // Power on sequence for EC200U
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(500);
  digitalWrite(MODEM_PWRKEY, LOW);
  delay(2000);
  
  Serial.println("[Modem] Waiting for boot...");
  delay(5000);  // EC200U takes ~5 seconds to boot

  // Start serial communication
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(1000);

  // Test modem communication
  Serial.println("[Modem] Testing communication...");
  bool atOk = false;
  for (int i = 0; i < 10; i++) {
    String resp = sendCommand("AT", 1000);
    if (resp.indexOf("OK") >= 0) {
      Serial.println("[Modem] ✓ Communication OK");
      atOk = true;
      break;
    }
    delay(1000);
  }

  if (!atOk) {
    Serial.println("[Modem] ❌ Communication failed");
    return false;
  }
  
  // Disable echo
  sendCommand("ATE0", 1000);
  
  // Check module info
  String model = sendCommand("ATI", 1000);
  Serial.println("[Modem] Model: " + model);
  
  // Check SIM card - retry multiple times as SIM detection can take time
  Serial.println("[Modem] Checking SIM...");
  bool simReady = false;
  String simStatus = "";

  for (int retry = 0; retry < 15; retry++) {
    simStatus = sendCommand("AT+CPIN?", 2000);

    if (simStatus.indexOf("READY") >= 0) {
      simReady = true;
      Serial.println("[Modem] ✓ SIM ready");
      break;
    }

    // Check if it's a temporary error (SIM still initializing)
    if (simStatus.indexOf("+CME ERROR: 14") >= 0) {
      // CME ERROR 14 = SIM busy (still initializing)
      if (retry % 3 == 0) {
        Serial.print("[Modem] SIM initializing");
      }
      Serial.print(".");
      delay(2000);  // Wait longer for SIM busy
    } else if (simStatus.indexOf("ERROR") >= 0) {
      // Other errors - wait a bit and retry
      if (retry == 0) {
        Serial.print("[Modem] Waiting for SIM");
      }
      Serial.print(".");
      delay(1000);
    } else {
      // No response or unknown - retry
      delay(1000);
    }
  }

  if (!simReady) {
    Serial.println("\n[Modem] ❌ SIM not ready after 15 attempts!");
    Serial.println("[Modem] Last response: " + simStatus);

    // Parse and display CME ERROR code if present
    int cmePos = simStatus.indexOf("+CME ERROR:");
    if (cmePos >= 0) {
      // Extract error code
      int codeStart = cmePos + 12; // Length of "+CME ERROR: "
      int codeEnd = simStatus.indexOf("\n", codeStart);
      if (codeEnd < 0) codeEnd = simStatus.length();
      String errorCode = simStatus.substring(codeStart, codeEnd);
      errorCode.trim();

      Serial.println("[Modem] CME Error Code: " + errorCode);

      // Provide helpful error descriptions
      int code = errorCode.toInt();
      switch (code) {
        case 10: Serial.println("[Modem] Error: SIM not inserted"); break;
        case 11: Serial.println("[Modem] Error: SIM PIN required"); break;
        case 12: Serial.println("[Modem] Error: SIM PUK required"); break;
        case 13: Serial.println("[Modem] Error: SIM failure"); break;
        case 14: Serial.println("[Modem] Error: SIM busy (timeout waiting)"); break;
        case 15: Serial.println("[Modem] Error: SIM wrong"); break;
        case 16: Serial.println("[Modem] Error: Incorrect password"); break;
        case 17: Serial.println("[Modem] Error: SIM PIN2 required"); break;
        case 18: Serial.println("[Modem] Error: SIM PUK2 required"); break;
        case 20: Serial.println("[Modem] Error: Memory full"); break;
        case 21: Serial.println("[Modem] Error: Invalid index"); break;
        case 22: Serial.println("[Modem] Error: Not found"); break;
        case 23: Serial.println("[Modem] Error: Memory failure"); break;
        case 24: Serial.println("[Modem] Error: Text string too long"); break;
        case 25: Serial.println("[Modem] Error: Invalid characters in text"); break;
        case 26: Serial.println("[Modem] Error: Dial string too long"); break;
        case 27: Serial.println("[Modem] Error: Invalid characters in dial string"); break;
        case 30: Serial.println("[Modem] Error: No network service"); break;
        case 31: Serial.println("[Modem] Error: Network timeout"); break;
        case 32: Serial.println("[Modem] Error: Network not allowed - emergency calls only"); break;
        case 100: Serial.println("[Modem] Error: Unknown error"); break;
        default: Serial.println("[Modem] Error: Code " + errorCode); break;
      }

      // Special guidance for common issues
      if (code == 10) {
        Serial.println("[Modem] ℹ Please insert a SIM card and restart");
      } else if (code == 11) {
        Serial.println("[Modem] ℹ Use AT+CPIN=<pin> to unlock SIM");
      } else if (code == 12) {
        Serial.println("[Modem] ℹ SIM locked! Use AT+CPIN=<puk>,<new_pin> to unlock");
      } else if (code == 13 || code == 15) {
        Serial.println("[Modem] ℹ Try reseating the SIM card or use a different SIM");
      } else if (code == 14) {
        Serial.println("[Modem] ℹ SIM was busy for too long - may be defective");
      }
    }

    return false;
  }
  
  // Configure network mode (LTE only for EC200U)
  sendCommand("AT+QCFG=\"nwscanmode\",3,1", 2000);  // LTE only
  
  // Set APN (CRITICAL for EC200U)
  Serial.println("[Modem] Configuring APN...");
  // Format: AT+QICSGP=<contextID>,<context_type>,"<APN>","<username>","<password>",<authentication>
  sendCommand("AT+QICSGP=1,1,\"" + String(MODEM_APN) + "\",\"\",\"\",1", 2000);
  
  // Wait for network registration
  Serial.println("[Modem] Waiting for network registration...");
  bool registered = false;
  int attempts = 0;

  while (attempts < NETWORK_REGISTRATION_TIMEOUT_S && !registered) {  // Configurable timeout
    String creg = sendCommand("AT+CREG?", 1000);
    String cgreg = sendCommand("AT+CGREG?", 1000);
    
    // Check registration status
    // +CREG: 0,1 = registered (home)
    // +CREG: 0,5 = registered (roaming)
    if ((creg.indexOf(",1") >= 0 || creg.indexOf(",5") >= 0) ||
        (cgreg.indexOf(",1") >= 0 || cgreg.indexOf(",5") >= 0)) {
      registered = true;
      Serial.println("\n[Modem] ✓ Network registered");
      break;
    }
    
    if (attempts % 5 == 0) {
      Serial.print("\n[Modem] Still waiting... ");
    }
    Serial.print(".");
    
    delay(1000);
    attempts++;
  }
  
  if (!registered) {
    Serial.println("\n[Modem] ❌ Network registration failed");
    
    // Debug info
    Serial.println("[Modem] Debug info:");
    sendCommand("AT+CREG?", 1000);
    sendCommand("AT+CGREG?", 1000);
    sendCommand("AT+COPS?", 3000);
    
    return false;
  }
  
  // Check signal quality
  String csq = getSignalQuality();
  Serial.println("[Modem] Signal quality: " + csq);
  
  // Check operator
  String cops = getOperator();
  Serial.println("[Modem] Operator: " + cops);
  
  // Activate PDP context (CRITICAL for EC200U)
  Serial.println("[Modem] Activating data connection...");
  sendCommand("AT+QIACT=1", 3000);
  delay(1000);
  
  // Check PDP context activation
  String qiact = sendCommand("AT+QIACT?", 2000);
  Serial.println("[Modem] PDP Context: " + qiact);
  
  if (qiact.indexOf("1,1") < 0) {
    Serial.println("[Modem] ⚠ PDP context not active, retrying...");
    sendCommand("AT+QIDEACT=1", 2000);
    delay(1000);
    sendCommand("AT+QIACT=1", 3000);
    delay(2000);
  }
  
  modemReady = true;
  Serial.println("[Modem] ✓ Initialization complete");
  
  return true;
}

String ModemBase::sendCommand(const String &cmd, uint32_t timeout) {
  Serial.println("[Modem] TX: " + cmd);

  // Clear input buffer
  clearSerialBuffer();

  // Send command
  SerialAT.println(cmd);

  // Wait for response
  unsigned long start = millis();
  String response = "";

  while (millis() - start < timeout) {
    if (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;

      // Check if we got OK
      if (response.indexOf("OK\r\n") >= 0) {
        break;
      }

      // Check if we got ERROR - wait a bit more to get the error code
      if (response.indexOf("ERROR") >= 0) {
        // Wait to capture complete error message (CME/CMS error codes)
        delay(200);
        while (SerialAT.available()) {
          response += (char)SerialAT.read();
        }
        break;
      }
    }
    delay(1);
  }

  if (response.length() > 0) {
    Serial.println("[Modem] RX: " + response);
  } else {
    Serial.println("[Modem] RX: (timeout)");
  }

  return response;
}

void ModemBase::clearSerialBuffer() {
  while (SerialAT.available()) {
    SerialAT.read();
  }
}

bool ModemBase::isReady() {
  return modemReady;
}

String ModemBase::getSignalQuality() {
  String csq = sendCommand("AT+CSQ", 1000);
  
  // Parse signal strength
  int rssiStart = csq.indexOf("+CSQ: ");
  if (rssiStart >= 0) {
    int commaPos = csq.indexOf(',', rssiStart);
    String rssiStr = csq.substring(rssiStart + 6, commaPos);
    int rssi = rssiStr.toInt();
    
    if (rssi == 99) {
      Serial.println("[Modem] ⚠ No signal!");
    } else {
      Serial.printf("[Modem] Signal strength: %d/31\n", rssi);
    }
  }
  
  return csq;
}

String ModemBase::getOperator() {
  return sendCommand("AT+COPS?", 3000);
}

void ModemBase::processBackground() {
  // Process any unsolicited response codes (URCs)
  while (SerialAT.available()) {
    String urc = SerialAT.readStringUntil('\n');
    urc.trim();
    
    if (urc.length() > 0) {
      Serial.println("[Modem] URC: " + urc);
    }
  }
}
