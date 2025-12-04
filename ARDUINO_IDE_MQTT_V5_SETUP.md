# Arduino IDE - Enable MQTT v5 Support

This guide shows how to enable MQTT v5 protocol in Arduino IDE for ESP32.

## Problem

EMQX broker requires MQTT v5, but ESP32 Arduino Core has `CONFIG_MQTT_PROTOCOL_5` disabled by default.

Error message:
```
E (23098) mqtt_client: Please first enable MQTT_PROTOCOL_5 feature in menuconfig
[MQTT] ❌ Failed to create MQTT client
```

## Solution: Modify Arduino ESP32 Core Build Configuration

### Method 1: Modify boards.txt (Board-specific)

**Advantage**: Only affects Heltec WiFi LoRa 32 V3 board

**Steps**:

1. **Close Arduino IDE** (important!)

2. **Open boards.txt** in a text editor:
   ```
   C:\Users\navin\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.3\boards.txt
   ```

3. **Find the Heltec WiFi LoRa 32 V3 section**

   Search for `heltec_wifi_lora_32_V3` - you'll see something like:

   ```ini
   heltec_wifi_lora_32_V3.name=Heltec WiFi LoRa 32(V3)
   heltec_wifi_lora_32_V3.build.board=HELTEC_WIFI_LORA_32_V3
   heltec_wifi_lora_32_V3.build.variant=heltec_wifi_lora_32_V3
   heltec_wifi_lora_32_V3.build.core=esp32
   heltec_wifi_lora_32_V3.build.mcu=esp32
   heltec_wifi_lora_32_V3.build.f_cpu=240000000L
   ...more lines...
   ```

4. **Add this line** in the `heltec_wifi_lora_32_V3` section (anywhere before the next board):

   ```ini
   heltec_wifi_lora_32_V3.build.extra_flags=-DCONFIG_MQTT_PROTOCOL_5=1 -DCONFIG_MQTT_PROTOCOL_311=1 -DCONFIG_MQTT_TRANSPORT_SSL=1 -DCONFIG_MQTT_BUFFER_SIZE=2048 -DCONFIG_MQTT_TASK_STACK_SIZE=6144
   ```

5. **Save boards.txt**

6. **Restart Arduino IDE**

7. **Clean and re-upload**:
   - Delete build cache (optional but recommended)
   - Upload sketch to ESP32

---

### Method 2: Create platform.local.txt (All ESP32 boards)

**Advantage**: Works for all ESP32 boards, survives core updates if you back it up

**Steps**:

1. **Close Arduino IDE** (important!)

2. **Navigate to ESP32 core directory**:
   ```
   C:\Users\navin\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.3.3\
   ```

3. **Create a new file** named `platform.local.txt` (if it doesn't exist)

4. **Add these lines** to `platform.local.txt`:

   ```ini
   # Enable MQTT Protocol v5 globally for all ESP32 boards
   compiler.c.extra_flags=-DCONFIG_MQTT_PROTOCOL_5=1 -DCONFIG_MQTT_PROTOCOL_311=1 -DCONFIG_MQTT_TRANSPORT_SSL=1 -DCONFIG_MQTT_BUFFER_SIZE=2048 -DCONFIG_MQTT_TASK_STACK_SIZE=6144
   compiler.cpp.extra_flags=-DCONFIG_MQTT_PROTOCOL_5=1 -DCONFIG_MQTT_PROTOCOL_311=1 -DCONFIG_MQTT_TRANSPORT_SSL=1 -DCONFIG_MQTT_BUFFER_SIZE=2048 -DCONFIG_MQTT_TASK_STACK_SIZE=6144
   ```

5. **Save platform.local.txt**

6. **Restart Arduino IDE**

7. **Upload sketch to ESP32**

---

### Method 3: Arduino IDE Preferences (Quick but needs to be set every time)

**Note**: This method may not work for all ESP32 Arduino Core versions.

**Steps**:

1. Open Arduino IDE

2. Go to **File → Preferences**

3. In the "Additional Boards Manager URLs" field (or custom build flags if available), try adding:
   ```
   -DCONFIG_MQTT_PROTOCOL_5=1
   ```

4. This method is **not recommended** as it's not reliable across all setups

---

## Verification

After modifying the configuration and uploading your sketch, you should see:

```
[MQTT] Initializing MQTT v5 client...
[MQTT] Broker URI: mqtts://sdbc1da0.ala.asia-southeast1.emqxsl.com:8883
[MQTT] → Configuring TLS/SSL...
[MQTT] ✓ TLS/SSL configured
[MQTT] ✓ MQTT v5 client initialized
[MQTT] Protocol: MQTT v5 over TLS/SSL
[MQTT] Starting MQTT client...
[MQTT] → Connecting to broker...
[MQTT] ✓ Connected to broker
```

**No more "MQTT_PROTOCOL_5 feature" error!** ✅

---

## Troubleshooting

### Issue: Still getting "MQTT_PROTOCOL_5 feature" error

**Solutions**:
1. Make sure you **restarted Arduino IDE** after modifying files
2. Try **Method 2** (platform.local.txt) instead of Method 1
3. Delete build cache:
   - Windows: Delete `C:\Users\navin\AppData\Local\Temp\arduino_build_*` folders
   - Or: Sketch → Show Sketch Folder → Delete `build` folder
4. Re-upload sketch

### Issue: Can't find boards.txt or it looks different

**Solution**:
- Your ESP32 core version might be different
- Try **Method 2** (platform.local.txt) instead
- Make sure you're using ESP32 Arduino Core 3.3.3

### Issue: Modifications don't persist after ESP32 core update

**Solution**:
- Back up `platform.local.txt` before updating
- Re-apply after update
- Or consider switching to PlatformIO (permanent solution)

---

## Build Flags Explained

| Flag | Purpose |
|------|---------|
| `CONFIG_MQTT_PROTOCOL_5=1` | Enable MQTT v5 protocol support |
| `CONFIG_MQTT_PROTOCOL_311=1` | Also enable v3.1.1 for compatibility |
| `CONFIG_MQTT_TRANSPORT_SSL=1` | Enable TLS/SSL transport |
| `CONFIG_MQTT_BUFFER_SIZE=2048` | Set buffer size to 2048 bytes |
| `CONFIG_MQTT_TASK_STACK_SIZE=6144` | Set MQTT task stack size |

---

## Recommended Method

**For most users**: Use **Method 2** (platform.local.txt)
- Works globally for all ESP32 boards
- Easy to backup and restore
- Survives Arduino IDE restarts

---

## Alternative: Switch to PlatformIO

If these methods don't work, consider PlatformIO:
- Native MQTT v5 support out of the box
- No manual configuration needed
- See `PLATFORMIO_SETUP.md` in the project root

---

## Support

If you continue to have issues:
1. Check Arduino IDE version (should be 2.x or 1.8.19+)
2. Check ESP32 core version (should be 3.3.3)
3. Try cleaning build cache and re-uploading
4. Consider PlatformIO as an alternative
