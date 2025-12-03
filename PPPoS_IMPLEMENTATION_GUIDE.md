# PPPoS Implementation Guide for EC200U + Heltec ESP32 V3

## Overview

This guide explains how to implement PPP over Serial (PPPoS) to connect your Heltec ESP32 to the internet through the EC200U cellular modem via UART, instead of using the modem's WiFi hotspot.

## Why Use PPPoS?

### Benefits
- **Full TCP/IP Stack**: ESP32 gets a real IP address and full internet connectivity
- **Standard Libraries**: Use any Arduino networking library (PubSubClient, HTTPClient, etc.)
- **Better Reliability**: More stable than WiFi hotspot mode
- **Lower Power**: No WiFi radio active on the modem
- **Easier Debugging**: Standard networking tools work normally

### When Modem WiFi Hotspot Fails
The modem's WiFi hotspot feature (`AT+QWAP` commands) can be unreliable. PPPoS provides a more robust alternative that works at the serial protocol level.

## Architecture

### Before (WiFi Hotspot Mode)
```
┌─────────────────────────────────────┐
│  ESP32 Application                  │
├─────────────────────────────────────┤
│  ESP32 WiFi (STA mode)              │  ← Connects to modem's AP
├─────────────────────────────────────┤
│         ))) WiFi ((((                │
├─────────────────────────────────────┤
│  EC200U Modem (WiFi AP mode)        │
│  Cellular Data Connection           │
└─────────────────────────────────────┘
```

### After (PPPoS Mode)
```
┌─────────────────────────────────────┐
│  ESP32 Application (MQTT, HTTP)     │
├─────────────────────────────────────┤
│  ESP32 TCP/IP Stack                 │
├─────────────────────────────────────┤
│  ESP32 PPP Client (esp_netif)       │
├─────────────────────────────────────┤
│  UART (GPIO45/46, 115200 baud)      │
├─────────────────────────────────────┤
│  EC200U Modem (PPP Server mode)     │
│  Cellular Data Connection           │
└─────────────────────────────────────┘
```

## Hardware Requirements

- **Heltec ESP32 V3** (or compatible)
- **Quectel EC200U** cellular modem
- **UART Connection**:
  - ESP32 GPIO45 (RX) → EC200U TX
  - ESP32 GPIO46 (TX) → EC200U RX
  - Common GND
- **SIM Card** with active data plan
- **APN** configuration for your carrier

## Software Requirements

- **Arduino IDE** or **PlatformIO**
- **ESP32 Arduino Core** 2.0.0 or later (includes ESP-IDF with PPP support)
- No additional libraries required (uses built-in esp_netif)

## Implementation Steps

### 1. PPPoS Configuration (Config.h)

Add to your `Config.h`:
```cpp
// ========== PPPoS Settings ==========
#define ENABLE_PPPOS 1                // Enable PPPoS mode
#define PPPOS_APN "airtelgprs.com"    // Your carrier's APN
#define PPPOS_CONNECT_TIMEOUT_MS 30000 // 30 second timeout
```

### 2. PPPoS Manager Integration

The project includes:
- `PPPoSManager.h` - PPPoS manager class header
- `PPPoSManager.cpp` - PPPoS manager implementation

### 3. Basic Usage Example

```cpp
#include "PPPoSManager.h"
#include "Config.h"

PPPoSManager pppos;
HardwareSerial ModemSerial(1);

void setup() {
  Serial.begin(115200);

  // Initialize modem serial
  ModemSerial.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(1000);

  // Initialize PPPoS
  if (!pppos.init(&ModemSerial, PPPOS_APN)) {
    Serial.println("PPPoS init failed!");
    return;
  }

  // Connect to cellular network via PPP
  if (!pppos.connect(PPPOS_CONNECT_TIMEOUT_MS)) {
    Serial.println("PPPoS connection failed!");
    return;
  }

  Serial.println("PPPoS connected!");
  Serial.println("IP: " + pppos.getLocalIP());

  // Now you can use standard networking libraries
  // Example: MQTT, HTTP, etc.
}

void loop() {
  // CRITICAL: Feed serial data to PPP stack
  pppos.loop();

  // Your application code here
  delay(10);
}
```

### 4. Integration with MQTT

Once PPPoS is connected, you can use standard MQTT libraries:

```cpp
#include <PubSubClient.h>
#include <WiFiClient.h>  // Works with PPP too!

WiFiClient espClient;
PubSubClient mqtt(espClient);

void setup() {
  // ... PPPoS setup code ...

  if (pppos.isConnected()) {
    // Configure MQTT
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);

    // Connect to MQTT broker
    if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
      Serial.println("MQTT connected!");
      mqtt.subscribe(MQTT_TOPIC_COMMANDS);
    }
  }
}

void loop() {
  pppos.loop();  // CRITICAL!
  mqtt.loop();

  // Your code here
}
```

## PPP Connection Sequence

### Step-by-Step Process

1. **Initialize Modem Serial**
   ```cpp
   ModemSerial.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
   ```

2. **Test Modem Communication**
   ```
   AT
   OK
   ```

3. **Configure PDP Context**
   ```
   AT+CGDCONT=1,"IP","airtelgprs.com"
   OK
   ```

4. **Dial PPP Connection**
   ```
   ATD*99#
   CONNECT
   ```

   After CONNECT, modem is in PPP mode (binary protocol).

5. **Initialize ESP32 PPP Client**
   - Create esp_netif for PPP
   - Register event handlers
   - Start PPP client

6. **PPP Negotiation**
   - LCP (Link Control Protocol)
   - Authentication (if required)
   - IPCP (IP Control Protocol)
   - IP address assignment

7. **Connected!**
   - Receive IP_EVENT_PPP_GOT_IP event
   - IP address is assigned
   - Internet is available

## Modem AT Commands Reference

### Essential Commands

| Command | Purpose | Example |
|---------|---------|---------|
| `AT` | Test communication | `AT` → `OK` |
| `AT+CREG?` | Check network registration | `AT+CREG?` → `+CREG: 0,1` |
| `AT+CSQ` | Check signal quality | `AT+CSQ` → `+CSQ: 25,0` |
| `AT+CGDCONT=1,"IP","<APN>"` | Set PDP context | `AT+CGDCONT=1,"IP","airtelgprs.com"` |
| `ATD*99#` | Dial PPP connection | `ATD*99#` → `CONNECT` |
| `+++` (with 1s delay) | Exit PPP mode | `+++` → `OK` (back to AT mode) |

### Checking PPP Status

```
AT+CGDCONT?    // Check PDP context
AT+CGACT?      // Check PDP activation status (NOT needed for PPP)
AT+CGPADDR=1   // Get IP address (only works in AT command mode)
```

**Note**: After `ATD*99#` and `CONNECT`, modem is in PPP binary mode. You cannot send AT commands. To return to AT mode, send `+++` with 1 second guard time before and after.

## Troubleshooting

### Problem: No CONNECT after ATD*99#

**Possible Causes**:
1. SIM card not inserted or not activated
2. No network registration
3. Incorrect APN
4. No data plan on SIM

**Debug Steps**:
```
AT+CPIN?       // Check SIM status (should be "READY")
AT+CREG?       // Check registration (+CREG: 0,1 or 0,5 is good)
AT+CSQ         // Check signal (+CSQ: 20-30 is good, 99 is bad)
AT+COPS?       // Check operator
```

### Problem: CONNECT received but no IP address

**Possible Causes**:
1. PPP client not starting properly
2. Serial data not being fed to PPP stack
3. PPP negotiation failing

**Debug Steps**:
1. Check `esp_netif_init()` returns ESP_OK
2. Verify `pppos.loop()` is being called regularly
3. Check for PPP event logs in serial output
4. Verify APN is correct

### Problem: Connection drops after a while

**Possible Causes**:
1. Not calling `pppos.loop()` frequently enough
2. Network issue (weak signal)
3. Modem going to sleep

**Solutions**:
1. Call `pppos.loop()` at least every 100ms
2. Improve antenna/signal
3. Configure modem power saving: `AT+QSCLK=0` (disable sleep)

### Problem: Can't send AT commands after PPP connection

This is **normal behavior**! After `ATD*99#` and `CONNECT`, the modem is in PPP binary mode.

To return to AT command mode:
1. Stop the PPP client: `pppos.disconnect()`
2. Send escape sequence: `+++` (with 1 second guard time before and after)
3. Wait for `OK`
4. Now you can send AT commands again

## Performance Tips

### 1. Call loop() Frequently
```cpp
void loop() {
  pppos.loop();  // Feed data to PPP stack - CRITICAL!

  // Keep other tasks short (<10ms each)
  mqtt.loop();
  // ... other tasks ...

  delay(10);  // Small delay to avoid watchdog
}
```

### 2. Avoid Blocking Operations
```cpp
// BAD - blocks PPP stack
delay(5000);

// GOOD - non-blocking delay
unsigned long start = millis();
while (millis() - start < 5000) {
  pppos.loop();
  delay(10);
}
```

### 3. Buffer Serial Data
The modem serial should run at 115200 baud minimum for good PPP performance. Higher baud rates (230400, 460800) may work but aren't necessary.

## SMS While Using PPPoS

**Important**: PPP uses the modem's serial port for binary data. You **cannot** send AT commands for SMS while PPP is active.

### Options:

1. **Disconnect PPP to send SMS**:
   ```cpp
   pppos.disconnect();
   // Send AT commands for SMS
   // Then reconnect PPP
   pppos.connect();
   ```

2. **Use a second UART** (if modem supports it):
   Some modems support multiple UART interfaces. Check EC200U documentation.

3. **Use SMS-over-IP** (if carrier supports):
   Send SMS via internet API instead of AT commands.

## Integration with Existing Code

### Current: AT Command MQTT

```cpp
// OLD: Using AT commands
modem.sendCommand("AT+QMTOPEN=0,\"broker\",1883");
modem.sendCommand("AT+QMTCONN=0,\"client_id\"");
modem.sendCommand("AT+QMTPUB=0,0,0,0,\"topic\",\"message\"");
```

### New: PPPoS with Standard MQTT Library

```cpp
// NEW: Using PPPoS with PubSubClient
#include <PubSubClient.h>
#include "PPPoSManager.h"

WiFiClient espClient;
PubSubClient mqtt(espClient);
PPPoSManager pppos;

void setup() {
  // Initialize PPPoS
  pppos.init(&ModemSerial, PPPOS_APN);
  pppos.connect();

  // Use standard MQTT library
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS);
  mqtt.publish("topic", "message");
}

void loop() {
  pppos.loop();  // CRITICAL!
  mqtt.loop();
}
```

## Files in This Implementation

```
IrrigationController/
├── PPPoSManager.h              # PPPoS manager header
├── PPPoSManager.cpp            # PPPoS manager implementation
├── Config.h                    # Updated with PPPoS settings
└── examples/
    └── PPPoS_Example.ino       # Simple example (modem initialization + dial)
```

## Next Steps

1. **Test the Basic Example**: Start with `examples/PPPoS_Example.ino` to verify modem can dial PPP
2. **Add PPPoS to Main Code**: Integrate `PPPoSManager` into `IrrigationController.ino`
3. **Switch to Standard MQTT**: Replace `ModemMQTT` with `PubSubClient`
4. **Update Config**: Enable `ENABLE_PPPOS` in `Config.h`
5. **Test Connectivity**: Verify MQTT and other services work over PPP

## References

- [ESP-IDF PPP Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_netif.html#ppp-support)
- [Quectel EC200U AT Commands Manual](https://www.quectel.com/product/lte-ec200u/)
- [PPP Protocol Overview (RFC 1661)](https://tools.ietf.org/html/rfc1661)

## Support

For issues or questions:
1. Check the troubleshooting section above
2. Review serial monitor output for error messages
3. Verify modem is registered on network (AT+CREG?)
4. Test basic AT commands work (AT, AT+CSQ, etc.)

---

**Note**: This implementation uses the ESP32's built-in ESP-IDF PPP support. No external libraries are required beyond the standard ESP32 Arduino core.
