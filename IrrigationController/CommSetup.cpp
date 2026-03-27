 #include "CommSetup.h"

CommSetup* CommSetup::instance = nullptr;

CommSetup::CommSetup() : stepCounter(0) {
  status.totalModules = 0;
  status.successfulModules = 0;
  instance = this;
}

// FIXED: Removed static handleBLECommand - this should be in IrrigationController.ino
// The callback registration now happens in setup() after all modules are initialized

void CommSetup::printStepHeader(const String &moduleName) {
  stepCounter++;
  Serial.printf("[%d/10] %s\n", stepCounter, moduleName.c_str());
  Serial.println("[CommSetup] → Initializing...");
}

void CommSetup::printStepSuccess(const String &moduleName) {
  Serial.printf("[CommSetup] ✓ %s initialized successfully\n", moduleName.c_str());
}

void CommSetup::printStepFailure(const String &moduleName, const String &reason) {
  String msg = "[CommSetup] ❌ " + moduleName + " initialization failed";
  if (reason.length() > 0) {
    msg += " - " + reason;
  }
  Serial.println(msg);
}

bool CommSetup::initBLE() {
  printStepHeader("Bluetooth Low Energy (BLE)");
  status.totalModules++;

  #if ENABLE_BLE
  try {
    if (!bleComm.init()) {
      printStepFailure("BLE", "init() returned false");
      return false;
    }

    // FIXED: Don't register callback here - it's done in setup()
    Serial.println("[CommSetup]   ✓ BLE initialized (callback will be registered in setup)");
    bleComm.printStatus();

    printStepSuccess("BLE");
    status.bleOk = true;
    status.successfulModules++;
    return true;
  } catch (...) {
    printStepFailure("BLE", "Exception during initialization");
    return false;
  }
  #else
  Serial.println("[CommSetup] ⚠ BLE disabled in Config.h");
  return true;
  #endif
}

bool CommSetup::initLoRa() {
  printStepHeader("LoRa Radio Communication");
  status.totalModules++;

  #if ENABLE_LORA
  try {
    Serial.printf("[CommSetup]   Frequency: %d MHz\n", (int)(LORA_FREQUENCY / 1E6));
    Serial.printf("[CommSetup]   Spreading Factor: %d\n", LORA_SPREADING_FACTOR);

    if (!loraComm.init()) {
      printStepFailure("LoRa", "init() returned false");
      return false;
    }

    Serial.println("[CommSetup]   ✓ LoRa parameters configured");
    printStepSuccess("LoRa");
    status.loraOk = true;
    status.successfulModules++;
    return true;
  } catch (...) {
    printStepFailure("LoRa", "Exception during initialization");
    return false;
  }
  #else
  Serial.println("[CommSetup] ⚠ LoRa disabled in Config.h");
  return true;
  #endif
}

bool CommSetup::initWiFi() {
  printStepHeader("WiFi Connectivity");
  status.totalModules++;

  #if ENABLE_WIFI
  try {
    Serial.printf("[CommSetup]   SSID: %s\n", WIFI_SSID);

    if (wifiComm.init(WIFI_SSID, WIFI_PASS)) {
      Serial.println("[CommSetup]   ✓ WiFi connected immediately");
      Serial.printf("[CommSetup]   IP Address: %s\n", wifiComm.getIPAddress().c_str());
    } else {
      Serial.println("[CommSetup]   ⚠ WiFi connection will retry in background");
    }

    printStepSuccess("WiFi");
    status.wifiOk = true;
    status.successfulModules++;
    return true;
  } catch (...) {
    printStepFailure("WiFi", "Exception during initialization");
    return false;
  }
  #else
  Serial.println("[CommSetup] ⚠ WiFi disabled in Config.h");
  return true;
  #endif
}

bool CommSetup::initModem() {
  printStepHeader("Modem (4G/LTE - EC200U)");
  status.totalModules++;

  #if ENABLE_MODEM
  try {
    Serial.println("[CommSetup]   Initializing modem UART...");
    Serial.printf("[CommSetup]   RX Pin: %d, TX Pin: %d\n", MODEM_RX, MODEM_TX);

    Serial.println("[CommSetup]   ✓ Modem UART configured");

    printStepSuccess("Modem");
    status.modemOk = true;
    status.successfulModules++;
    return true;
  } catch (...) {
    printStepFailure("Modem", "Exception during initialization");
    return false;
  }
  #else
  Serial.println("[CommSetup] ⚠ Modem disabled in Config.h");
  return true;
  #endif
}

bool CommSetup::initSMS() {
  printStepHeader("SMS (Short Message Service)");
  status.totalModules++;

  #if ENABLE_SMS
  try {
    Serial.println("[CommSetup]   Configuring SMS module...");

    if (!sms.configure()) {
      printStepFailure("SMS", "configure() returned false");
      return false;
    }

    Serial.println("[CommSetup]   ✓ SMS text mode configured");
    printStepSuccess("SMS");
    status.smsOk = true;
    status.successfulModules++;
    return true;
  } catch (...) {
    printStepFailure("SMS", "Exception during initialization");
    return false;
  }
  #else
  Serial.println("[CommSetup] ⚠ SMS disabled");
  return true;
  #endif
}

bool CommSetup::initMQTT() {
  printStepHeader("MQTT (Message Queuing Telemetry Transport)");
  status.totalModules++;

  #if ENABLE_MQTT
  try {
    Serial.printf("[CommSetup]   Broker: %s\n", MQTT_BROKER);
    Serial.println("[CommSetup]   Protocol: MQTT v3.1.1");

    if (!mqtt.init()) {
      printStepFailure("MQTT", "init() returned false");
      return false;
    }

    Serial.println("[CommSetup]   ✓ MQTT client initialized");

    if (!mqtt.configure()) {
      printStepFailure("MQTT", "configure() returned false");
      return false;
    }

    Serial.println("[CommSetup]   ✓ MQTT connection started");

    printStepSuccess("MQTT");
    status.mqttOk = true;
    status.successfulModules++;
    return true;
  } catch (...) {
    printStepFailure("MQTT", "Exception during initialization");
    return false;
  }
  #else
  Serial.println("[CommSetup] ⚠ MQTT disabled in Config.h");
  return true;
  #endif
}

bool CommSetup::initHTTP() {
  printStepHeader("HTTP REST API");
  status.totalModules++;

  #if ENABLE_HTTP
  try {
    Serial.printf("[CommSetup]   Port: %d\n", HTTP_SERVER_PORT);

    if (!httpComm.init(HTTP_SERVER_PORT)) {
      printStepFailure("HTTP", "init() returned false");
      return false;
    }

    Serial.println("[CommSetup]   ✓ HTTP server started");
    printStepSuccess("HTTP");
    status.httpOk = true;
    status.successfulModules++;
    return true;
  } catch (...) {
    printStepFailure("HTTP", "Exception during initialization");
    return false;
  }
  #else
  Serial.println("[CommSetup] ⚠ HTTP disabled in Config.h");
  return true;
  #endif
}

bool CommSetup::initPPPoS() {
  printStepHeader("PPPoS (PPP over Serial) - Cellular Data");
  status.totalModules++;

  #if ENABLE_PPPOS
  try {
    Serial.printf("[CommSetup]   APN: %s\n", MODEM_APN);
    Serial.printf("[CommSetup]   Timeout: %d ms\n", PPPOS_CONNECT_TIMEOUT_MS);

    if (!pppos.init(&SerialAT, MODEM_APN)) {
      printStepFailure("PPPoS", "init() returned false");
      return false;
    }

    Serial.println("[CommSetup]   ✓ PPPoS manager initialized");

    if (!networkMgr.connect(PPPOS_CONNECT_TIMEOUT_MS, WIFI_CONNECT_TIMEOUT_MS)) {
      Serial.println("[CommSetup]   ⚠ Network connection will retry in background");
    } else {
      Serial.printf("[CommSetup]   ✓ Network connected\n");
      Serial.printf("[CommSetup]   IP Address: %s\n", networkMgr.getLocalIP().c_str());
    }

    printStepSuccess("PPPoS");
    status.ppposOk = true;
    status.successfulModules++;
    return true;
  } catch (...) {
    printStepFailure("PPPoS", "Exception during initialization");
    return false;
  }
  #else
  Serial.println("[CommSetup] ⚠ PPPoS disabled in Config.h");
  return true;
  #endif
}

bool CommSetup::initNodeCommunication() {
  printStepHeader("Node Communication (LoRa)");
  status.totalModules++;

  try {
    if (!nodeComm.init(&loraComm)) {
      printStepFailure("Node Communication", "init() returned false");
      return false;
    }

    Serial.println("[CommSetup]   ✓ Node communication module ready");

    printStepSuccess("Node Communication");
    status.nodeCommOk = true;
    status.successfulModules++;
    return true;
  } catch (...) {
    printStepFailure("Node Communication", "Exception during initialization");
    return false;
  }
}

bool CommSetup::initUserCommunication() {
  printStepHeader("User Communication Handler");
  status.totalModules++;

  try {
    Serial.println("[CommSetup]   ✓ User communication module ready");

    printStepSuccess("User Communication");
    status.userCommOk = true;
    status.successfulModules++;
    return true;
  } catch (...) {
    printStepFailure("User Communication", "Exception during initialization");
    return false;
  }
}

CommSetupStatus CommSetup::initializeAll() {
  Serial.println("\n");
  Serial.println("==========================================");
  Serial.println("  COMMUNICATION MODULES INITIALIZATION");
  Serial.println("==========================================\n");

  initBLE();
  initLoRa();
  initWiFi();
  initModem();
  initSMS();
  initMQTT();
  initHTTP();
  initPPPoS();
  initNodeCommunication();
  initUserCommunication();

  printSummary();

  return status;
}

CommSetupStatus CommSetup::getStatus() const {
  return status;
}

bool CommSetup::isFullyInitialized() const {
  return (status.successfulModules == status.totalModules) && (status.totalModules > 0);
}

void CommSetup::printSummary() {
  Serial.println("\n");
  Serial.println("==========================================");
  Serial.println("  INITIALIZATION SUMMARY");
  Serial.println("==========================================");
  Serial.printf("Total Modules: %d\n", status.totalModules);
  Serial.printf("Successful: %d\n", status.successfulModules);
  Serial.printf("Failed: %d\n", status.totalModules - status.successfulModules);

  Serial.println("\nModule Status:");
  Serial.printf("  BLE:                  %s\n", status.bleOk ? "✓ OK" : "✗ FAILED");
  Serial.printf("  LoRa:                 %s\n", status.loraOk ? "✓ OK" : "✗ FAILED");
  Serial.printf("  WiFi:                 %s\n", status.wifiOk ? "✓ OK" : "⚠ CONNECTING");
  Serial.printf("  Modem:                %s\n", status.modemOk ? "✓ OK" : "✗ FAILED");
  Serial.printf("  SMS:                  %s\n", status.smsOk ? "✓ OK" : "⚠ DISABLED");
  Serial.printf("  MQTT:                 %s\n", status.mqttOk ? "✓ OK" : "��� CONNECTING");
  Serial.printf("  HTTP:                 %s\n", status.httpOk ? "✓ OK" : "✗ FAILED");
  Serial.printf("  PPPoS:                %s\n", status.ppposOk ? "✓ OK" : "⚠ CONNECTING");
  Serial.printf("  Node Communication:   %s\n", status.nodeCommOk ? "✓ OK" : "✗ FAILED");
  Serial.printf("  User Communication:   %s\n", status.userCommOk ? "✓ OK" : "✗ FAILED");

  Serial.println("\n==========================================");
  if (isFullyInitialized()) {
    Serial.println("✓ ALL MODULES INITIALIZED SUCCESSFULLY!");
  } else {
    Serial.printf("⚠ %d modules not fully initialized\n", 
      status.totalModules - status.successfulModules);
  }
  Serial.println("==========================================\n");
}

void CommSetup::printStatus() {
  printSummary();
}

String CommSetup::getStatusString() const {
  if (isFullyInitialized()) {
    return "All communication modules ready";
  } else {
    String msg = String(status.successfulModules) + "/" + String(status.totalModules) + " modules ready";
    return msg;
  }
}

String CommSetup::getDetailedReport() const {
  String report = "=== COMMUNICATION SETUP REPORT ===\n";
  report += "Total Modules: " + String(status.totalModules) + "\n";
  report += "Successful: " + String(status.successfulModules) + "\n\n";
  report += "Individual Status:\n";
  report += "BLE: " + String(status.bleOk ? "OK" : "FAILED") + "\n";
  report += "LoRa: " + String(status.loraOk ? "OK" : "FAILED") + "\n";
  report += "WiFi: " + String(status.wifiOk ? "OK" : "FAILED") + "\n";
  report += "Modem: " + String(status.modemOk ? "OK" : "FAILED") + "\n";
  report += "SMS: " + String(status.smsOk ? "OK" : "FAILED") + "\n";
  report += "MQTT: " + String(status.mqttOk ? "OK" : "FAILED") + "\n";
  report += "HTTP: " + String(status.httpOk ? "OK" : "FAILED") + "\n";
  report += "PPPoS: " + String(status.ppposOk ? "OK" : "FAILED") + "\n";
  report += "Node Comm: " + String(status.nodeCommOk ? "OK" : "FAILED") + "\n";
  report += "User Comm: " + String(status.userCommOk ? "OK" : "FAILED") + "\n";
  return report;
}

bool CommSetup::reinitModule(const String &moduleName) {
  Serial.printf("[CommSetup] Reinitializing module: %s\n", moduleName.c_str());

  String module = moduleName;
  module.toUpperCase();

  if (module == "BLE") return initBLE();
  if (module == "LORA") return initLoRa();
  if (module == "WIFI") return initWiFi();
  if (module == "MODEM") return initModem();
  if (module == "SMS") return initSMS();
  if (module == "MQTT") return initMQTT();
  if (module == "HTTP") return initHTTP();
  if (module == "PPPOS") return initPPPoS();

  Serial.printf("[CommSetup] Unknown module: %s\n", moduleName.c_str());
  return false;
}