// PPPoS_Example.ino - Example showing how to use PPPoS with EC200U modem
// This is a minimal example demonstrating PPP over Serial connection
//
// HARDWARE SETUP:
// - Heltec ESP32 V3
// - EC200U modem connected to GPIO45 (RX) and GPIO46 (TX)
// - Modem must be powered and initialized before using PPP
//
// HOW IT WORKS:
// 1. Initialize modem serial communication
// 2. Configure PDP context with APN
// 3. Dial PPP connection (ATD*99#)
// 4. Start ESP32 PPP client
// 5. Get IP address and use standard network libraries
//
// BENEFITS:
// - Use standard MQTT libraries (PubSubClient, AsyncMqttClient, etc.)
// - Use standard HTTP client libraries
// - Full TCP/IP stack available over cellular
// - Better reliability and debugging
//
// NOTE: This is a SIMPLIFIED example. For production use, integrate with
// the PPPoSManager class provided in this project.

#include <Arduino.h>

// Modem pins (Heltec ESP32 V3)
#define MODEM_RX 45
#define MODEM_TX 46
#define MODEM_PWRKEY 4
#define MODEM_RESET 15

// APN for your carrier
#define APN "airtelgprs.com"

// Modem serial
HardwareSerial ModemSerial(1);

// PPP timeout
const unsigned long PPP_CONNECT_TIMEOUT_MS = 30000;

// Simple AT command sender
String sendATCommand(const String &cmd, uint32_t timeout = 2000) {
  Serial.println("[AT] → " + cmd);

  // Clear buffer
  while (ModemSerial.available()) {
    ModemSerial.read();
  }

  // Send command
  ModemSerial.println(cmd);

  // Read response
  unsigned long start = millis();
  String response = "";

  while (millis() - start < timeout) {
    if (ModemSerial.available()) {
      char c = ModemSerial.read();
      response += c;

      if (response.indexOf("OK") >= 0 || response.indexOf("ERROR") >= 0) {
        break;
      }
    }
    delay(1);
  }

  if (response.length() > 0) {
    Serial.println("[AT] ← " + response);
  }

  return response;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n");
  Serial.println("========================================");
  Serial.println("  PPPoS Example - EC200U + Heltec ESP32");
  Serial.println("========================================\n");

  // Initialize GPIO pins
  pinMode(MODEM_PWRKEY, OUTPUT);
  pinMode(MODEM_RESET, OUTPUT);
  digitalWrite(MODEM_RESET, HIGH);  // Keep reset HIGH (not asserted)
  digitalWrite(MODEM_PWRKEY, LOW);  // Keep power key LOW (not pressed)

  // Start modem serial
  Serial.println("[1/5] Starting modem serial...");
  ModemSerial.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(1000);

  // Test modem communication
  Serial.println("[2/5] Testing modem communication...");
  bool atOk = false;
  for (int i = 0; i < 10; i++) {
    String resp = sendATCommand("AT", 1000);
    if (resp.indexOf("OK") >= 0) {
      Serial.println("      ✓ Modem communication OK");
      atOk = true;
      break;
    }
    delay(1000);
  }

  if (!atOk) {
    Serial.println("      ❌ Modem communication failed!");
    Serial.println("\nTroubleshooting:");
    Serial.println("1. Check modem is powered on");
    Serial.println("2. Check UART connections (RX/TX)");
    Serial.println("3. Check baud rate (115200)");
    return;
  }

  // Disable echo
  sendATCommand("ATE0", 1000);

  // Check network registration
  Serial.println("[3/5] Checking network registration...");
  bool registered = false;
  for (int i = 0; i < 10; i++) {
    String creg = sendATCommand("AT+CREG?", 1000);
    if (creg.indexOf(",1") >= 0 || creg.indexOf(",5") >= 0) {
      registered = true;
      Serial.println("      ✓ Network registered");
      break;
    }
    Serial.print(".");
    delay(1000);
  }

  if (!registered) {
    Serial.println("\n      ⚠ Network not registered (continuing anyway)");
  }

  // Configure PDP context
  Serial.println("[4/5] Configuring PDP context...");
  String pdpCmd = "AT+CGDCONT=1,\"IP\",\"" + String(APN) + "\"";
  sendATCommand(pdpCmd, 2000);
  delay(500);

  // Dial PPP
  Serial.println("[5/5] Dialing PPP connection (ATD*99#)...");
  Serial.println("      Waiting for CONNECT...");

  ModemSerial.println("ATD*99#");

  // Wait for CONNECT
  unsigned long start = millis();
  bool gotConnect = false;
  String response = "";

  while (millis() - start < 10000) {
    while (ModemSerial.available()) {
      char c = (char)ModemSerial.read();
      Serial.write(c);
      response += c;

      if (response.indexOf("CONNECT") >= 0) {
        gotConnect = true;
        break;
      }

      if (response.indexOf("ERROR") >= 0 || response.indexOf("NO CARRIER") >= 0) {
        Serial.println("\n      ❌ Dial failed!");
        return;
      }
    }
    if (gotConnect) break;
    delay(10);
  }

  if (!gotConnect) {
    Serial.println("\n      ❌ No CONNECT response (timeout)");
    Serial.println("\nTroubleshooting:");
    Serial.println("1. Check SIM card is inserted and has data plan");
    Serial.println("2. Check APN is correct for your carrier");
    Serial.println("3. Check network registration (AT+CREG?)");
    Serial.println("4. Try manual PDP activation (AT+QIACT=1)");
    return;
  }

  Serial.println("\n\n✓ CONNECT received!");
  Serial.println("========================================");
  Serial.println("Modem is now in PPP mode.");
  Serial.println("========================================\n");

  Serial.println("NEXT STEPS:");
  Serial.println("1. Initialize ESP32 PPP client (esp_netif)");
  Serial.println("2. Feed serial data to PPP stack");
  Serial.println("3. Wait for IP_EVENT_PPP_GOT_IP event");
  Serial.println("4. Use standard networking libraries");
  Serial.println();
  Serial.println("For complete implementation, see PPPoSManager class.");
  Serial.println();

  // NOTE: At this point, modem is in PPP mode and ready for ESP32 PPP client
  // See PPPoSManager.cpp for full implementation with esp_netif
}

void loop() {
  // In PPP mode, you would continuously feed serial data to the PPP stack
  // Example:
  // while (ModemSerial.available()) {
  //   uint8_t c = ModemSerial.read();
  //   esp_netif_ppp_input(ppp_netif, &c, 1);
  // }

  delay(1000);
}
