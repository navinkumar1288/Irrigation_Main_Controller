// MQTTComm.h - MQTT communication using ESP32 native networking
// Works with both PPPoS (cellular) and WiFi connections
#ifndef MQTT_COMM_H
#define MQTT_COMM_H

#include <Arduino.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "Config.h"

// Callback type for incoming MQTT messages
typedef void (*MQTTMessageCallback)(const String &topic, const String &payload);

class MQTTComm {
private:
  WiFiClient espClient;
  PubSubClient mqtt;
  bool configured;
  unsigned long lastReconnectAttempt;
  unsigned long reconnectInterval;
  MQTTMessageCallback messageCallback;

  // Internal reconnection logic
  bool attemptConnection();

public:
  MQTTComm();

  // Initialize MQTT (no network connection needed yet)
  bool init();

  // Configure MQTT connection (requires active network: PPPoS or WiFi)
  bool configure();

  // Publish message to topic
  bool publish(const String &topic, const String &payload);

  // Subscribe to topic
  bool subscribe(const String &topic);

  // Check if connected to MQTT broker
  bool isConnected();

  // Attempt reconnection if disconnected
  void reconnect();

  // Process background tasks (must be called in loop)
  void processBackground();

  // Set callback for incoming messages
  void setMessageCallback(MQTTMessageCallback callback);

  // Get underlying PubSubClient for advanced usage
  PubSubClient& getClient();
};

#endif
