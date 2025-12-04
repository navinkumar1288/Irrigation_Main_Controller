# PlatformIO Setup Guide - Irrigation Main Controller

This guide will help you migrate from Arduino IDE to PlatformIO to enable MQTT v5 support.

## Why PlatformIO?

- ✅ **MQTT v5 Support**: Proper sdkconfig support for enabling MQTT_PROTOCOL_5
- ✅ **Faster Compilation**: Incremental builds are much faster
- ✅ **Better Dependency Management**: Libraries are project-specific
- ✅ **Advanced Debugging**: Built-in debugger support
- ✅ **Multiple Environments**: Easy to manage different board configurations

## Installation Steps

### Step 1: Install Visual Studio Code

1. Download and install **Visual Studio Code** from: https://code.visualstudio.com/
2. Launch VS Code

### Step 2: Install PlatformIO Extension

1. Open VS Code
2. Click the **Extensions** icon in the left sidebar (or press `Ctrl+Shift+X`)
3. Search for **"PlatformIO IDE"**
4. Click **Install** on the PlatformIO IDE extension
5. Wait for installation to complete (may take a few minutes)
6. Restart VS Code if prompted

### Step 3: Open the Project

1. In VS Code, go to **File → Open Folder**
2. Navigate to: `C:\Users\navin\Documents\GitHub\Irrigation_Main_Controller`
3. Click **Select Folder**
4. PlatformIO will automatically detect the `platformio.ini` file

### Step 4: Install Dependencies

1. PlatformIO will automatically start downloading:
   - ESP32 platform
   - Arduino framework
   - Required libraries
2. This may take 5-10 minutes on first run
3. You'll see progress in the bottom status bar

### Step 5: Build the Project

1. Open the **PlatformIO** icon in the left sidebar (alien head icon)
2. Under **PROJECT TASKS**, expand **heltec_wifi_lora_32_V3**
3. Click **Build** (or press `Ctrl+Alt+B`)
4. Wait for compilation to complete
5. Check for **SUCCESS** message in the terminal

### Step 6: Upload to ESP32

1. Connect your Heltec ESP32 to the computer via USB
2. In **PROJECT TASKS**, click **Upload** (or press `Ctrl+Alt+U`)
3. PlatformIO will auto-detect the COM port
4. Wait for upload to complete

### Step 7: Monitor Serial Output

1. In **PROJECT TASKS**, click **Monitor** (or press `Ctrl+Alt+S`)
2. Serial monitor will open at 115200 baud
3. You should see the startup messages with MQTT v5 support enabled

## Verification

After uploading, you should see in the serial monitor:

```
[MQTT] Initializing MQTT v5 client...
[MQTT] Broker URI: mqtts://sdbc1da0.ala.asia-southeast1.emqxsl.com:8883
[MQTT] → Configuring TLS/SSL...
[MQTT] ✓ TLS/SSL configured
[MQTT] ✓ MQTT v5 client initialized
[MQTT] Broker: sdbc1da0.ala.asia-southeast1.emqxsl.com:8883
[MQTT] Protocol: MQTT v5 over TLS/SSL
[MQTT] Note: MQTT v5 enabled via sdkconfig.defaults
[MQTT] Starting MQTT client...
[MQTT] ✓ MQTT client started (will connect asynchronously)
```

**No more "MQTT_PROTOCOL_5 feature" error!** ✅

## Project Structure

```
Irrigation_Main_Controller/
├── platformio.ini           # PlatformIO configuration
├── sdkconfig.defaults       # ESP-IDF custom config (MQTT v5)
├── IrrigationController/    # Source code (Arduino sketch folder)
│   ├── IrrigationController.ino
│   ├── Config.h
│   ├── MQTTComm.cpp
│   ├── MQTTComm.h
│   └── ... (all other .cpp/.h files)
└── .git/
```

## Key Configuration (platformio.ini)

### MQTT v5 Enabled via Build Flags:
```ini
build_flags =
    -DCONFIG_MQTT_PROTOCOL_5=1
    -DCONFIG_MQTT_PROTOCOL_311=1
    -DCONFIG_MQTT_TRANSPORT_SSL=1
    -DCONFIG_MQTT_BUFFER_SIZE=2048
```

### Board Configuration:
```ini
[env:heltec_wifi_lora_32_V3]
platform = espressif32
board = heltec_wifi_lora_32_V3
framework = arduino
```

## Useful PlatformIO Commands

### Terminal Commands (from project root):
```bash
# Build project
pio run

# Upload to board
pio run --target upload

# Clean build
pio run --target clean

# Monitor serial output
pio device monitor

# Upload and monitor in one command
pio run --target upload --target monitor
```

### VS Code Tasks (via PlatformIO sidebar):
- **Build**: Compile the project
- **Upload**: Flash to ESP32
- **Clean**: Remove build artifacts
- **Monitor**: Open serial monitor
- **Upload and Monitor**: Flash + Monitor

## Troubleshooting

### Issue: "COM port not found"
**Solution**:
1. Check USB cable connection
2. Install CH340/CP2102 drivers if needed
3. Manually specify port in `platformio.ini`:
   ```ini
   upload_port = COM3  ; Replace COM3 with your port
   monitor_port = COM3
   ```

### Issue: "Library not found"
**Solution**:
1. Delete `.pio` folder in project root
2. Run: `pio run` to reinstall dependencies

### Issue: Build errors after migration
**Solution**:
1. Clean build: `pio run --target clean`
2. Rebuild: `pio run`

## Advantages Over Arduino IDE

| Feature | Arduino IDE | PlatformIO |
|---------|-------------|------------|
| MQTT v5 Support | ❌ Not supported | ✅ Fully supported |
| Build Speed | Slow (60s) | Fast (10-15s after first build) |
| Dependency Management | Manual | Automatic |
| Code Intelligence | Basic | Full IntelliSense |
| Debugging | Limited | Built-in debugger |
| Multi-board | Manual switching | Multiple environments |

## Keeping Arduino IDE (Optional)

You can keep both:
- **PlatformIO**: For development and uploading (MQTT v5 works)
- **Arduino IDE**: For quick testing (MQTT v5 won't work)

Just don't modify the same files in both at the same time!

## Getting Help

- PlatformIO Docs: https://docs.platformio.org/
- ESP32 Platform: https://docs.platformio.org/en/latest/platforms/espressif32.html
- Community: https://community.platformio.org/

## Next Steps

1. ✅ Install VS Code + PlatformIO
2. ✅ Open project folder
3. ✅ Build project
4. ✅ Upload to ESP32
5. ✅ Verify MQTT v5 connection works
6. 🎉 Enjoy your MQTT v5 enabled irrigation controller!
