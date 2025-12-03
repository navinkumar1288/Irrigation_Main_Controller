// NetworkManager.cpp - Unified network connection manager with automatic fallback
#include "NetworkManager.h"
#include "PPPoSManager.h"
#include "WiFiComm.h"

NetworkManager::NetworkManager()
  : ppposManager(nullptr),
    wifiComm(nullptr),
    modemSerial(nullptr),
    activeConnection(ConnectionType::NONE),
    state(NetworkState::DISCONNECTED),
    localIP(""),
    lastConnectionAttempt(0),
    reconnectInterval(60000) {  // Default 60 second reconnect interval
}

void NetworkManager::init(PPPoSManager* pppos, WiFiComm* wifi, HardwareSerial* serial) {
  Serial.println("[NetMgr] Initializing Network Manager...");

  ppposManager = pppos;
  wifiComm = wifi;
  modemSerial = serial;

  Serial.println("[NetMgr] ✓ Network Manager initialized");
  Serial.println("[NetMgr] Fallback order: PPPoS → WiFi");
}

bool NetworkManager::connect(uint32_t pppos_timeout_ms, uint32_t wifi_timeout_ms) {
  Serial.println("[NetMgr] ========================================");
  Serial.println("[NetMgr] Starting network connection...");
  Serial.println("[NetMgr] ========================================");

  lastConnectionAttempt = millis();

#if ENABLE_PPPOS
  // Try PPPoS first (cellular data via modem)
  if (ppposManager != nullptr) {
    Serial.println("[NetMgr] [1/2] Attempting PPPoS connection...");
    if (tryPPPoS(pppos_timeout_ms)) {
      activeConnection = ConnectionType::PPPOS;
      state = NetworkState::CONNECTED;
      localIP = ppposManager->getLocalIP();

      Serial.println("[NetMgr] ========================================");
      Serial.println("[NetMgr] ✓ NETWORK CONNECTED VIA PPPOS");
      Serial.println("[NetMgr] ========================================");
      Serial.println("[NetMgr] Connection Type: Cellular (PPP)");
      Serial.println("[NetMgr] IP Address: " + localIP);
      Serial.println("[NetMgr] ========================================");

      return true;
    }

    Serial.println("[NetMgr] ❌ PPPoS connection failed");
    Serial.println("[NetMgr] → Falling back to WiFi...");
  }
#endif

#if ENABLE_WIFI
  // Fallback to WiFi
  if (wifiComm != nullptr) {
    Serial.println("[NetMgr] [2/2] Attempting WiFi connection...");
    if (tryWiFi(wifi_timeout_ms)) {
      activeConnection = ConnectionType::WIFI;
      state = NetworkState::CONNECTED;
      localIP = wifiComm->getLocalIP();

      Serial.println("[NetMgr] ========================================");
      Serial.println("[NetMgr] ✓ NETWORK CONNECTED VIA WIFI");
      Serial.println("[NetMgr] ========================================");
      Serial.println("[NetMgr] Connection Type: WiFi");
      Serial.println("[NetMgr] SSID: " + String(WIFI_SSID));
      Serial.println("[NetMgr] IP Address: " + localIP);
      Serial.println("[NetMgr] ========================================");

      return true;
    }

    Serial.println("[NetMgr] ❌ WiFi connection failed");
  }
#endif

  // Both failed
  activeConnection = ConnectionType::NONE;
  state = NetworkState::DISCONNECTED;
  localIP = "";

  Serial.println("[NetMgr] ========================================");
  Serial.println("[NetMgr] ❌ ALL NETWORK CONNECTIONS FAILED");
  Serial.println("[NetMgr] ========================================");
  Serial.println("[NetMgr] PPPoS: Failed");
  Serial.println("[NetMgr] WiFi: Failed");
  Serial.println("[NetMgr] → Will retry in " + String(reconnectInterval / 1000) + " seconds");
  Serial.println("[NetMgr] ========================================");

  return false;
}

bool NetworkManager::tryPPPoS(uint32_t timeout_ms) {
#if ENABLE_PPPOS
  if (ppposManager == nullptr || modemSerial == nullptr) {
    Serial.println("[NetMgr] ❌ PPPoS manager not initialized");
    return false;
  }

  state = NetworkState::CONNECTING_PPPOS;

  Serial.println("[NetMgr] → Initializing PPPoS...");
  if (!ppposManager->init(modemSerial, PPPOS_APN)) {
    Serial.println("[NetMgr] ❌ PPPoS initialization failed");
    return false;
  }

  Serial.println("[NetMgr] → Connecting to cellular network...");
  Serial.println("[NetMgr] APN: " + String(PPPOS_APN));
  Serial.println("[NetMgr] Timeout: " + String(timeout_ms / 1000) + " seconds");

  if (ppposManager->connect(timeout_ms)) {
    Serial.println("[NetMgr] ✓ PPPoS connected!");
    Serial.println("[NetMgr] ✓ IP: " + ppposManager->getLocalIP());
    return true;
  }

  Serial.println("[NetMgr] ❌ PPPoS connection timeout");
  return false;
#else
  Serial.println("[NetMgr] ⚠ PPPoS disabled in Config.h");
  return false;
#endif
}

bool NetworkManager::tryWiFi(uint32_t timeout_ms) {
#if ENABLE_WIFI
  if (wifiComm == nullptr) {
    Serial.println("[NetMgr] ❌ WiFi manager not initialized");
    return false;
  }

  state = NetworkState::CONNECTING_WIFI;

  Serial.println("[NetMgr] → Initializing WiFi...");
  if (!wifiComm->init()) {
    Serial.println("[NetMgr] ❌ WiFi initialization failed");
    return false;
  }

  Serial.println("[NetMgr] → Connecting to WiFi network...");
  Serial.println("[NetMgr] SSID: " + String(WIFI_SSID));
  Serial.println("[NetMgr] Timeout: " + String(timeout_ms / 1000) + " seconds");

  if (wifiComm->connect(timeout_ms)) {
    Serial.println("[NetMgr] ✓ WiFi connected!");
    Serial.println("[NetMgr] ✓ IP: " + wifiComm->getLocalIP());
    return true;
  }

  Serial.println("[NetMgr] ❌ WiFi connection timeout");
  return false;
#else
  Serial.println("[NetMgr] ⚠ WiFi disabled in Config.h");
  return false;
#endif
}

bool NetworkManager::isConnected() {
  // Check active connection status
  switch (activeConnection) {
    case ConnectionType::PPPOS:
#if ENABLE_PPPOS
      if (ppposManager != nullptr) {
        bool connected = ppposManager->isConnected();
        if (!connected) {
          // Connection was lost
          Serial.println("[NetMgr] ⚠ PPPoS connection lost");
          state = NetworkState::DISCONNECTED;
          activeConnection = ConnectionType::NONE;
          localIP = "";
        }
        return connected;
      }
#endif
      return false;

    case ConnectionType::WIFI:
#if ENABLE_WIFI
      if (wifiComm != nullptr) {
        bool connected = wifiComm->isConnected();
        if (!connected) {
          // Connection was lost
          Serial.println("[NetMgr] ⚠ WiFi connection lost");
          state = NetworkState::DISCONNECTED;
          activeConnection = ConnectionType::NONE;
          localIP = "";
        }
        return connected;
      }
#endif
      return false;

    case ConnectionType::NONE:
    default:
      return false;
  }
}

ConnectionType NetworkManager::getConnectionType() {
  return activeConnection;
}

String NetworkManager::getLocalIP() {
  return localIP;
}

NetworkState NetworkManager::getState() {
  return state;
}

void NetworkManager::processBackground() {
  // If disconnected and enough time has passed, attempt reconnection
  if (!isConnected() && state == NetworkState::DISCONNECTED) {
    if (millis() - lastConnectionAttempt >= reconnectInterval) {
      Serial.println("[NetMgr] → Attempting automatic reconnection...");
      connect();
    }
  }

#if ENABLE_PPPOS
  // If connected via PPPoS, must call loop() to feed data to PPP stack
  if (activeConnection == ConnectionType::PPPOS && ppposManager != nullptr) {
    ppposManager->loop();
  }
#endif
}

bool NetworkManager::disconnect() {
  Serial.println("[NetMgr] Disconnecting network...");

  bool success = false;

  switch (activeConnection) {
    case ConnectionType::PPPOS:
#if ENABLE_PPPOS
      if (ppposManager != nullptr) {
        success = ppposManager->disconnect();
        Serial.println("[NetMgr] PPPoS disconnected");
      }
#endif
      break;

    case ConnectionType::WIFI:
#if ENABLE_WIFI
      if (wifiComm != nullptr) {
        success = wifiComm->disconnect();
        Serial.println("[NetMgr] WiFi disconnected");
      }
#endif
      break;

    case ConnectionType::NONE:
      Serial.println("[NetMgr] No active connection to disconnect");
      success = true;
      break;
  }

  activeConnection = ConnectionType::NONE;
  state = NetworkState::DISCONNECTED;
  localIP = "";

  return success;
}

void NetworkManager::setReconnectInterval(unsigned long interval_ms) {
  reconnectInterval = interval_ms;
  Serial.println("[NetMgr] Reconnect interval set to " + String(interval_ms / 1000) + " seconds");
}
