# MQTT v5 Error Fix

## Problem

Getting this error when the ESP32 starts:

```
E (10725) mqtt_client: Please first enable MQTT_PROTOCOL_5 feature in menuconfig
[MQTT] ❌ Failed to create MQTT client
```

## Root Cause

The ESP-IDF framework within Arduino-ESP32 was compiled **without** `CONFIG_MQTT_PROTOCOL_5` enabled. Even though we have:
- ✅ `CONFIG_MQTT_PROTOCOL_5=y` in `sdkconfig.defaults`
- ✅ `-DCONFIG_MQTT_PROTOCOL_5=1` in `platformio.ini`

PlatformIO was using a **cached version** of the framework that didn't have MQTT v5 enabled.

## Solution

### Quick Fix (Recommended)

Run the automated rebuild script:

**Windows:**
```cmd
rebuild_with_mqtt_v5.bat
```

**Linux/Mac:**
```bash
chmod +x rebuild_with_mqtt_v5.sh
./rebuild_with_mqtt_v5.sh
```

This will:
1. Delete all build caches (`.pio` directory)
2. Force PlatformIO to rebuild the ESP32 framework with MQTT v5 enabled
3. Rebuild your project from scratch

### Manual Fix

If you prefer to do it manually:

```bash
# 1. Clean all builds
pio run --target clean
rm -rf .pio

# 2. Rebuild from scratch (this will take 5-10 minutes)
pio run

# 3. Upload to ESP32
pio run --target upload

# 4. Monitor serial output
pio device monitor
```

### For Windows (PowerShell/CMD):

```cmd
pio run --target clean
rmdir /s /q .pio
pio run
pio run --target upload
pio device monitor
```

## What Was Fixed

1. **Updated `platformio.ini`** - Fixed sdkconfig path reference:
   ```ini
   board_build.cmake_extra_args =
       -DSDKCONFIG_DEFAULTS="${PROJECT_DIR}/sdkconfig.defaults"
   ```

2. **Added embed_files** - Ensured CA certificate is embedded:
   ```ini
   board_build.embed_files = IrrigationController/emqx_ca_cert.pem
   ```

## Verification

After rebuilding and uploading, you should see:

```
[MQTT] Initializing MQTT v5 client...
[MQTT] Broker URI: mqtts://sdbc1da0.ala.asia-southeast1.emqxsl.com:8883
[MQTT] → Configuring TLS/SSL...
[MQTT] ⚠ TLS certificate validation disabled (testing mode)
[MQTT] ✓ TLS/SSL configured
[MQTT] ✓ MQTT v5 client initialized    ← ✅ SUCCESS!
[MQTT] Broker: sdbc1da0.ala.asia-southeast1.emqxsl.com:8883
[MQTT] Protocol: MQTT v5 over TLS/SSL
[MQTT] Note: MQTT v5 enabled via sdkconfig.defaults
```

**No more "MQTT_PROTOCOL_5 feature" error!** ✅

## Why This Happened

PlatformIO caches compiled frameworks in `.pio/` directory to speed up builds. When you first built the project, the Arduino-ESP32 framework was compiled without MQTT v5 support.

Subsequent builds reused this cached framework, even though the `sdkconfig.defaults` file was present.

The solution is to **force a complete rebuild** that recompiles the framework with the new configuration.

## Time Required

- **First rebuild**: 5-10 minutes (downloading and compiling framework)
- **Subsequent builds**: 15-30 seconds (only your code changes)

## Alternative: Arduino IDE Users

If you're using Arduino IDE instead of PlatformIO, see `ARDUINO_IDE_MQTT_V5_SETUP.md` for instructions on modifying `boards.txt` or `platform.local.txt`.

## Still Having Issues?

1. **Check PlatformIO version**: `pio --version` (should be 6.0+)
2. **Check build output** for any errors during framework compilation
3. **Try removing global cache**:
   - Windows: `%USERPROFILE%\.platformio\.cache`
   - Linux/Mac: `~/.platformio/.cache`
4. **Verify sdkconfig.defaults exists** in project root
5. **Check the git branch**: Should be on `claude/setup-mqtt-client-...`

## Files Modified

- `platformio.ini` - Fixed sdkconfig path, added embed_files
- `rebuild_with_mqtt_v5.sh` - Linux/Mac rebuild script (new)
- `rebuild_with_mqtt_v5.bat` - Windows rebuild script (new)
- `MQTT_V5_FIX.md` - This documentation (new)

## Technical Details

The ESP-IDF MQTT client library checks for `CONFIG_MQTT_PROTOCOL_5` at **compile time** (when building the framework), not at runtime. This means:

- ❌ **Build flags alone** (`-DCONFIG_MQTT_PROTOCOL_5=1`) don't work - they only define preprocessor macros for your code
- ✅ **sdkconfig.defaults** works - it configures ESP-IDF during framework compilation
- ✅ **Complete rebuild** required - to recompile the framework with new config

## Related Files

- `PLATFORMIO_SETUP.md` - General PlatformIO setup guide
- `ARDUINO_IDE_MQTT_V5_SETUP.md` - Arduino IDE setup guide (alternative)
- `sdkconfig.defaults` - ESP-IDF configuration file
- `platformio.ini` - PlatformIO project configuration
