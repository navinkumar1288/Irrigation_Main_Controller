// ModemBase.cpp - Base modem functionality for Quectel EC200U
#include "ModemBase.h"

HardwareSerial SerialAT(1);  // Use Serial1 for modem

ModemBase::ModemBase() : modemReady(false) {
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
  
  // Test modem
  Serial.println("[Modem] Testing communication...");
  for (int i = 0; i < 5; i++) {
    String resp = sendCommand("AT", 1000);
    if (resp.indexOf("OK") >= 0) {
      Serial.println("[Modem] ✓ Communication OK");
      break;
    }
    delay(1000);
  }
  
  // Disable echo
  sendCommand("ATE0", 1000);
  
  // Check module info
  String model = sendCommand("ATI", 1000);
  Serial.println("[Modem] Model: " + model);
  
  // Check SIM card
  Serial.println("[Modem] Checking SIM...");
  String simStatus = sendCommand("AT+CPIN?", 2000);
  if (simStatus.indexOf("READY") < 0) {
    Serial.println("[Modem] ❌ SIM not ready!");
    Serial.println("[Modem] Response: " + simStatus);
    return false;
  }
  Serial.println("[Modem] ✓ SIM ready");
  
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
      
      // Check if we got OK or ERROR
      if (response.indexOf("OK\r\n") >= 0 || response.indexOf("ERROR") >= 0) {
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
