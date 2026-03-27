// IrrigationController.ino - Main sketch with modular architecture
// Clean separation: CommSetup, UserCommunication, NodeCommunication

#include "Config.h"
#include "Utils.h"
#include "MessageQueue.h"
#include "StorageManager.h"
#include "TimeManager.h"
#include "DisplayManager.h"
#include "LoRaComm.h"
#include "MQTTComm.h"
#include "ModemSMS.h"
#include "BLEComm.h"
#include "WiFiComm.h"
#include "HTTPComm.h"
#include "ScheduleManager.h"
#include "NodeCommunication.h"
#include "UserCommunication.h"
#include "PPPoSManager.h"
#include "IrrigationNetworkManager.h"
#include "MessageFormats.h"
#include "CommSetup.h"

// ========== Global Variable Definitions ==========
SystemConfig sysConfig;
std::vector<Schedule> schedules;
String currentScheduleId = "";
std::vector<SeqStep> seq;
int currentStepIndex = -1;
unsigned long stepStartMillis = 0;
bool scheduleLoaded = false;
bool scheduleRunning = false;
time_t scheduleStartEpoch = 0;
uint32_t pumpOnBeforeMs = PUMP_ON_LEAD_DEFAULT_MS;
uint32_t pumpOffAfterMs = PUMP_OFF_DELAY_DEFAULT_MS;
uint32_t LAST_CLOSE_DELAY_MS = LAST_CLOSE_DELAY_MS_DEFAULT;
uint32_t DRIFT_THRESHOLD_S = 300;
uint32_t SYNC_CHECK_INTERVAL_MS = 3600000UL;
bool ENABLE_SMS_BROADCAST = true;

// ========== Module Instances ==========
Preferences prefs;
MessageQueue incomingQueue;
StorageManager storage;
TimeManager timeManager;
DisplayManager displayMgr;
LoRaComm loraComm;
MQTTComm mqtt;
ModemSMS sms;
BLEComm bleComm;
WiFiComm wifiComm;
HTTPComm httpComm;
ScheduleManager scheduleMgr;
NodeCommunication nodeComm;
UserCommunication userComm;
PPPoSManager pppos;
IrrigationNetworkManager networkMgr;
CommSetup commSetup;

TwoWire WireRTC = TwoWire(1);
RTC_DS3231 rtc;
bool rtcAvailable = false;
bool loraInitialized = false;
CommSetupStatus commStatus;

// ========== BLE Command Handler Callback ==========
void handleBLECommand(int node, String command) {
  Serial.printf("[MAIN] BLE Command: node=%d, command=%s\n", node, command.c_str());
  userComm.processBLECommand(node, command);
}

// ========== Node Command Handler Callback ==========
bool handleNodeCommand(int nodeId, const String& command) {
  Serial.printf("[MAIN] Node Command: nodeId=%d, command=%s\n", nodeId, command.c_str());
  nodeComm.sendCommand(nodeId, command);
  return true;
}

// ========== Setup ==========
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n");
  Serial.println("==========================================");
  Serial.println("  IRRIGATION CONTROLLER v2.0");
  Serial.println("  Modular Architecture");
  Serial.println("==========================================\n");
  
  // ========== Step 1: Core System Initialization ==========
  Serial.println("[SETUP] Initializing core systems...\n");
  
  Serial.println("[1/6] Storage Manager...");
  if (storage.init()) {
    Serial.println("      ✓ Storage initialized");
  }
  
  Serial.println("[2/6] Preferences...");
  prefs.begin("irrig", false);
  Serial.println("      ✓ Preferences initialized");
  
  Serial.println("[3/6] Configuration...");
  storage.loadSystemConfig(sysConfig);
  storage.loadAllSchedules(schedules);
  Serial.println("      ✓ Configuration loaded");
  
  Serial.println("[4/6] Display Manager...");
  #if ENABLE_DISPLAY
  if (displayMgr.init()) {
    displayMgr.showMessage("Irrigation", "v2.0", "Init...", "");
    Serial.println("      ✓ Display initialized");
  }
  #endif
  
  Serial.println("[5/6] Real-Time Clock...");
  WireRTC.begin(RTC_SDA, RTC_SCL);
  if (rtc.begin(&WireRTC)) {
    rtcAvailable = true;
    Serial.println("      ✓ RTC synchronized");
  } else {
    rtcAvailable = false;
    Serial.println("      ⚠ RTC not available");
  }

  // ========== Step 2: Communication Modules Initialization ==========
  Serial.println("[6/6] Communication Modules...");
  commStatus = commSetup.initializeAll();
  
  if (commStatus.successfulModules < commStatus.totalModules) {
    Serial.printf("⚠ %d/%d modules initialized\n",
      commStatus.successfulModules, commStatus.totalModules);
  }

  // ========== Step 3: Initialize User Communication ==========
  Serial.println("\n[SETUP] Initializing UserCommunication...");
  userComm.init(&sms, &bleComm, &loraComm, &mqtt, &wifiComm, &httpComm, 
                &commSetup, SMS_ALERT_PHONE_1);
  userComm.setNodeCommandCallback(handleNodeCommand);
  Serial.println("      ✓ UserCommunication ready");

  // ========== Step 4: Register BLE Callback ==========
  Serial.println("[SETUP] Registering BLE callback...");
  bleComm.setCommandCallback(handleBLECommand);
  Serial.println("      ✓ BLE callback registered");

  // ========== Step 5: Initialize Application Modules ==========
  Serial.println("[SETUP] Initializing application modules...");
  
  // FIXED: Initialize ScheduleManager with UserComm pointer
  scheduleMgr.init(&userComm);
  Serial.println("      ✓ Schedule Manager ready");
  
  if (nodeComm.init(&loraComm)) {
    Serial.println("      ✓ Node Communication ready");
  }

  // ========== Setup Complete ==========
  Serial.println("\n==========================================");
  Serial.println("✓ SYSTEM READY");
  Serial.println("==========================================\n");
  
  userComm.printBriefStatus(&schedules, scheduleRunning);
}

// ========== Main Loop ==========
void loop() {
  // Background tasks
  #if ENABLE_WIFI
  wifiComm.processBackground();
  #endif

  #if ENABLE_MQTT
  mqtt.processBackground();
  #endif

  #if ENABLE_HTTP
  httpComm.processBackground();
  #endif

  #if ENABLE_PPPOS
  networkMgr.processBackground();
  #endif

  #if ENABLE_LORA
  loraComm.processIncoming();
  nodeComm.processIncoming();
  #endif

  #if ENABLE_BLE
  static unsigned long lastBLECheck = 0;
  if (millis() - lastBLECheck > 30000) {
    lastBLECheck = millis();
    if (bleComm.isConnected()) {
      Serial.println("[MAIN] ✓ BLE connected");
    }
  }
  #endif

  // Message processing
  String msg;
  if (incomingQueue.dequeue(msg)) {
    Serial.println("[MAIN] Queue message: " + msg);
  }

  userComm.processAllChannels(&schedules, &scheduleRunning, &scheduleLoaded, 
                              &ENABLE_SMS_BROADCAST);

  // Health check
  static unsigned long lastHealthCheck = 0;
  if (millis() - lastHealthCheck > 60000) {
    lastHealthCheck = millis();
    if (!userComm.isSystemHealthy()) {
      userComm.sendAlert(userComm.getHealthStatus(), "WARNING");
    }
  }

  // Status report
  static unsigned long lastStatusReport = 0;
  if (millis() - lastStatusReport > 300000) {
    lastStatusReport = millis();
    userComm.printBriefStatus(&schedules, scheduleRunning);
  }

  #if ENABLE_DISPLAY
  displayMgr.update();
  #endif

  vTaskDelay(pdMS_TO_TICKS(10));
}

// ========== Diagnostic Functions ==========

void printFullSystemDiagnostics() {
  Serial.println("\n==========================================");
  Serial.println("  FULL SYSTEM DIAGNOSTIC");
  Serial.println("==========================================\n");
  
  userComm.printSystemStatus(&schedules, scheduleRunning);
  userComm.printCommStatus();
  userComm.printSystemDiagnostics();
  
  Serial.println("==========================================\n");
}

String getSystemStatusJSON() {
  return userComm.getStatusJSON(&schedules, scheduleRunning);
}

void checkSystemHealth() {
  if (userComm.isSystemHealthy()) {
    Serial.println("[MAIN] ✓ System Health: HEALTHY");
  } else {
    Serial.println("[MAIN] ⚠ System Health: " + userComm.getHealthStatus());
  }
}

void startSchedule(String scheduleId) {
  currentScheduleId = scheduleId;
  scheduleRunning = true;
  userComm.onScheduleStarted(scheduleId);
}

void stopAllSchedules() {
  scheduleRunning = false;
  if (currentScheduleId.length() > 0) {
    userComm.onScheduleCompleted(currentScheduleId);
  }
}

void notifyValveAction(int nodeId, String valve, String action) {
  userComm.onValveAction(nodeId, valve, action);
}

void notifySystemError(String errorMessage) {
  userComm.onSystemError(errorMessage);
}

void notifySystemWarning(String warningMessage) {
  userComm.onSystemWarning(warningMessage);
}