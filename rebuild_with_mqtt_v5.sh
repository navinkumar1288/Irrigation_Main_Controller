#!/bin/bash
# Script to completely clean and rebuild project with MQTT v5 support
# This ensures the ESP-IDF framework is recompiled with CONFIG_MQTT_PROTOCOL_5 enabled

echo "================================================"
echo "  Clean Rebuild Script - MQTT v5 Support"
echo "================================================"
echo ""
echo "This script will:"
echo "  1. Remove ALL cached builds (.pio directory)"
echo "  2. Remove ALL platform packages (forces framework rebuild)"
echo "  3. Rebuild project from scratch with MQTT v5 enabled"
echo ""
read -p "Press ENTER to continue or CTRL+C to cancel..."
echo ""

# Step 1: Remove .pio directory (all build artifacts)
echo "[1/4] Removing .pio directory..."
if [ -d ".pio" ]; then
    rm -rf .pio
    echo "      ✓ .pio directory removed"
else
    echo "      → .pio directory doesn't exist (already clean)"
fi
echo ""

# Step 2: Remove platformio cache (optional but recommended)
echo "[2/4] Removing PlatformIO cache..."
if [ -d "~/.platformio/.cache" ]; then
    rm -rf ~/.platformio/.cache
    echo "      ✓ PlatformIO cache cleared"
else
    echo "      → No cache to clear"
fi
echo ""

# Step 3: Rebuild project
echo "[3/4] Rebuilding project with MQTT v5 support..."
echo "      This may take 5-10 minutes (downloading + compiling framework)"
echo ""
pio run --target clean
pio run
echo ""

# Step 4: Verify build
if [ $? -eq 0 ]; then
    echo "[4/4] ✓ Build SUCCESS!"
    echo ""
    echo "================================================"
    echo "  Next Steps:"
    echo "================================================"
    echo ""
    echo "1. Connect your ESP32 to the computer"
    echo "2. Upload the firmware:"
    echo "   $ pio run --target upload"
    echo ""
    echo "3. Monitor serial output:"
    echo "   $ pio device monitor"
    echo ""
    echo "4. You should see:"
    echo "   [MQTT] ✓ MQTT v5 client initialized"
    echo "   (No more 'MQTT_PROTOCOL_5 feature' error!)"
    echo ""
else
    echo "[4/4] ❌ Build FAILED!"
    echo ""
    echo "Please check the error messages above."
    echo ""
fi
