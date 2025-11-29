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

  // REMOVED: Hardware reset/power cycle removed per user request
  // The modem should already be powered on and running
  // If modem needs reset, do it manually via hardware power cycle

  // Initialize GPIO pins but don't toggle them
  pinMode(MODEM_PWRKEY, OUTPUT);
  pinMode(MODEM_RESET, OUTPUT);
  digitalWrite(MODEM_RESET, HIGH);  // Keep reset HIGH (not asserted)
  digitalWrite(MODEM_PWRKEY, LOW);  // Keep power key LOW (not pressed)

  Serial.println("[Modem] Skipping hardware reset - using existing modem session");
  delay(1000);  // Brief delay for stability

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
  
  // Check SIM card - quick check with fewer retries since modem may already be initialized
  Serial.println("[Modem] Checking SIM...");
  bool simReady = false;

  // Quick check - only 5 retries since modem should already be running
  for (int retry = 0; retry < 5; retry++) {
    String simStatus = sendCommand("AT+CPIN?", 2000);

    if (simStatus.indexOf("READY") >= 0) {
      simReady = true;
      Serial.println("[Modem] ✓ SIM ready");
      break;
    }

    // If SIM busy, wait a bit
    if (simStatus.indexOf("+CME ERROR: 14") >= 0) {
      Serial.print(".");
      delay(1000);
    } else if (retry < 4) {
      delay(500);
    }
  }

  if (!simReady) {
    Serial.println("\n[Modem] ⚠ SIM check failed - but continuing anyway");
    Serial.println("[Modem] ℹ Modem may already be initialized from previous session");
    // Don't return false - continue with init
  }
  
  // Configure network mode (LTE only for EC200U)
  sendCommand("AT+QCFG=\"nwscanmode\",3,1", 2000);  // LTE only
  
  // Set APN (CRITICAL for EC200U)
  Serial.println("[Modem] Configuring APN...");
  // Format: AT+QICSGP=<contextID>,<context_type>,"<APN>","<username>","<password>",<authentication>
  sendCommand("AT+QICSGP=1,1,\"" + String(MODEM_APN) + "\",\"\",\"\",1", 2000);
  
  // Check network registration - quick check since modem may already be registered
  Serial.println("[Modem] Checking network registration...");
  bool registered = false;

  // Quick check - only 10 attempts (10 seconds) since modem should already be registered
  for (int attempts = 0; attempts < 10; attempts++) {
    String creg = sendCommand("AT+CREG?", 1000);
    String cgreg = sendCommand("AT+CGREG?", 1000);

    // Check registration status
    // +CREG: 0,1 = registered (home)
    // +CREG: 0,5 = registered (roaming)
    if ((creg.indexOf(",1") >= 0 || creg.indexOf(",5") >= 0) ||
        (cgreg.indexOf(",1") >= 0 || cgreg.indexOf(",5") >= 0)) {
      registered = true;
      Serial.println("[Modem] ✓ Network registered");
      break;
    }

    if (attempts % 3 == 0 && attempts > 0) {
      Serial.print(".");
    }
    delay(1000);
  }

  if (!registered) {
    Serial.println("\n[Modem] ⚠ Network registration timeout - but continuing anyway");
    Serial.println("[Modem] ℹ SMS may still work if modem was already registered");
    // Don't return false - SMS can work even without full data registration
  }
  
  // Check signal quality
  String csq = getSignalQuality();
  Serial.println("[Modem] Signal quality: " + csq);
  
  // Check operator
  String cops = getOperator();
  Serial.println("[Modem] Operator: " + cops);
  
  // Activate PDP context (needed for MQTT, but not for SMS)
  Serial.println("[Modem] Checking data connection...");
  String qiact = sendCommand("AT+QIACT?", 2000);

  if (qiact.indexOf("1,1") < 0) {
    Serial.println("[Modem] → Activating PDP context...");
    sendCommand("AT+QIACT=1", 3000);
    delay(1000);
    qiact = sendCommand("AT+QIACT?", 2000);

    if (qiact.indexOf("1,1") >= 0) {
      Serial.println("[Modem] ✓ PDP context active");
    } else {
      Serial.println("[Modem] ⚠ PDP activation failed - SMS will still work");
    }
  } else {
    Serial.println("[Modem] ✓ PDP context already active");
  }

  // Always mark modem as ready - SMS works even without full data connectivity
  modemReady = true;
  Serial.println("[Modem] ✓ Modem ready for SMS/MQTT");

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
