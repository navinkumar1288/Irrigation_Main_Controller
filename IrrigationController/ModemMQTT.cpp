// ModemMQTT.cpp - MQTT communication for Quectel EC200U
#include "ModemMQTT.h"
#include <vector>

// Shared URC buffer for forwarding non-MQTT URCs to SMS handler
// MQTT processBackground() runs first and buffers URCs for SMS
static std::vector<String> sharedURCBuffer;

ModemMQTT::ModemMQTT() : mqttConnected(false), needsReconfigure(false), lastMqttCheck(0), mqttCheckInterval(30000) {}

// Escape quotes and backslashes in strings for AT commands
String ModemMQTT::escapeATString(const String &input) {
  String result = "";
  result.reserve(input.length() + 10);  // Reserve some extra space

  for (unsigned int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if (c == '"' || c == '\\') {
      result += '\\';  // Escape quote and backslash
    }
    result += c;
  }

  return result;
}

bool ModemMQTT::configure() {
  if (!modemReady) {
    Serial.println("[MQTT] ❌ Modem not ready for MQTT");
    return false;
  }
  
  Serial.println("[MQTT] Configuring...");
  
  // Configure MQTT connection for EC200U
  // AT+QMTCFG="version",<client_idx>,<vsn>
  sendCommand("AT+QMTCFG=\"version\",0,4", 2000);  // MQTT 3.1.1
  
  // Set keep-alive
  sendCommand("AT+QMTCFG=\"keepalive\",0,120", 2000);
  
  // Set clean session
  sendCommand("AT+QMTCFG=\"session\",0,0", 2000);
  
  // Set timeout
  sendCommand("AT+QMTCFG=\"timeout\",0,30,3,0", 2000);
  
  // Open MQTT connection
  if (!openMQTTConnection()) {
    return false;
  }
  
  // Connect to MQTT broker
  if (!connectMQTTBroker()) {
    return false;
  }
  
  mqttConnected = true;
  needsReconfigure = false;  // Clear reconfiguration flag
  Serial.println("[MQTT] ✓ Connected and ready");

  return true;
}

bool ModemMQTT::openMQTTConnection() {
  Serial.println("[MQTT] Opening connection to broker...");
  
  String openCmd = "AT+QMTOPEN=0,\"" + String(MQTT_BROKER) + "\"," + String(MQTT_PORT);
  String openResp = sendCommand(openCmd, 5000);
  
  if (openResp.indexOf("OK") < 0) {
    Serial.println("[MQTT] ❌ Failed to open connection");
    return false;
  }
  
  // Wait for QMTOPEN URC response
  delay(2000);
  
  Serial.println("[MQTT] ✓ Connection opened");
  return true;
}

bool ModemMQTT::connectMQTTBroker() {
  Serial.println("[MQTT] Connecting to broker...");
  
  String connectCmd = "AT+QMTCONN=0,\"" + String(MQTT_CLIENT_ID) + "\"";
  if (strlen(MQTT_USER) > 0) {
    connectCmd += ",\"" + String(MQTT_USER) + "\",\"" + String(MQTT_PASS) + "\"";
  }
  
  String connectResp = sendCommand(connectCmd, 5000);
  
  if (connectResp.indexOf("OK") < 0) {
    Serial.println("[MQTT] ❌ Failed to connect to broker");
    return false;
  }
  
  // Wait for QMTCONN URC response
  delay(3000);
  
  Serial.println("[MQTT] ✓ Broker connected");
  return true;
}

bool ModemMQTT::publish(const String &topic, const String &payload) {
  if (!mqttConnected) {
    Serial.println("[MQTT] ❌ Not connected - attempting reconnect");
    reconnect();
    if (!mqttConnected) {
      return false;
    }
  }

  // Escape topic and payload to prevent command injection
  String escapedTopic = escapeATString(topic);
  String escapedPayload = escapeATString(payload);

  // Publish message
  // AT+QMTPUB=<client_idx>,<msgID>,<qos>,<retain>,"<topic>","<msg>"
  String pubCmd = "AT+QMTPUB=0,0,0,0,\"" + escapedTopic + "\",\"" + escapedPayload + "\"";

  Serial.println("[MQTT] Publishing to topic: " + topic);
  Serial.println("[MQTT] Payload: " + payload);

  String resp = sendCommand(pubCmd, 5000);

  if (resp.indexOf("OK") >= 0) {
    Serial.println("[MQTT] ✓ Published successfully");
    return true;
  } else {
    Serial.println("[MQTT] ❌ Publish failed");
    mqttConnected = false;  // Mark as disconnected
    return false;
  }
}

bool ModemMQTT::subscribe(const String &topic) {
  if (!mqttConnected) {
    Serial.println("[MQTT] ❌ Not connected");
    return false;
  }

  // Escape topic to prevent command injection
  String escapedTopic = escapeATString(topic);

  // Subscribe to topic
  // AT+QMTSUB=<client_idx>,<msgID>,"<topic>",<qos>
  String subCmd = "AT+QMTSUB=0,1,\"" + escapedTopic + "\",0";

  Serial.println("[MQTT] Subscribing to topic: " + topic);

  String resp = sendCommand(subCmd, 5000);

  if (resp.indexOf("OK") >= 0) {
    Serial.println("[MQTT] ✓ Subscribed successfully");
    return true;
  } else {
    Serial.println("[MQTT] ❌ Subscribe failed");
    return false;
  }
}

bool ModemMQTT::isConnected() {
  return mqttConnected;
}

void ModemMQTT::reconnect() {
  Serial.println("[MQTT] Attempting reconnection...");
  
  // Close existing connection
  sendCommand("AT+QMTDISC=0", 2000);
  delay(1000);
  sendCommand("AT+QMTCLOSE=0", 2000);
  delay(1000);
  
  // Reconfigure and connect
  if (configure()) {
    Serial.println("[MQTT] ✓ Reconnected successfully");
  } else {
    Serial.println("[MQTT] ❌ Reconnection failed");
  }
}

void ModemMQTT::processBackground() {
  // Process MQTT-specific URCs
  // Note: Don't call ModemBase::processBackground() because it consumes
  // all SerialAT data, leaving nothing for us to process!
  while (SerialAT.available()) {
    String urc = SerialAT.readStringUntil('\n');
    urc.trim();

    if (urc.length() > 0) {
      // Check if this is an MQTT-related URC
      bool isMQTTURC = (urc.indexOf("+QMTSTAT") >= 0 ||
                        urc.indexOf("+QMTRECV") >= 0 ||
                        urc.indexOf("+QMTPUB") >= 0 ||
                        urc.indexOf("+QMTSUB") >= 0 ||
                        urc.indexOf("+QMTOPEN") >= 0 ||
                        urc.indexOf("+QMTCONN") >= 0 ||
                        urc.indexOf("+QMTDISC") >= 0);

      // Check if this is a shared URC (modem restart)
      bool isSharedURC = (urc.indexOf("RDY") >= 0 ||
                          urc.indexOf("POWERED DOWN") >= 0);

      // If it's not an MQTT or shared URC, buffer it for SMS handler
      if (!isMQTTURC && !isSharedURC) {
        Serial.println("[MQTT] Forwarding non-MQTT URC to SMS: " + urc);
        sharedURCBuffer.push_back(urc);
        continue;  // Skip processing this URC
      }

      // Process MQTT and shared URCs
      Serial.println("[MQTT] URC: " + urc);

      // Handle modem restart/reboot
      // When modem restarts, all configuration is lost (including MQTT)
      if (isSharedURC) {
        Serial.println("[MQTT] ⚠ Modem restart detected!");

        // Reset state and mark for reconfiguration
        mqttConnected = false;
        needsReconfigure = true;

        Serial.println("[MQTT] → MQTT marked for reconfiguration");

        // Also forward to SMS handler
        sharedURCBuffer.push_back(urc);
      }

      // Handle MQTT disconnection
      // +QMTSTAT: <client_idx>,<err_code>
      if (urc.indexOf("+QMTSTAT") >= 0) {
        // Error code 2 = connection closed
        if (urc.indexOf(",2") >= 0 || urc.indexOf(",1") >= 0) {
          Serial.println("[MQTT] ⚠ Disconnected (URC)");
          mqttConnected = false;
        }
      }

      // Handle incoming messages
      // +QMTRECV: <client_idx>,<msgID>,"<topic>","<payload>"
      if (urc.indexOf("+QMTRECV") >= 0) {
        Serial.println("[MQTT] 📨 Received message: " + urc);
        // Parse and handle message here
        // You can add a callback mechanism if needed
      }

      // Handle publish confirmation
      if (urc.indexOf("+QMTPUB") >= 0) {
        Serial.println("[MQTT] ✓ Publish confirmed");
      }

      // Handle subscription confirmation
      if (urc.indexOf("+QMTSUB") >= 0) {
        Serial.println("[MQTT] ✓ Subscription confirmed");
      }
    }
  }
  
  // Periodic connection check
  if (mqttConnected && (millis() - lastMqttCheck > mqttCheckInterval)) {
    lastMqttCheck = millis();
    
    // Send a keep-alive check (can use AT+QMTSTAT or just track URCs)
    Serial.println("[MQTT] Connection status check...");
  }
  
  // Auto-reconnect if disconnected
  if (!mqttConnected && modemReady) {
    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt > 60000) {  // Try every 60 seconds
      lastReconnectAttempt = millis();
      Serial.println("[MQTT] Auto-reconnecting...");
      reconnect();
    }
  }
}

bool ModemMQTT::needsReconfiguration() {
  return needsReconfigure;
}

// Provide access to shared URC buffer for SMS handler
std::vector<String>& ModemMQTT::getSharedURCBuffer() {
  return sharedURCBuffer;
}
