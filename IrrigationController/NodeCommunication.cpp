// NodeCommunication.cpp - Handles all LoRa node communications
#include "NodeCommunication.h"
#include "MessageQueue.h"

extern MessageQueue incomingQueue;

NodeCommunication::NodeCommunication() : loraComm(nullptr), initialized(false) {}

bool NodeCommunication::init(LoRaComm* lora) {
  if (lora == nullptr) {
    Serial.println("[NodeComm] ❌ LoRa pointer is null");
    return false;
  }

  loraComm = lora;
  initialized = true;
  Serial.println("[NodeComm] ✓ Initialized");
  return true;
}

bool NodeCommunication::isInitialized() {
  return initialized;
}

// Send command to node (main to node)
bool NodeCommunication::sendCommand(int nodeId, const String &command) {
  if (!initialized || loraComm == nullptr) {
    Serial.println("[NodeComm] ❌ Not initialized");
    return false;
  }

  if (nodeId < 1 || nodeId > 255) {
    Serial.println("[NodeComm] ❌ Invalid node ID: " + String(nodeId));
    return false;
  }

  Serial.println("[NodeComm] → Sending to Node " + String(nodeId) + ": " + command);
  bool result = loraComm->sendWithAck(command, nodeId, "", 0, 0);

  if (result) {
    Serial.println("[NodeComm] ✓ Node " + String(nodeId) + " acknowledged");
  } else {
    Serial.println("[NodeComm] ✗ Node " + String(nodeId) + " timeout");
  }

  return result;
}

// Send command and wait for specific response
bool NodeCommunication::sendCommandWithResponse(int nodeId, const String &command, String &response) {
  if (!sendCommand(nodeId, command)) {
    return false;
  }

  // Response would be in the LoRa acknowledgment
  // For now, just return success if ACK received
  response = "ACK";
  return true;
}

// Process incoming messages from nodes (node to main)
void NodeCommunication::processIncoming() {
  if (!initialized || loraComm == nullptr) {
    return;
  }

  loraComm->processIncoming();
}

// Get node status
String NodeCommunication::getNodeStatus(int nodeId) {
  if (!initialized) {
    return "NodeComm not initialized";
  }

  // Could implement node tracking here
  return "Node " + String(nodeId) + " status unknown";
}
