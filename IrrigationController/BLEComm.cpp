#include "BLEComm.h"
#include "MessageQueue.h"

// Server callbacks
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    bleComm.setConnected(true);
    Serial.println("[BLE] Client connected");
  }
  
  void onDisconnect(BLEServer* pServer) override {
    bleComm.setConnected(false);
    Serial.println("[BLE] Client disconnected");
    BLEDevice::startAdvertising();
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
  
  BLEDevice::init("IrrigCtrl");
  
  server = BLEDevice::createServer();
  if (!server) {
    Serial.println("❌ BLE server creation failed");
    return false;
  }
  
  server->setCallbacks(new MyServerCallbacks());
  
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
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x00);
  
  BLEAdvertisementData advData;
  advData.setName("IrrigCtrl");
  advData.setFlags(0x06);
  pAdvertising->setAdvertisementData(advData);
  
  BLEAdvertisementData scanResp;
  scanResp.setName("IrrigCtrl");
  pAdvertising->setScanResponseData(scanResp);
  
  BLEDevice::startAdvertising();
  
  Serial.println("✓ BLE initialized, advertising as: IrrigCtrl");
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