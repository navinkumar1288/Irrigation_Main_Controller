// MQTTComm.cpp - MQTT communication using ESP32 native networking
#include "MQTTComm.h"

// Static callback wrapper for PubSubClient
static MQTTComm *mqttInstance = nullptr;

static void mqttCallbackWrapper(char *topic, byte *payload, unsigned int length) {
  if (mqttInstance && mqttInstance->getClient().connected()) {
    String topicStr = String(topic);
    String payloadStr = "";
    payloadStr.reserve(length + 1);

    for (unsigned int i = 0; i < length; i++) {
      payloadStr += (char)payload[i];
    }

    Serial.println("[MQTT] ← " + topicStr + ": " + payloadStr);

    // Call user callback if set
    // Note: We'll handle this via the message callback in the class
  }
}

MQTTComm::MQTTComm() : mqtt(espClient), configured(false), lastReconnectAttempt(0), reconnectInterval(5000), messageCallback(nullptr) {
  mqttInstance = this;
}

bool MQTTComm::init() {
  Serial.println("[MQTT] Initializing MQTT client...");

  // Configure MQTT client
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallbackWrapper);
  mqtt.setBufferSize(512);  // Increase buffer for larger messages
  mqtt.setKeepAlive(120);   // 2 minutes keep-alive

  Serial.println("[MQTT] ✓ MQTT client initialized");
  Serial.println("[MQTT] Broker: " + String(MQTT_BROKER) + ":" + String(MQTT_PORT));

  return true;
}

bool MQTTComm::configure() {
  Serial.println("[MQTT] Configuring MQTT connection...");

  // Check if we have network connectivity (PPPoS or WiFi)
  // WiFiClient will work with either PPPoS or WiFi
  // No explicit check needed - connection attempt will fail if no network

  if (!attemptConnection()) {
    Serial.println("[MQTT] ❌ Initial connection failed");
    return false;
  }

  configured = true;
  Serial.println("[MQTT] ✓ MQTT configured and connected");

  return true;
}

bool MQTTComm::attemptConnection() {
  Serial.println("[MQTT] Connecting to broker...");
  Serial.println("[MQTT] Client ID: " + String(MQTT_CLIENT_ID));

  // Attempt connection with credentials
  bool connected = mqtt.connect(
    MQTT_CLIENT_ID,
    MQTT_USER,
    MQTT_PASS
  );

  if (connected) {
    Serial.println("[MQTT] ✓ Connected to broker");
    return true;
  } else {
    int state = mqtt.state();
    Serial.print("[MQTT] ❌ Connection failed, state: ");

    switch (state) {
      case -4:
        Serial.println("MQTT_CONNECTION_TIMEOUT");
        break;
      case -3:
        Serial.println("MQTT_CONNECTION_LOST");
        break;
      case -2:
        Serial.println("MQTT_CONNECT_FAILED");
        break;
      case -1:
        Serial.println("MQTT_DISCONNECTED");
        break;
      case 1:
        Serial.println("MQTT_CONNECT_BAD_PROTOCOL");
        break;
      case 2:
        Serial.println("MQTT_CONNECT_BAD_CLIENT_ID");
        break;
      case 3:
        Serial.println("MQTT_CONNECT_UNAVAILABLE");
        break;
      case 4:
        Serial.println("MQTT_CONNECT_BAD_CREDENTIALS");
        break;
      case 5:
        Serial.println("MQTT_CONNECT_UNAUTHORIZED");
        break;
      default:
        Serial.println(String(state));
        break;
    }

    return false;
  }
}

bool MQTTComm::publish(const String &topic, const String &payload) {
  if (!mqtt.connected()) {
    Serial.println("[MQTT] ❌ Cannot publish - not connected");
    return false;
  }

  Serial.println("[MQTT] → " + topic + ": " + payload);

  bool result = mqtt.publish(topic.c_str(), payload.c_str());

  if (result) {
    Serial.println("[MQTT] ✓ Published");
  } else {
    Serial.println("[MQTT] ❌ Publish failed");
  }

  return result;
}

bool MQTTComm::subscribe(const String &topic) {
  if (!mqtt.connected()) {
    Serial.println("[MQTT] ❌ Cannot subscribe - not connected");
    return false;
  }

  Serial.println("[MQTT] Subscribing to: " + topic);

  bool result = mqtt.subscribe(topic.c_str());

  if (result) {
    Serial.println("[MQTT] ✓ Subscribed");
  } else {
    Serial.println("[MQTT] ❌ Subscribe failed");
  }

  return result;
}

bool MQTTComm::isConnected() {
  return mqtt.connected();
}

void MQTTComm::reconnect() {
  // Throttle reconnection attempts
  if (millis() - lastReconnectAttempt < reconnectInterval) {
    return;
  }

  lastReconnectAttempt = millis();

  if (!mqtt.connected()) {
    Serial.println("[MQTT] Reconnecting...");

    if (attemptConnection()) {
      Serial.println("[MQTT] ✓ Reconnected successfully");

      // Resubscribe to topics if needed
      // (Application should call subscribe() after reconnection)
    } else {
      Serial.println("[MQTT] ❌ Reconnection failed, will retry in " +
                     String(reconnectInterval / 1000) + " seconds");
    }
  }
}

void MQTTComm::processBackground() {
  // Process MQTT loop (handles keep-alive, incoming messages, etc.)
  if (mqtt.connected()) {
    mqtt.loop();
  } else {
    // Attempt reconnection if configured
    if (configured) {
      reconnect();
    }
  }
}

void MQTTComm::setMessageCallback(MQTTMessageCallback callback) {
  messageCallback = callback;
}

PubSubClient& MQTTComm::getClient() {
  return mqtt;
}
