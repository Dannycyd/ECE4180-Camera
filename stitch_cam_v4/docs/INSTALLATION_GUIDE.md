# Stitch Camera v2 - Complete Setup Guide

## 📋 Table of Contents
1. [Hardware Requirements](#hardware-requirements)
2. [Software Requirements](#software-requirements)
3. [Library Installation](#library-installation)
4. [Project Setup](#project-setup)
5. [Configuration](#configuration)
6. [Upload Process](#upload-process)
7. [Troubleshooting](#troubleshooting)
8. [Testing](#testing)

---

## 🔧 Hardware Requirements

### Required Components
- **ESP32-S3 N16R8** development board
- **OV2640 camera module** with ArduCAM interface
- **ST7789 LCD display** (240x320)
- **MicroSD card** (formatted as FAT32)
- **2x Push buttons** (capture + mode)
- **RGB LED** (common cathode)
- **3x 220Ω resistors** (for RGB LED)
- **Jumper wires**
- **Breadboard** (optional)

### Pin Connections

```
Camera (HSPI):
  SCK  → GPIO 10
  MISO → GPIO 11
  MOSI → GPIO 12
  CS   → GPIO 13

Camera I2C:
  SDA  → GPIO 9
  SCL  → GPIO 8

SD Card:
  CS   → GPIO 14
  (shares SPI with camera)

LCD (FSPI):
  SCK  → GPIO 5
  MOSI → GPIO 4
  CS   → GPIO 17
  DC   → GPIO 18
  RST  → GPIO 15
  BL   → GPIO 16 (backlight)

Buttons:
  CAPTURE → GPIO 1  (active LOW, internal pullup)
  MODE    → GPIO 45 (active LOW, internal pullup)

RGB LED:
  RED   → GPIO 2  (PWM channel 0)
  GREEN → GPIO 42 (PWM channel 1)
  BLUE  → GPIO 41 (PWM channel 2)
  GND   → GND (through 220Ω resistors)
```

---

## 💻 Software Requirements

### Arduino IDE Setup
1. **Arduino IDE 2.x** or later
2. **ESP32 Board Support**:
   - Add to Boards Manager URLs:
     ```
     https://espressif.github.io/arduino-esp32/package_esp32_index.json
     ```
   - Install **esp32 by Espressif Systems** (version 2.0.14 or later)

### Board Configuration
In Arduino IDE, select:
```
Board: "ESP32S3 Dev Module"
USB CDC On Boot: "Disabled"  ← CRITICAL!
CPU Frequency: "240MHz (WiFi)"
Flash Mode: "QIO 80MHz"
Flash Size: "16MB (128Mb)"
Partition Scheme: "Huge APP (3MB No OTA/1MB SPIFFS)"
PSRAM: "OPI PSRAM"
Upload Mode: "UART0 / Hardware CDC"
Upload Speed: "115200"  ← Start with this, increase if stable
```

---

## 📚 Library Installation

### Method 1: Arduino Library Manager

Install these libraries via **Sketch → Include Library → Manage Libraries**:

1. **ArduCAM** by Lee
   - Search: "ArduCAM"
   - Install latest version

2. **TJpg_Decoder** by Bodmer
   - Search: "TJpg_Decoder"
   - Install latest version

3. **ESP32** core libraries (already included):
   - WiFi
   - WebServer
   - SD
   - SPI

### Method 2: Manual Installation

If library manager fails:

```bash
cd ~/Documents/Arduino/libraries/

# ArduCAM
git clone https://github.com/ArduCAM/Arduino.git ArduCAM

# TJpg_Decoder
git clone https://github.com/Bodmer/TJpg_Decoder.git
```

Restart Arduino IDE after manual installation.

---

## 📁 Project Setup

### 1. Create Project Folder

```bash
Documents/Arduino/StitchCam_v2/
├── main.ino
├── config.h
├── hardware_manager.h
├── camera_manager.h
├── lcd_manager.h
├── storage_manager.h
├── wifi_manager.h
├── web_server.h
├── ui_manager.h
├── task_manager.h
├── memorysaver.h
└── hardware/
    ├── DEV_Config.h
    ├── DEV_Config.cpp
    ├── LCD_Driver.h
    ├── LCD_Driver.cpp
    ├── GUI_Paint.h
    ├── GUI_Paint.cpp
    ├── Debug.h
    ├── index_html_gz.h
    └── gallery_html_gz.h
```

### 2. Copy All Files

Copy all `.h` and `.cpp` files from the project to your Arduino folder.

**IMPORTANT**: Rename the folder to match the main file:
```bash
mv StitchCam_v2 main
# Arduino will rename it to "main" when you open it
```

Or simply rename `main.ino` to `StitchCam_v2.ino` and put it in a folder called `StitchCam_v2`.

---

## ⚙️ Configuration

### 1. WiFi Settings

Edit `config.h`:

```cpp
// WiFi Configuration
#define WIFI_SSID     "YourNetworkName"
#define WIFI_PASSWORD "YourPassword"
```

### 2. Optional Settings

```cpp
// Debug output (comment out to disable)
#define DEBUG

// Camera settings
namespace CameraConfig {
  constexpr uint16_t FRAME_WIDTH = 320;
  constexpr uint16_t FRAME_HEIGHT = 240;
  constexpr uint32_t MAX_JPEG_SIZE = 32768;  // 32KB
}

// UI settings
namespace UIConfig {
  constexpr uint8_t COUNTDOWN_SECONDS = 3;
  constexpr uint32_t DEBOUNCE_MS = 200;
  constexpr uint32_t STATUS_DISPLAY_MS = 2000;
}

// Web server
namespace WebConfig {
  constexpr uint16_t HTTP_PORT = 80;
}
```

### 3. Pin Customization (if needed)

Edit the `Pins` namespace in `config.h`:

```cpp
namespace Pins {
  // Camera SPI
  constexpr uint8_t CAM_SCK  = 10;
  constexpr uint8_t CAM_MISO = 11;
  constexpr uint8_t CAM_MOSI = 12;
  constexpr uint8_t CAM_CS   = 13;
  
  // etc...
}
```

---

## 📤 Upload Process

### Manual Flash Sequence (if auto-upload fails)

1. **Hold BOOT button**
2. **Press RESET button**
3. **Release RESET button**
4. **Release BOOT button**
5. **Click Upload in Arduino IDE**
6. Wait for "Connecting..." message
7. After upload completes, **press RESET**

### Verify Upload

Open Serial Monitor (115200 baud):

```
╔════════════════════════════════════╗
║    Stitch Camera v2 Starting...   ║
║         Ohana means family! 🌺     ║
╚════════════════════════════════════╝

═══════════════════════════════════
  HARDWARE INITIALIZATION
═══════════════════════════════════
→ Initializing GPIO pins...
✓ GPIO initialized
→ Initializing SPI...
✓ SPI initialized (HSPI & FSPI)
✓ Hardware initialized

═══════════════════════════════════
  PERIPHERAL INITIALIZATION
═══════════════════════════════════
→ Initializing LCD...
✓ LCD initialized
→ Initializing Camera...
  ✓ OV2640 detected (VID=0x26 PID=0x42)
✓ Camera initialized
→ Initializing SD Card...
  ✓ SD Card Type: SDHC
  ✓ SD Card Size: 15923MB
  ✓ Found 0 existing photos
✓ SD Card initialized
→ Initializing UI...
✓ UI initialized

═══════════════════════════════════
  NETWORK INITIALIZATION
═══════════════════════════════════
→ Connecting to WiFi...
  SSID: YourNetwork
....
✓ WiFi connected!
  IP Address: 192.168.1.100
  Signal: -45 dBm
✓ Web server started on port 80

═══════════════════════════════════
  TASK INITIALIZATION
═══════════════════════════════════
→ Creating FreeRTOS Tasks...
  ✓ Camera task created (Core 0)
  ✓ UI task created (Core 0)
✓ All tasks created

╔════════════════════════════════════╗
║       System Ready! 🌺💙          ║
╚════════════════════════════════════╝

═══════════════════════════════════
  SYSTEM INFORMATION
═══════════════════════════════════
Camera:  Ready ✓
LCD:     Ready ✓
SD Card: Ready ✓
WiFi:    Connected ✓
IP:      192.168.1.100
Web UI:  http://192.168.1.100
Photos:  0 saved
```

---

## 🔍 Troubleshooting

### Issue: "Connecting..." never succeeds

**Solution**:
1. Lower upload speed to 115200
2. Use manual flash sequence
3. Check USB cable (use data cable, not charge-only)
4. Try different USB port

### Issue: ArduCAM compilation errors

**Error**: `macro "swap" requires 3 arguments`

**Solution**: WiFi libraries are included FIRST in main.ino:
```cpp
#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include <SPI.h>
```

This prevents ArduCAM's problematic `swap` macro from conflicting with C++ STL.

### Issue: Camera not detected

**Symptoms**:
```
❌ Camera detection failed! VID=0xFF PID=0xFF
```

**Solutions**:
1. Check SPI wiring (SCK, MISO, MOSI, CS)
2. Check I2C wiring (SDA, SCL)
3. Verify camera module has 3.3V power
4. Try swapping SDA/SCL if reversed
5. Measure voltages with multimeter

### Issue: SD Card not detected

**Symptoms**:
```
❌ SD Card initialization failed
❌ No SD card attached
```

**Solutions**:
1. Format SD card as FAT32
2. Try different SD card (Class 10 recommended)
3. Check CS pin connection (GPIO 14)
4. SD card shares SPI with camera - check MISO/MOSI/SCK
5. Reduce SPI speed if unstable

### Issue: WiFi won't connect

**Symptoms**:
```
❌ WiFi connection timeout
```

**Solutions**:
1. Verify SSID and password in config.h
2. Check 2.4GHz WiFi (ESP32 doesn't support 5GHz)
3. Move closer to router
4. Check router firewall settings
5. Try different WiFi network

### Issue: LCD shows garbage or nothing

**Solutions**:
1. Check FSPI wiring (SCK=5, MOSI=4, CS=17, DC=18, RST=15)
2. Verify backlight pin (GPIO 16)
3. Check 3.3V power to LCD
4. Measure backlight voltage (should be 3.3V when on)
5. Try adjusting LCD_SetBacklight() value

### Issue: Buttons not responding

**Solutions**:
1. Check button wiring (active LOW configuration)
2. Buttons should connect GPIO to GND when pressed
3. Internal pullups are enabled in code
4. Test with multimeter: should read 3.3V when open, 0V when pressed
5. Adjust debounce time in config.h if needed

### Issue: "Brownout detector was triggered"

**Solutions**:
1. Use better power supply (5V 2A minimum)
2. Add capacitors near ESP32 (100µF + 100nF)
3. Reduce camera resolution/quality temporarily
4. Disable WiFi during camera capture
5. Check for loose connections

---

## ✅ Testing

### 1. Basic Hardware Test

```cpp
// Test each component individually:
// - Upload ESP32S3_LED_Test.ino first
// - Verify LED blinks
// - Test buttons with Serial Monitor
```

### 2. Camera Test

Open Serial Monitor and look for:
```
✓ OV2640 detected (VID=0x26 PID=0x42)
✓ Captured JPEG: 15234 bytes
```

### 3. LCD Test

You should see:
- Boot screen with "STITCH CAM"
- Live camera preview
- Mode indicator in top-right corner

### 4. SD Card Test

```
✓ SD Card Type: SDHC
✓ SD Card Size: 15923MB
✓ Created directory: /photos
```

### 5. WiFi Test

Navigate to the IP address shown in Serial Monitor:
```
Web UI: http://192.168.1.100
```

You should see the Stitch-themed camera interface.

### 6. Full System Test

1. **Instant Capture**:
   - Press CAPTURE button → LED turns RED → Photo saved
   - Check SD card for `/photos/IMG_0001.jpg`

2. **Countdown Mode**:
   - Press MODE button → LED turns BLUE briefly
   - Press CAPTURE → 3-2-1 countdown → Photo captured

3. **Web Interface**:
   - Open `http://[ESP32_IP]` in browser
   - Click "Take Photo" button
   - Toggle between Instant/Countdown mode
   - View gallery at `http://[ESP32_IP]/gallery`

---

## 🎯 Success Criteria

✅ Serial output shows all components initialized  
✅ LCD displays live camera preview  
✅ RGB LED shows green (idle) status  
✅ Buttons trigger captures  
✅ Photos saved to SD card  
✅ WiFi connected and web interface accessible  
✅ Gallery shows captured photos  

---

## 📞 Support

If you encounter issues:

1. **Check Serial Monitor** - most errors are logged
2. **Verify wiring** - double-check all connections
3. **Test components individually** - isolate the problem
4. **Check power supply** - ensure adequate current
5. **Review this guide** - solutions for common issues above

**Debug Checklist**:
- [ ] USB cable is data-capable
- [ ] Board set to "USB CDC On Boot: Disabled"
- [ ] Upload speed not too high (start with 115200)
- [ ] WiFi included before ArduCAM headers
- [ ] All libraries installed correctly
- [ ] SD card formatted as FAT32
- [ ] Power supply is 5V 2A minimum
- [ ] All wiring matches pin configuration

---

## 🚀 Next Steps

Once everything works:

1. **Customize the theme** - edit `index_html_gz.h`
2. **Add features** - face detection, motion detection
3. **Optimize performance** - adjust buffer sizes, task priorities
4. **Expand storage** - implement photo deletion, pagination
5. **Enhance UI** - add settings page, live streaming

Enjoy your Stitch Camera! 🌺📸💙
