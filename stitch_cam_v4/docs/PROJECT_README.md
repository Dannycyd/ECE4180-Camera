# 🌺 Stitch Camera v2.0 - Professional RTOS Edition

<div align="center">

**A Disney Stitch-themed ESP32-S3 camera system with clean RTOS architecture**

[Features](#features) • [Quick Start](#quick-start) • [Architecture](#architecture) • [Documentation](#documentation) • [API](#api-reference)

![ESP32-S3](https://img.shields.io/badge/ESP32--S3-N16R8-blue)
![ArduCAM](https://img.shields.io/badge/Camera-OV2640-green)
![FreeRTOS](https://img.shields.io/badge/RTOS-Dual--Core-orange)
![WiFi](https://img.shields.io/badge/WiFi-2.4GHz-yellow)

</div>

---

## ✨ Features

### Hardware
- 📷 **OV2640 Camera** (2MP JPEG capture)
- 🖥️ **ST7789 LCD** (240×320 live preview with DMA)
- 💾 **SD Card Storage** (FAT32 filesystem)
- 🔘 **Physical Buttons** (capture + mode toggle)
- 💡 **RGB LED** (status indicator with PWM)
- 📡 **WiFi Web Interface** (Stitch-themed controls)

### Software
- 🎯 **Dual-Core RTOS** (FreeRTOS task architecture)
- ⚡ **DMA Transfers** (fast LCD updates)
- 🔄 **Two Capture Modes** (instant + countdown)
- 🌐 **RESTful API** (JSON endpoints)
- 📁 **Photo Gallery** (web-based browsing)
- 🎨 **Stitch Theme** (tropical UI design)
- 🛡️ **Type-Safe Config** (enum classes, namespaces)

---

## 🚀 Quick Start

### 1. Hardware Setup

```
ESP32-S3 ─┬─ OV2640 Camera (SPI + I2C)
          ├─ ST7789 LCD (Fast SPI)
          ├─ SD Card (shared SPI)
          ├─ 2× Buttons (GPIO)
          └─ RGB LED (PWM)
```

See [INSTALLATION_GUIDE.md](INSTALLATION_GUIDE.md) for detailed wiring.

### 2. Software Installation

```bash
# Install Arduino IDE 2.x
# Add ESP32 board support
# Install libraries: ArduCAM, TJpg_Decoder
```

See [LIBRARIES.md](LIBRARIES.md) for complete library setup.

### 3. Configure & Upload

1. Edit `config.h`:
   ```cpp
   #define WIFI_SSID     "YourNetwork"
   #define WIFI_PASSWORD "YourPassword"
   ```

2. Board Settings:
   - **Board**: ESP32S3 Dev Module
   - **USB CDC On Boot**: Disabled ⚠️
   - **Partition**: Huge APP (3MB)
   - **Upload Speed**: 115200

3. Upload and monitor Serial (115200 baud)

### 4. Access Web Interface

```
http://[ESP32_IP_ADDRESS]/
```

Look for IP in Serial Monitor output.

---

## 🏗️ Architecture

### Dual-Core Task Distribution

```
┌─────────────────────────────────────────┐
│         ESP32-S3 Dual Core              │
├─────────────────────────────────────────┤
│  Core 0 (Hardware)  │  Core 1 (Network) │
├─────────────────────┼───────────────────┤
│  • Camera Task      │  • WiFi Manager   │
│  • LCD Task         │  • Web Server     │
│  • UI Task          │  • Client Handler │
│  • Storage Task     │  • API Endpoints  │
│                     │                   │
│  Priority: 2        │  Priority: 1      │
│  Stack: 8-16 KB     │  Stack: 4-8 KB    │
└─────────────────────┴───────────────────┘
```

### Modular Manager System

```
main.ino
  ├─ config.h              # Centralized configuration
  ├─ hardware_manager.h    # GPIO, SPI, I2C, PWM
  ├─ camera_manager.h      # OV2640 capture & decode
  ├─ lcd_manager.h         # ST7789 display with DMA
  ├─ storage_manager.h     # SD card file operations
  ├─ wifi_manager.h        # WiFi connection
  ├─ web_server.h          # HTTP server + routes
  ├─ ui_manager.h          # Button handling + modes
  ├─ task_manager.h        # FreeRTOS orchestration
  └─ memorysaver.h         # ArduCAM config
```

### Memory Layout

```
Flash (16MB):
  ├─ App Code:      ~700 KB
  ├─ OTA:           0 KB (disabled)
  ├─ SPIFFS:        1 MB
  └─ Free:          ~14 MB

SRAM (512KB):
  ├─ Program:       ~120 KB
  ├─ DMA Buffer:    16 KB
  ├─ Camera FIFO:   32 KB
  ├─ Stack/Heap:    ~50 KB
  └─ Free:          ~290 KB

PSRAM (8MB):
  ├─ Frame Buffer:  153 KB (320×240×2)
  ├─ JPEG Buffer:   32 KB
  └─ Available:     ~7.8 MB
```

---

## 📖 Documentation

| Document | Description |
|----------|-------------|
| [README.md](README.md) | This file - project overview |
| [INSTALLATION_GUIDE.md](INSTALLATION_GUIDE.md) | Complete setup instructions |
| [LIBRARIES.md](LIBRARIES.md) | Library dependencies |
| [config.h](config.h) | Configuration reference |

---

## 🔌 API Reference

### Web Endpoints

| Method | Endpoint | Description | Response |
|--------|----------|-------------|----------|
| GET | `/` | Main control page | HTML |
| GET | `/gallery` | Photo gallery | HTML |
| GET | `/capture` | Trigger instant capture | `OK` |
| GET | `/toggle` | Switch capture mode | `OK` |
| GET | `/countdown_start` | Start countdown | `OK` |
| GET | `/status` | System status | JSON |
| GET | `/stream` | Current frame | JPEG |
| GET | `/photos` | List all photos | JSON |
| GET | `/photo?name=X` | Download photo | JPEG |
| GET | `/delete?name=X` | Delete photo | `OK` |

### Status JSON Format

```json
{
  "mode": "Instant",
  "status": "Idle",
  "photos": 5,
  "sdAvailable": true,
  "cameraAvailable": true
}
```

### Photos List JSON

```json
{
  "photos": [
    "/photos/IMG_0001.jpg",
    "/photos/IMG_0002.jpg",
    "/photos/IMG_0003.jpg"
  ]
}
```

---

## 🎨 Customization

### Change WiFi Settings

```cpp
// config.h
#define WIFI_SSID     "MyNetwork"
#define WIFI_PASSWORD "MyPassword123"
```

### Adjust Camera Resolution

```cpp
// config.h - CameraConfig namespace
constexpr uint16_t FRAME_WIDTH = 640;   // 320, 640, 800
constexpr uint16_t FRAME_HEIGHT = 480;  // 240, 480, 600
```

### Modify Countdown Duration

```cpp
// config.h - UIConfig namespace
constexpr uint8_t COUNTDOWN_SECONDS = 5;  // 1-10 seconds
```

### Change Pin Assignments

```cpp
// config.h - Pins namespace
constexpr uint8_t BUTTON_CAPTURE = 1;   // Your GPIO
constexpr uint8_t BUTTON_MODE = 45;     // Your GPIO
constexpr uint8_t LED_RED = 2;          // Your GPIO
// etc...
```

### Customize Web Theme

Edit compressed HTML files:
1. Decompress `hardware/index_html_gz.h`
2. Modify HTML/CSS/JS
3. Recompress with gzip
4. Convert to C array with `xxd -i`

---

## 🐛 Troubleshooting

### Camera Not Detected

```
❌ Camera detection failed! VID=0xFF PID=0xFF
```

**Solutions**:
- Check SPI wiring (SCK=10, MISO=11, MOSI=12, CS=13)
- Verify I2C wiring (SDA=9, SCL=8)
- Ensure 3.3V power to camera
- Try swapping SDA/SCL

### ArduCAM Compilation Error

```
error: macro "swap" requires 3 arguments
```

**Solution**: WiFi.h MUST be included before ArduCAM:
```cpp
#include <WiFi.h>  // First!
#include <ArduCAM.h>
```

### Upload Fails / Bootloop

**Manual Flash Sequence**:
1. Hold BOOT button
2. Press RESET
3. Release RESET
4. Release BOOT
5. Click Upload
6. Press RESET after upload

**Board Settings**:
- Set "USB CDC On Boot" to **Disabled**
- Lower upload speed to 115200

### SD Card Not Working

**Solutions**:
- Format as FAT32
- Use Class 10 card
- Check CS pin (GPIO 14)
- Reduce SPI clock if unstable

### WiFi Won't Connect

**Solutions**:
- Verify 2.4GHz network (ESP32 doesn't support 5GHz)
- Check SSID/password in config.h
- Move closer to router
- Check Serial Monitor for error details

See [INSTALLATION_GUIDE.md](INSTALLATION_GUIDE.md) for comprehensive troubleshooting.

---

## 📊 Performance

| Metric | Value |
|--------|-------|
| Frame Rate | 10-15 FPS (320×240) |
| JPEG Size | 10-30 KB typical |
| Capture Time | ~100-200 ms |
| LCD Update | ~60 ms (DMA) |
| Boot Time | ~5 seconds |
| WiFi Connect | ~2-5 seconds |
| Max Photos | Limited by SD card |

---

## 🛠️ Development

### Project Structure

```
stitch_cam_v2/
├── Core Files
│   ├── main.ino              # Entry point
│   ├── config.h              # Configuration
│   └── memorysaver.h         # ArduCAM config
│
├── Managers (Business Logic)
│   ├── hardware_manager.h    # GPIO/SPI/I2C init
│   ├── camera_manager.h      # Camera control
│   ├── lcd_manager.h         # Display control
│   ├── storage_manager.h     # SD operations
│   ├── wifi_manager.h        # WiFi connectivity
│   ├── web_server.h          # HTTP server
│   ├── ui_manager.h          # Button handling
│   └── task_manager.h        # RTOS tasks
│
├── Hardware Drivers
│   ├── DEV_Config.h/cpp      # SPI/DMA config
│   ├── LCD_Driver.h/cpp      # ST7789 driver
│   ├── GUI_Paint.h/cpp       # Graphics lib
│   └── Debug.h               # Debug macros
│
├── Web Assets (Compressed)
│   ├── index_html_gz.h       # Main page
│   └── gallery_html_gz.h     # Gallery page
│
└── Documentation
    ├── README.md             # This file
    ├── INSTALLATION_GUIDE.md # Setup guide
    └── LIBRARIES.md          # Dependencies
```

### Adding Features

1. **New API Endpoint**:
   ```cpp
   // In web_server.h
   server.on("/my_endpoint", HTTP_GET, handleMyEndpoint);
   
   static void handleMyEndpoint() {
     server.send(200, "text/plain", "Hello!");
   }
   ```

2. **New Task**:
   ```cpp
   // In task_manager.h
   xTaskCreatePinnedToCore(
     myTask,                // Function
     "MyTask",              // Name
     4096,                  // Stack size
     NULL,                  // Parameters
     1,                     // Priority
     &myTaskHandle,         // Handle
     0                      // Core (0 or 1)
   );
   ```

3. **Custom Manager**:
   ```cpp
   // Create my_manager.h
   class MyManager {
   public:
     static void init();
     static void doSomething();
   private:
     static bool initialized;
   };
   ```

---

## 🤝 Contributing

This is a complete, working project ready for customization!

**Ideas for Enhancement**:
- Face detection (using ESP-DL)
- Motion detection
- Time-lapse mode
- WiFi AP mode for standalone operation
- OTA firmware updates
- MQTT integration
- Cloud photo backup
- Advanced image processing

---

## 📜 License

MIT License - Feel free to modify and use for your projects!

---

## 🙏 Acknowledgments

- **RandomNerdTutorials** - ESP32-CAM streaming example
- **ArduCAM** - Camera library
- **Bodmer** - TJpg_Decoder library
- **Waveshare** - LCD driver inspiration
- **Disney** - Stitch character theme

---

## 📞 Support

**Having Issues?**
1. Check [INSTALLATION_GUIDE.md](INSTALLATION_GUIDE.md) troubleshooting section
2. Verify wiring with pinout diagram
3. Test components individually
4. Enable DEBUG in config.h for detailed logs
5. Check Serial Monitor output

**Success Criteria**:
- ✅ All managers initialize successfully
- ✅ LCD shows live camera preview
- ✅ Buttons trigger captures
- ✅ Photos save to SD card
- ✅ Web interface accessible
- ✅ LED indicates system status

---

<div align="center">

**Made with 💙 for the Stitch Camera project**

*Ohana means family, and family means nobody gets left behind!* 🌺

[⬆ Back to Top](#-stitch-camera-v20---professional-rtos-edition)

</div>
