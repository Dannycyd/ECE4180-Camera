# 🚀 Stitch Camera v2 - Quick Reference

## ⚡ Fast Start

```bash
1. Wire hardware (see pinout below)
2. Install libraries: ArduCAM, TJpg_Decoder
3. Edit config.h: Set WiFi SSID/Password
4. Upload to ESP32-S3 (USB CDC: Disabled!)
5. Open http://[IP_ADDRESS]
```

---

## 📌 Pinout Cheat Sheet

```
┌─────────────────────────────────────┐
│         ESP32-S3 Connections        │
├─────────────────────────────────────┤
│ Camera SPI (HSPI)                   │
│  SCK=10  MISO=11  MOSI=12  CS=13   │
│ Camera I2C                          │
│  SDA=9   SCL=8                      │
│ SD Card                             │
│  CS=14  (shares SPI with camera)    │
├─────────────────────────────────────┤
│ LCD (FSPI)                          │
│  SCK=5   MOSI=4   CS=17             │
│  DC=18   RST=15   BL=16             │
├─────────────────────────────────────┤
│ Buttons (active LOW)                │
│  CAPTURE=1   MODE=45                │
│ RGB LED (PWM)                       │
│  R=2   G=42   B=41                  │
└─────────────────────────────────────┘
```

---

## 🔧 Board Settings

```
Board:          ESP32S3 Dev Module
USB CDC On Boot: Disabled ⚠️
CPU Frequency:   240MHz (WiFi)
Flash Size:      16MB (128Mb)
Partition:       Huge APP (3MB No OTA)
PSRAM:          OPI PSRAM
Upload Speed:    115200 (start), 921600 (stable)
```

---

## 📡 API Endpoints

```
GET  /                    → Main page
GET  /gallery             → Photo gallery
GET  /capture             → Take photo
GET  /toggle              → Change mode
GET  /countdown_start     → Start countdown
GET  /status              → JSON status
GET  /stream              → Current frame
GET  /photos              → JSON list
GET  /photo?name=X        → Download
GET  /delete?name=X       → Delete
```

---

## 🎮 Physical Controls

```
[CAPTURE Button]
  • Instant Mode: Press → Capture immediately
  • Countdown Mode: Press → 3-2-1 countdown

[MODE Button]
  • Toggle: Instant ↔ Countdown
  • LED flashes BLUE when changed
```

---

## 💡 LED Status Codes

```
🟢 GREEN   → Idle / Ready
🔴 RED     → Capturing photo
🟡 YELLOW  → Saving to SD
🔵 BLUE    → Mode changed
🔵 CYAN    → Streaming
🔴 RED     → Error
```

---

## 🐛 Common Issues & Fixes

### Upload Fails
```
1. Hold BOOT → Press RESET → Release RESET → Release BOOT
2. Set "USB CDC On Boot" to Disabled
3. Lower upload speed to 115200
```

### Camera Not Found
```
1. Check VID=0x26 PID=0x42 in Serial Monitor
2. Verify SPI: SCK=10, MISO=11, MOSI=12, CS=13
3. Verify I2C: SDA=9, SCL=8
4. Swap SDA/SCL if needed
```

### ArduCAM Won't Compile
```
1. Include WiFi.h BEFORE ArduCAM headers
2. Check main.ino has correct order
```

### SD Card Not Working
```
1. Format as FAT32
2. Use Class 10 card
3. Check CS pin = GPIO 14
```

### WiFi Won't Connect
```
1. Use 2.4GHz network (not 5GHz)
2. Check SSID/password in config.h
3. Check Serial Monitor for errors
```

---

## 📁 File Organization

```
main.ino              ← Arduino entry point
config.h              ← WiFi, pins, settings
memorysaver.h         ← ArduCAM config

Managers:
├─ hardware_manager.h ← GPIO, SPI, I2C
├─ camera_manager.h   ← OV2640 control
├─ lcd_manager.h      ← Display
├─ storage_manager.h  ← SD card
├─ wifi_manager.h     ← WiFi
├─ web_server.h       ← HTTP server
├─ ui_manager.h       ← Buttons
└─ task_manager.h     ← FreeRTOS

hardware/             ← LCD drivers + HTML
```

---

## 🔑 Key Configuration

### WiFi
```cpp
// config.h
#define WIFI_SSID "YourNetwork"
#define WIFI_PASSWORD "YourPassword"
```

### Camera Resolution
```cpp
// config.h - CameraConfig
FRAME_WIDTH = 320    // 320, 640, 800
FRAME_HEIGHT = 240   // 240, 480, 600
```

### Countdown Timer
```cpp
// config.h - UIConfig
COUNTDOWN_SECONDS = 3  // 1-10
```

### Debug Output
```cpp
// config.h
#define DEBUG  // Comment out to disable
```

---

## 📊 Memory Usage

```
Flash:  ~700 KB / 16 MB
SRAM:   ~120 KB / 512 KB
PSRAM:  ~200 KB / 8 MB (buffers)
```

---

## ✅ Verification Checklist

```
□ Serial shows "System Ready! 🌺💙"
□ LCD displays boot screen
□ Camera VID=0x26 PID=0x42 detected
□ SD card initialized (SDHC/SD)
□ WiFi connected (IP displayed)
□ Web server started on port 80
□ Tasks created on both cores
□ LED shows GREEN (idle)
□ Buttons respond to press
□ Photos save to /photos/
□ Web interface accessible
```

---

## 🎯 Serial Monitor Output (Success)

```
╔════════════════════════════════════╗
║    Stitch Camera v2 Starting...   ║
║         Ohana means family! 🌺     ║
╚════════════════════════════════════╝

→ Initializing GPIO pins...
✓ GPIO initialized
→ Initializing Camera...
  ✓ OV2640 detected (VID=0x26 PID=0x42)
✓ Camera initialized
→ Initializing SD Card...
  ✓ SD Card Type: SDHC
  ✓ SD Card Size: 15923MB
✓ SD Card initialized
→ Connecting to WiFi...
✓ WiFi connected!
  IP Address: 192.168.1.100
✓ Web server started on port 80
✓ All tasks created

╔════════════════════════════════════╗
║       System Ready! 🌺💙          ║
╚════════════════════════════════════╝
```

---

## 📞 Quick Help

**Problem**: Upload hangs at "Connecting..."
**Fix**: Manual flash sequence (hold BOOT, etc.)

**Problem**: Compilation error with "swap"
**Fix**: WiFi.h must be included first

**Problem**: No camera image
**Fix**: Check SPI wiring and I2C connections

**Problem**: SD card fails
**Fix**: Format as FAT32, check CS=14

**Problem**: WiFi timeout
**Fix**: Use 2.4GHz network, verify credentials

---

## 🌐 Web Access

```
Main Interface:  http://[IP]/
Photo Gallery:   http://[IP]/gallery
Current Frame:   http://[IP]/stream
System Status:   http://[IP]/status
```

---

## 📚 Full Documentation

- **INSTALLATION_GUIDE.md** → Complete setup
- **LIBRARIES.md** → Dependencies
- **PROJECT_README.md** → Architecture
- **README.md** → Original docs

---

## 💾 Photo Storage

```
SD Card Format: FAT32
Directory:      /photos/
Naming:         IMG_0001.jpg, IMG_0002.jpg, ...
Max Size:       Limited by SD card capacity
```

---

## 🎨 Customization

```cpp
// WiFi
WIFI_SSID / WIFI_PASSWORD

// Pins
Pins::BUTTON_CAPTURE = 1
Pins::LED_RED = 2

// Camera
CameraConfig::FRAME_WIDTH = 320
CameraConfig::MAX_JPEG_SIZE = 32768

// UI
UIConfig::COUNTDOWN_SECONDS = 3
UIConfig::DEBOUNCE_MS = 200

// Tasks
TaskConfig::CAMERA_PRIORITY = 2
TaskConfig::CAMERA_CORE = 0
```

---

<div align="center">

**🌺 Stitch Camera v2.0 Quick Reference 🌺**

*Keep this handy while building!*

</div>
