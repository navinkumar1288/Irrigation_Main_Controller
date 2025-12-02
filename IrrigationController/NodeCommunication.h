// NodeCommunication.h - Handles all LoRa node communications
#ifndef NODE_COMMUNICATION_H
#define NODE_COMMUNICATION_H

#include <Arduino.h>
#include "Config.h"
#include "LoRaComm.h"

class NodeCommunication {
private:
  LoRaComm* loraComm;
  bool initialized;

public:
  NodeCommunication();
  bool init(LoRaComm* lora);
  bool isInitialized();

  // Main to Node - Send commands
  bool sendCommand(int nodeId, const String &command);
  bool sendCommandWithResponse(int nodeId, const String &command, String &response);

  // Node to Main - Process incoming messages
  void processIncoming();

  // Node status
  String getNodeStatus(int nodeId);
};

extern NodeCommunication nodeComm;

#endif
