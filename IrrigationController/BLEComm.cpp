#include "BLEComm.h"
#include "MessageQueue.h"

// Server callbacks
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    bleComm.setConnected(true);
    Serial.println("[BLE] Client connected");

    // Stop advertising when connected (reduce BLE overhead)
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    if (pAdvertising) {
      pAdvertising->stop();
    }

    // Note: MTU negotiation happens automatically - no manual intervention needed
    Serial.println("[BLE] Connection established, MTU negotiation in progress");
  }

  void onDisconnect(BLEServer* pServer) override {
    bleComm.setConnected(false);
    Serial.println("[BLE] Client disconnected");

    // Small delay before restarting advertising to prevent rapid reconnect issues
    delay(500);
    BLEDevice::startAdvertising();
    Serial.println("[BLE] Advertising restarted");
  }
};

// Characteristic callbacks
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    auto value = pChar->getValue();
    String payload = String(value.c_str());
    payload.trim();
    
    Serial.println("[BLE] RX: " + payload);
    
    if (payload.length() == 0) return;
    
    String response = "";
    
    // Check if it's a simple command: <node> <command>
    int space = payload.indexOf(' ');
    if (space > 0 && !payload.startsWith("SCH|") && !payload.startsWith("{")) {
      // It's a simple command like "1 PING"
      int node = payload.substring(0, space).toInt();
      String cmd = payload.substring(space + 1);
      cmd.toUpperCase();
      cmd.trim();
      
      if (node > 0 && node <= 255 && cmd.length() > 0) {
        Serial.printf("[BLE] Command for Node %d: %s\n", node, cmd.c_str());
        
        // Use callback instead of direct LoRa access
        if (bleComm.commandCallback != nullptr) {
          bleComm.commandCallback(node, cmd);
          response = "OK|Command sent to node " + String(node);
        } else {
          response = "ERROR|No command handler";
        }
      } else {
        response = "ERROR|Invalid format. Use: <node> <command>";
        Serial.println("[BLE] Invalid command format");
      }
    }
    // It's a schedule or other message - queue it
    else {
      if (payload.indexOf("SRC=") < 0) {
        payload += ",SRC=BT";
      }
      
      incomingQueue.enqueue(payload);
      response = "QUEUED|Message queued for processing";
      Serial.println("[BLE] Message queued");
    }
    
    // Send response
    if (response.length() > 0 && bleComm.isConnected()) {
      bleComm.notify(response);
    }
  }
};

BLEComm::BLEComm() : server(nullptr), txChar(nullptr), rxChar(nullptr), connected(false), commandCallback(nullptr) {}

bool BLEComm::init() {
  Serial.println("[BLE] Initializing...");

  // Initialize BLE with device name from Config.h
  BLEDevice::init(BLE_DEVICE_NAME);

  server = BLEDevice::createServer();
  if (!server) {
    Serial.println("❌ BLE server creation failed");
    return false;
  }

  server->setCallbacks(new MyServerCallbacks());

  // NOTE: MTU negotiation happens automatically during connection.
  // Do NOT call BLEDevice::setMTU() here - it forces MTU requirements
  // that many clients cannot meet, causing connection failures.

  BLEService *pService = server->createService(SERVICE_UUID);
  if (!pService) {
    Serial.println("❌ BLE service creation failed");
    return false;
  }

  txChar = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY
  );

  if (txChar) {
    txChar->addDescriptor(new BLE2902());
    txChar->setValue("OK");
  } else {
    Serial.println("⚠ TX characteristic creation failed");
  }

  rxChar = pService->createCharacteristic(
    CHARACTERISTIC_UUID_RX,
    BLECharacteristic::PROPERTY_WRITE
  );

  if (rxChar) {
    rxChar->setCallbacks(new MyCharacteristicCallbacks());
  } else {
    Serial.println("⚠ RX characteristic creation failed");
  }

  pService->start();

  // Configure advertising with proper connection parameters
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);

  // CRITICAL: Set to 0x00 (no preference) to let client choose intervals
  // Forcing specific intervals causes many clients to reject the connection
  pAdvertising->setMinPreferred(0x00);  // No minimum preference - compatible with all clients

  BLEAdvertisementData advData;
  advData.setName(BLE_DEVICE_NAME);
  advData.setFlags(0x06);  // General discoverable, BR/EDR not supported

  // Add manufacturer data (improves device recognition on some clients)
  String mfr = String("\x01\x02\x03\x04");
  advData.setManufacturerData(mfr);

  pAdvertising->setAdvertisementData(advData);

  BLEAdvertisementData scanResp;
  scanResp.setName(BLE_DEVICE_NAME);
  pAdvertising->setScanResponseData(scanResp);

  BLEDevice::startAdvertising();

  Serial.println("✓ BLE initialized, advertising as: " + String(BLE_DEVICE_NAME));
  Serial.println("  MTU: Auto-negotiated during connection");
  Serial.println("  Connection interval: No preference (client decides)");
  return true;
}

bool BLEComm::notify(const String &msg) {
  if (!connected || !txChar) {
    return false;
  }
  
  String m = msg;
  if (m.length() > 200) {
    m = m.substring(0, 200);
  }
  
  txChar->setValue((uint8_t*)m.c_str(), m.length());
  txChar->notify();
  
  Serial.println("[BLE] TX: " + m);
  delay(10);
  
  return true;
}

bool BLEComm::isConnected() {
  return connected;
}

void BLEComm::setConnected(bool state) {
  connected = state;
}

void BLEComm::setCommandCallback(BLECommandCallback callback) {
  commandCallback = callback;
}