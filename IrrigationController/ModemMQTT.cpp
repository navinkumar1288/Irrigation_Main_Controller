// ModemMQTT.cpp - MQTT communication for Quectel EC200U
#include "ModemMQTT.h"
#include <vector>

// Shared URC buffer for forwarding non-MQTT URCs to SMS handler
// MQTT processBackground() runs first and buffers URCs for SMS
static std::vector<String> sharedURCBuffer;

ModemMQTT::ModemMQTT() : mqttConnected(false), needsReconfigure(false), lastMqttCheck(0), mqttCheckInterval(30000), lastReconfigAttempt(0), reconfigAttempts(0), cooldownStartTime(0), inCooldown(false) {}

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
    needsReconfigure = false;  // Clear flag to prevent infinite loop
    return false;
  }

  Serial.println("[MQTT] Configuring...");

  // IMPORTANT: Clean up any existing MQTT connections first
  // This is critical after modem restart to clear old state
  Serial.println("[MQTT] Cleaning up old connections...");
  sendCommand("AT+QMTDISC=0", 2000);
  delay(500);
  sendCommand("AT+QMTCLOSE=0", 2000);
  delay(500);

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
    needsReconfigure = false;  // Clear flag even on failure to prevent infinite loop
    return false;
  }

  // Connect to MQTT broker
  if (!connectMQTTBroker()) {
    needsReconfigure = false;  // Clear flag even on failure to prevent infinite loop
    return false;
  }

  mqttConnected = true;
  needsReconfigure = false;  // Clear reconfiguration flag
  reconfigAttempts = 0;  // Reset attempt counter on success
  Serial.println("[MQTT] ✓ Connected and ready");

  return true;
}

bool ModemMQTT::openMQTTConnection() {
  Serial.println("[MQTT] Opening connection to broker...");

  String openCmd = "AT+QMTOPEN=0,\"" + String(MQTT_BROKER) + "\"," + String(MQTT_PORT);
  String openResp = sendCommand(openCmd, 5000);

  if (openResp.indexOf("OK") < 0) {
    Serial.println("[MQTT] ❌ Failed to send open command");
    return false;
  }

  // Wait for +QMTOPEN URC response (can take 10-15 seconds)
  // Format: +QMTOPEN: <client_idx>,<result>
  // result: 0=success, 1=wrong parameter, 2=MQTT ID occupied, 3=failed to activate PDP, 4=failed to parse domain, 5=network disconnected
  Serial.println("[MQTT] Waiting for +QMTOPEN URC...");

  unsigned long start = millis();
  bool openSuccess = false;

  while (millis() - start < 20000) {  // Wait up to 20 seconds
    if (SerialAT.available()) {
      String urc = SerialAT.readStringUntil('\n');
      urc.trim();

      if (urc.length() > 0) {
        Serial.println("[MQTT] URC: " + urc);

        if (urc.indexOf("+QMTOPEN:") >= 0) {
          // Parse result code
          int commaPos = urc.lastIndexOf(",");
          if (commaPos >= 0) {
            String resultStr = urc.substring(commaPos + 1);
            resultStr.trim();
            int result = resultStr.toInt();

            if (result == 0) {
              Serial.println("[MQTT] ✓ Connection opened successfully");
              openSuccess = true;
              break;
            } else {
              Serial.println("[MQTT] ❌ Open failed with error code: " + String(result));
              return false;
            }
          }
        }

        // Forward non-MQTT URCs to SMS handler
        if (urc.indexOf("+CMTI:") >= 0 || urc.indexOf("+CDS:") >= 0 || urc.indexOf("+CMGS:") >= 0) {
          Serial.println("[MQTT] Forwarding SMS URC: " + urc);
          sharedURCBuffer.push_back(urc);
        }
      }
    }
    delay(100);  // Small delay to prevent tight loop
  }

  if (!openSuccess) {
    Serial.println("[MQTT] ❌ Timeout waiting for +QMTOPEN URC");
    return false;
  }

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
    Serial.println("[MQTT] ❌ Failed to send connect command");
    return false;
  }

  // Wait for +QMTCONN URC response
  // Format: +QMTCONN: <client_idx>,<result>[,<ret_code>]
  // result: 0=success, 1=packet retransmit, 2=failed to send, 3=authentication error, 4=server unavailable
  Serial.println("[MQTT] Waiting for +QMTCONN URC...");

  unsigned long start = millis();
  bool connectSuccess = false;

  while (millis() - start < 15000) {  // Wait up to 15 seconds
    if (SerialAT.available()) {
      String urc = SerialAT.readStringUntil('\n');
      urc.trim();

      if (urc.length() > 0) {
        Serial.println("[MQTT] URC: " + urc);

        if (urc.indexOf("+QMTCONN:") >= 0) {
          // Parse result code
          // Format: +QMTCONN: 0,0,0 (client, result, ret_code)
          int firstComma = urc.indexOf(",");
          int secondComma = urc.indexOf(",", firstComma + 1);

          if (firstComma >= 0 && secondComma >= 0) {
            String resultStr = urc.substring(firstComma + 1, secondComma);
            resultStr.trim();
            int result = resultStr.toInt();

            if (result == 0) {
              Serial.println("[MQTT] ✓ Broker connected successfully");
              connectSuccess = true;
              break;
            } else {
              Serial.println("[MQTT] ❌ Connect failed with error code: " + String(result));
              return false;
            }
          }
        }

        // Forward non-MQTT URCs to SMS handler
        if (urc.indexOf("+CMTI:") >= 0 || urc.indexOf("+CDS:") >= 0 || urc.indexOf("+CMGS:") >= 0) {
          Serial.println("[MQTT] Forwarding SMS URC: " + urc);
          sharedURCBuffer.push_back(urc);
        }
      }
    }
    delay(100);  // Small delay to prevent tight loop
  }

  if (!connectSuccess) {
    Serial.println("[MQTT] ❌ Timeout waiting for +QMTCONN URC");
    return false;
  }

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
        reconfigAttempts = 0;  // Reset attempt counter for new modem restart event
        inCooldown = false;  // Clear cooldown on modem restart - give MQTT a fresh chance
        cooldownStartTime = 0;

        // CRITICAL: Mark modem as not ready until it fully initializes
        // Modem sends RDY immediately but takes 5+ seconds to actually be ready
        modemReady = false;
        Serial.println("[MQTT] → Modem marked as not ready (waiting for +QIND: SMS DONE)");

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
  
  // Auto-reconnect if disconnected (but not during cooldown period)
  if (!mqttConnected && modemReady && !inCooldown) {
    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt > 60000) {  // Try every 60 seconds
      lastReconnectAttempt = millis();
      Serial.println("[MQTT] Auto-reconnecting...");
      reconnect();
    }
  }
}

bool ModemMQTT::needsReconfiguration() {
  if (!needsReconfigure) {
    return false;
  }

  unsigned long now = millis();

  // Check if we're in 1-hour cooldown period
  if (inCooldown) {
    const unsigned long COOLDOWN_DURATION = 3600000;  // 1 hour in milliseconds

    if (now - cooldownStartTime >= COOLDOWN_DURATION) {
      // Cooldown period ended - reset and try again
      Serial.println("[MQTT] ⏰ Cooldown period ended (1 hour), will retry connection");
      inCooldown = false;
      reconfigAttempts = 0;
      cooldownStartTime = 0;
      needsReconfigure = true;  // Re-enable reconfiguration
    } else {
      // Still in cooldown - don't try to reconnect
      unsigned long remainingMinutes = (COOLDOWN_DURATION - (now - cooldownStartTime)) / 60000;
      if (reconfigAttempts == 0) {  // Only print once to avoid spam
        Serial.println("[MQTT] ⏸ In cooldown period. Will retry in ~" + String(remainingMinutes) + " minutes");
        Serial.println("[MQTT] ℹ SMS commands work independently of MQTT");
        reconfigAttempts = 1;  // Set to 1 to prevent repeated printing
      }
      return false;  // Don't try to reconfigure during cooldown
    }
  }

  // Throttle reconfiguration attempts - don't try too frequently
  unsigned long minInterval = 10000;  // Wait at least 10 seconds between attempts

  if (now - lastReconfigAttempt < minInterval) {
    return false;  // Too soon, wait longer
  }

  // Limit to 3 attempts, then enter 1-hour cooldown
  if (reconfigAttempts >= 3) {
    Serial.println("[MQTT] ⚠ Max reconfiguration attempts (3) reached");
    Serial.println("[MQTT] ⏸ Entering 1-hour cooldown period");
    Serial.println("[MQTT] ℹ SMS commands will continue to work normally");
    needsReconfigure = false;  // Clear flag
    inCooldown = true;
    cooldownStartTime = now;
    reconfigAttempts = 0;  // Reset for next time
    return false;
  }

  // Update tracking
  lastReconfigAttempt = now;
  reconfigAttempts++;

  Serial.println("[MQTT] Reconfiguration attempt " + String(reconfigAttempts) + "/3");
  return true;
}

// Provide access to shared URC buffer for SMS handler
std::vector<String>& ModemMQTT::getSharedURCBuffer() {
  return sharedURCBuffer;
}
