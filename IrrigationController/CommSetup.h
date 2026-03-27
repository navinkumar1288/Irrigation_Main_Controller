#ifndef COMM_SETUP_H
#define COMM_SETUP_H

#include <Arduino.h>
#include "Config.h"
#include "BLEComm.h"
#include "LoRaComm.h"
#include "MQTTComm.h"
#include "ModemSMS.h"
#include "WiFiComm.h"
#include "HTTPComm.h"
#include "PPPoSManager.h"
#include "IrrigationNetworkManager.h"
#include "NodeCommunication.h"
#include "UserCommunication.h"

// Forward declarations
extern BLEComm bleComm;
extern LoRaComm loraComm;
extern MQTTComm mqtt;
extern ModemSMS sms;
extern WiFiComm wifiComm;
extern HTTPComm httpComm;
extern PPPoSManager pppos;
extern IrrigationNetworkManager networkMgr;
extern NodeCommunication nodeComm;
extern UserCommunication userComm;

// Communication module setup status
struct CommSetupStatus {
  bool bleOk;
  bool loraOk;
  bool wifiOk;
  bool modemOk;
  bool smsOk;
  bool mqttOk;
  bool httpOk;
  bool ppposOk;
  bool nodeCommOk;
  bool userCommOk;
  
  int totalModules;
  int successfulModules;
  
  CommSetupStatus() : bleOk(false), loraOk(false), wifiOk(false), modemOk(false),
                      smsOk(false), mqttOk(false), httpOk(false), ppposOk(false),
                      nodeCommOk(false), userCommOk(false), totalModules(0), 
                      successfulModules(0) {}
  
  String getStatusString() {
    return String(successfulModules) + "/" + String(totalModules) + " modules ready";
  }
};

/**
 * CommSetup Class - Centralized Communication Module Setup
 * Initializes all 10 communication modules
 */
class CommSetup {
private:
  CommSetupStatus status;
  int stepCounter;
  static CommSetup* instance;

  bool initBLE();
  bool initLoRa();
  bool initWiFi();
  bool initModem();
  bool initSMS();
  bool initMQTT();
  bool initHTTP();
  bool initPPPoS();
  bool initNodeCommunication();
  bool initUserCommunication();

  static void handleBLECommand(int node, String command);

  void printStepHeader(const String &moduleName);
  void printStepSuccess(const String &moduleName);
  void printStepFailure(const String &moduleName, const String &reason = "");
  void printSummary();

public:
  CommSetup();

  CommSetupStatus initializeAll();
  CommSetupStatus getStatus() const;
  void printStatus();
  String getStatusString() const;
  bool isFullyInitialized() const;
  int getSuccessfulCount() const { return status.successfulModules; }
  int getTotalModules() const { return status.totalModules; }
  bool reinitModule(const String &moduleName);
  String getDetailedReport() const;
};

#endif