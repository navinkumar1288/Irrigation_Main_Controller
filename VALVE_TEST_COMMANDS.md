# Valve Test Commands - Irrigation Main Controller

## Overview
This document describes how to test valve open/close operations on remote nodes using various interfaces (Serial, SMS, BLE).

## Command Interfaces

### 1. Serial Commands (USB/UART - 115200 baud)
Connect to the controller via USB serial port and send commands directly.

#### Basic Commands
```bash
# Test node connectivity
1 PING

# Get node status
1 STATUS

# Close valve on node
1 CLOSE

# Open valve (no duration - manual close required)
1 OPEN
```

#### Enhanced Commands with Duration
```bash
# Open valve for 60 seconds (60000ms)
1 OPEN 60000

# Open valve for 2 minutes (120000ms)
1 OPEN 120000

# Open valve for 30 seconds
2 OPEN 30000
```

#### Multi-Valve Commands (for nodes with multiple valves)
```bash
# Open valve 2 on node 1 for 30 seconds
1 OPEN 30000 2

# Open valve 1 on node 1 for 1 minute
1 OPEN 60000 1

# Open valve 3 on node 2 for 45 seconds
2 OPEN 45000 3

# Open valve 4 on node 1 for 2 minutes
1 OPEN 120000 4
```

#### Command Format
```
<node_id> <command> [duration_ms] [valve_id]

Parameters:
  node_id      : Remote node ID (1-255)
  command      : PING, STATUS, OPEN, CLOSE
  duration_ms  : Duration in milliseconds (optional, for OPEN only)
  valve_id     : Valve number on node (optional, 0-255)
```

#### Serial Command Examples
```
# Test connectivity to all nodes
1 PING
2 PING
3 PING

# Open valve on node 1 for testing (5 seconds)
1 OPEN 5000

# Open specific valves on node with 4 valves
1 OPEN 10000 1    # Valve 1 for 10 seconds
1 OPEN 15000 2    # Valve 2 for 15 seconds
1 OPEN 20000 3    # Valve 3 for 20 seconds
1 OPEN 25000 4    # Valve 4 for 25 seconds

# Emergency close
1 CLOSE
```

---

### 2. SMS Commands
Send SMS to the configured alert phone number (+919944272647).

#### Basic SMS Commands
```
STATUS           - Get system status
SCHEDULES        - List enabled schedules
STOP             - Stop all running schedules
SMS ON           - Enable SMS alerts
SMS OFF          - Disable SMS alerts
CHECK            - Manually scan for messages
HELP             - Show command list
```

#### Node Control via SMS
```
# Test node connectivity
1 PING

# Get node status
1 STATUS

# Open valve for 60 seconds
1 OPEN 60000

# Open valve 2 for 30 seconds
1 OPEN 30000 2

# Close valve
1 CLOSE

# Alternative format with NODE prefix
NODE 1 PING
NODE 1 OPEN 60000
NODE 1 OPEN 30000 2
```

#### SMS Command Format
```
<node_id> <command> [duration_ms] [valve_id]

OR

NODE <node_id> <command> [duration_ms] [valve_id]
```

#### SMS Response
The system will reply with:
- Success: `Node 1 OK: OPEN 60000ms V2`
- Timeout: `Node 1 TIMEOUT`
- Error: `LoRa not available`

---

### 3. BLE Commands
Connect via Bluetooth Low Energy using a BLE app (nRF Connect, Serial Bluetooth Terminal, etc.).

**Service UUID:** `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
**RX Characteristic:** `6e400002-b5a3-f393-e0a9-e50e24dcca9e` (Write to this)
**TX Characteristic:** `6e400003-b5a3-f393-e0a9-e50e24dcca9e` (Read notifications)

#### BLE Command Format
Same as serial commands:
```
1 PING
1 STATUS
1 OPEN 60000
1 OPEN 30000 2
1 CLOSE
```

---

## LoRa Protocol Details

### Command Structure
```
CMD|MID=<message_id>|<command>|N=<node>,S=<schedule>,I=<index>,T=<duration>
```

### Examples
```
CMD|MID=1001|PING|N=1,S=,I=0
CMD|MID=1002|OPEN|N=1,S=,I=0,T=60000
CMD|MID=1003|OPEN|N=1,S=TEST_V2,I=2,T=30000
CMD|MID=1004|CLOSE|N=1,S=,I=0
```

### ACK Response
```
ACK|MID=<message_id>|<command>|N=<node>,S=<schedule>,I=<index>|OK
```

### LoRa Parameters
- **Frequency:** 865 MHz (India ISM band)
- **Spreading Factor:** 10
- **Bandwidth:** 125 kHz
- **Max Retries:** 3 attempts
- **ACK Timeout:** 5000ms (5 seconds)
- **Retry Delay:** 300ms between attempts

---

## Testing Scenarios

### 1. Basic Connectivity Test
```bash
# Via Serial Monitor
1 PING
2 PING
3 PING
```

**Expected Output:**
```
[Serial] Node: 1, Command: PING
[Serial] Sending via LoRa...
[LoRa] TX: CMD|MID=1001|PING|N=1,S=,I=0
[LoRa] RX: ACK|MID=1001|PONG|N=1,S=,I=0|OK (RSSI=-45, SNR=8)
[Serial] ✓✓✓ SUCCESS ✓✓✓
```

---

### 2. Single Valve Test (Short Duration)
```bash
# Open valve for 10 seconds to verify operation
1 OPEN 10000
```

**Expected Behavior:**
1. Command sent via LoRa
2. Node acknowledges
3. Valve opens immediately
4. Valve closes after 10 seconds
5. Node sends AUTO_CLOSE message

---

### 3. Multi-Valve Sequence Test
```bash
# Test all 4 valves on node 1 sequentially
1 OPEN 5000 1    # Valve 1: 5 seconds
# Wait 10 seconds
1 OPEN 5000 2    # Valve 2: 5 seconds
# Wait 10 seconds
1 OPEN 5000 3    # Valve 3: 5 seconds
# Wait 10 seconds
1 OPEN 5000 4    # Valve 4: 5 seconds
```

---

### 4. Emergency Stop Test
```bash
# Open valve for long duration
1 OPEN 300000

# Immediately close it
1 CLOSE
```

**Expected:** Valve closes immediately regardless of remaining time.

---

### 5. Schedule-Based Test
```json
{
  "id": "ValveTest",
  "rec": "O",
  "enabled": true,
  "start_time": "2024-01-15T14:30:00",
  "seq": [
    {"node_id": 1, "valve_id": 1, "duration_ms": 30000},
    {"node_id": 1, "valve_id": 2, "duration_ms": 30000},
    {"node_id": 1, "valve_id": 3, "duration_ms": 30000},
    {"node_id": 1, "valve_id": 4, "duration_ms": 30000}
  ],
  "pump_on_before_ms": 3000,
  "pump_off_after_ms": 3000
}
```

Or compact format:
```
SCH|ID=ValveTest,REC=O,T=2024-01-15T14:30:00,SEQ=1.1:30000;1.2:30000;1.3:30000;1.4:30000,PB=3000,PA=3000
```

---

## Troubleshooting

### Command Not Working
1. **Check LoRa Status:**
   ```
   [Serial] ✓ LoRa initialized
   ```

2. **Verify Node ID:**
   - Must be 1-255
   - Node must be powered and in range

3. **Check RSSI/SNR:**
   - Good: RSSI > -80, SNR > 5
   - Poor: RSSI < -100, SNR < 0
   - Move closer or check antenna

### Timeout Errors
```
[LoRa] ✗ ACK timeout
[LoRa] Retry...
[LoRa] ✗✗✗ FAILED after 3 attempts
```

**Solutions:**
- Verify node is powered on
- Check battery level (low battery = poor RF)
- Reduce distance between controller and node
- Check for RF interference
- Verify node firmware is running

### Valve Doesn't Open
1. **Check Node Telemetry:**
   ```
   STAT|N=1,BATT=85,BV=3.8,SOLV=5.2,V1=0,V2=0,V3=0,V4=0,M1=45,M2=52,M3=48,M4=41
   ```
   - V1-V4 should show 1 when open, 0 when closed

2. **Verify Power:**
   - Battery voltage (BV) should be > 3.3V
   - Solar voltage (SOLV) if using solar panel

3. **Check Wiring:**
   - Valve solenoid connected correctly
   - Relay output working

---

## Serial Monitor Setup

### Arduino IDE
1. Tools → Serial Monitor
2. Set baud rate: **115200**
3. Line ending: **Newline** or **Both NL & CR**
4. Type commands and press Enter

### PlatformIO
```bash
pio device monitor -b 115200
```

### Screen (Linux/Mac)
```bash
screen /dev/ttyUSB0 115200
```

### PuTTY (Windows)
- Connection Type: Serial
- Speed: 115200
- Data bits: 8
- Stop bits: 1
- Parity: None

---

## Command Reference Quick Sheet

| Command | Format | Example | Description |
|---------|--------|---------|-------------|
| PING | `<node> PING` | `1 PING` | Test connectivity |
| STATUS | `<node> STATUS` | `1 STATUS` | Get node status |
| CLOSE | `<node> CLOSE` | `1 CLOSE` | Close valve |
| OPEN | `<node> OPEN <ms>` | `1 OPEN 60000` | Open for duration |
| OPEN (valve) | `<node> OPEN <ms> <v>` | `1 OPEN 30000 2` | Open specific valve |

**Duration Units:**
- 1 second = 1000ms
- 1 minute = 60000ms
- 5 minutes = 300000ms
- 10 minutes = 600000ms

---

## Safety Notes

⚠️ **IMPORTANT SAFETY GUIDELINES:**

1. **Always use reasonable durations** - Start with short durations (5-10 seconds) for initial testing
2. **Monitor water flow** - Ensure valves are connected to controlled water sources
3. **Emergency stop available** - Use `<node> CLOSE` to immediately stop any valve
4. **Battery monitoring** - Low battery affects valve operation and LoRa range
5. **Test in dry conditions first** - Verify electrical operation before connecting to water
6. **Check for leaks** - Inspect all connections before automated operation

---

## Event Logging

All commands generate events published via MQTT:

**Success:**
```
EVT|CMD|N=1|C=OPEN|D=60000|V=2|OK
```

**Failure:**
```
ERR|CMD|N=1|C=OPEN|D=60000|V=2|FAIL
```

**Auto-Close:**
```
AUTO_CLOSE|N=1,S=TEST_V2,I=2
```

**Telemetry:**
```
STAT|N=1,BATT=85,BV=3.8,SOLV=5.2,V1=1,V2=0,V3=0,V4=0,M1=45,M2=52,M3=48,M4=41
```

---

## Additional Resources

- **Config.h** - System configuration and LoRa parameters
- **LoRaComm.cpp** - LoRa communication protocol implementation
- **ScheduleManager.cpp** - Schedule execution logic
- **IrrigationController.ino** - Main controller logic and command handlers

---

## Support

For issues or questions:
1. Check serial monitor output for diagnostic messages
2. Verify all connections and power
3. Review RSSI/SNR values for RF quality
4. Check battery levels on remote nodes
5. Review this documentation for command format

**Emergency Stop:** Send SMS `STOP` or serial command `<node> CLOSE`
