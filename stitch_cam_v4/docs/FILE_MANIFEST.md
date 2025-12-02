# 📦 Stitch Camera v2 - Complete File Manifest

## 📂 Project Structure

```
stitch_cam_v2/
│
├── 📄 Core Arduino Files
│   ├── main.ino (1.4 KB)
│   │   └── Entry point, initializes all managers, creates RTOS tasks
│   ├── config.h (5.0 KB)
│   │   └── Centralized configuration: WiFi, pins, settings, namespaces
│   └── memorysaver.h (0.4 KB)
│       └── ArduCAM memory optimization (enables only OV2640)
│
├── 💼 Manager Files (Business Logic)
│   ├── hardware_manager.h (2.9 KB)
│   │   └── GPIO initialization, SPI setup (HSPI + FSPI), RGB LED PWM
│   ├── camera_manager.h (5.3 KB)
│   │   └── ArduCAM OV2640 control, JPEG capture, frame decoding
│   ├── lcd_manager.h (5.6 KB)
│   │   └── ST7789 display control, DMA transfers, UI rendering
│   ├── storage_manager.h (6.3 KB)
│   │   └── SD card operations, photo save/delete/list
│   ├── wifi_manager.h (3.5 KB)
│   │   └── WiFi connectivity, reconnection, status reporting
│   ├── web_server.h (6.5 KB)
│   │   └── HTTP server, RESTful API endpoints, GZIP serving
│   ├── ui_manager.h (5.6 KB)
│   │   └── Button interrupts, debouncing, mode management, LED control
│   └── task_manager.h (6.8 KB)
│       └── FreeRTOS task creation, camera/UI tasks, capture logic
│
├── 🔧 Hardware Drivers
│   └── hardware/
│       ├── DEV_Config.h (2.2 KB)
│       │   └── SPI/DMA configuration declarations
│       ├── DEV_Config.cpp (2.5 KB)
│       │   └── Hardware initialization, bulk SPI transfers
│       ├── LCD_Driver.h (0.8 KB)
│       │   └── ST7789 driver declarations
│       ├── LCD_Driver.cpp (7.1 KB)
│       │   └── ST7789 implementation, initialization, drawing
│       ├── GUI_Paint.h (6.9 KB)
│       │   └── Graphics library declarations, fonts, colors
│       ├── GUI_Paint.cpp (33.1 KB)
│       │   └── Drawing primitives: text, rectangles, lines, circles
│       ├── Debug.h (0.7 KB)
│       │   └── Debug macros for conditional serial output
│       ├── index_html_gz.h (17.5 KB)
│       │   └── Compressed main control page (Stitch theme)
│       └── gallery_html_gz.h (15.0 KB)
│           └── Compressed photo gallery page
│
└── 📚 Documentation
    ├── PROJECT_README.md (11.5 KB)
    │   └── Main project documentation, architecture, API reference
    ├── INSTALLATION_GUIDE.md (12.6 KB)
    │   └── Step-by-step setup, troubleshooting, verification
    ├── LIBRARIES.md (6.2 KB)
    │   └── Library dependencies, installation methods
    ├── QUICK_REFERENCE.md (4.8 KB)
    │   └── Cheat sheet for pinout, settings, common fixes
    └── README.md (9.5 KB)
        └── Original architecture documentation

```

---

## 📊 File Statistics

### Code Files
```
Type                    Files   Lines   Size
─────────────────────────────────────────────
.ino (Arduino)          1       51      1.4 KB
.h (Headers)            16      892     62.4 KB
.cpp (Implementation)   3       654     42.7 KB
─────────────────────────────────────────────
Total Code              20      1597    106.5 KB
```

### Documentation
```
Type                    Files   Size
───────────────────────────────────────
.md (Markdown)          5       44.6 KB
```

### Total Project
```
Files:  25
Size:   151.1 KB
```

---

## 🎯 File Purposes

### Entry Point
**main.ino**
- Includes all libraries and managers (WiFi FIRST to avoid ArduCAM conflicts)
- Calls init() on all managers in correct order
- Creates FreeRTOS tasks via TaskManager
- Runs web server on Core 1 in loop()

### Configuration
**config.h**
- WiFi credentials (WIFI_SSID, WIFI_PASSWORD)
- Pin assignments organized in `Pins` namespace
- Camera/LCD/Storage/UI/Task configuration in dedicated namespaces
- Type-safe enums: CaptureMode, SystemStatus, LEDColor
- Compile-time constants with `constexpr`

**memorysaver.h**
- Tells ArduCAM library to only compile OV2640 support
- Saves ~200 KB of flash by excluding other camera models

### Hardware Abstraction
**hardware_manager.h**
- Initializes all GPIO pins with correct modes
- Sets up dual SPI buses (HSPI for camera, FSPI for LCD)
- Configures I2C for camera communication
- Creates PWM channels for RGB LED (3 channels, 5kHz, 8-bit)
- Provides LED color control functions

### Camera Control
**camera_manager.h**
- Manages ArduCAM OV2640 camera module
- SPI communication for image data (FIFO buffer)
- I2C communication for sensor configuration
- JPEG capture to 32KB buffer
- Decoding JPEG to RGB565 for LCD display via TJpg_Decoder
- Quality/effect settings

### Display Management
**lcd_manager.h**
- Controls ST7789 240×320 LCD
- Fast DMA transfers for frame buffer (16KB chunks)
- Text rendering with multiple fonts (Font16, Font24)
- Graphics primitives (rectangles, status bars)
- Countdown/status/error message displays
- Boot screen and WiFi status screens

### Storage Operations
**storage_manager.h**
- SD card initialization and management
- Auto-creates `/photos/` directory
- Saves JPEG files with sequential naming (IMG_0001.jpg, etc.)
- Lists/reads/deletes photos
- Tracks photo count
- FAT32 filesystem operations

### Network Connectivity
**wifi_manager.h**
- WiFi STA mode connection
- Hostname: "StitchCam"
- Auto-reconnection logic
- Signal strength monitoring (RSSI)
- Connection status reporting
- IP address management

### Web Server
**web_server.h**
- HTTP server on port 80
- Serves compressed HTML pages (GZIP)
- RESTful API endpoints (GET methods)
- Streams JPEG frames
- Photo download/delete handlers
- JSON status responses
- Security: prevents directory traversal attacks

### User Interface
**ui_manager.h**
- Button interrupt handlers with debouncing
- Capture/mode toggle request flags
- Countdown animation (3-2-1)
- LED status updates based on system state
- Thread-safe flag management for RTOS

### Task Orchestration
**task_manager.h**
- Creates dual-core FreeRTOS tasks
- **Camera Task** (Core 0): Continuous frame capture, LCD update
- **UI Task** (Core 0): Button handling, photo save logic
- Capture workflows: instant vs countdown
- Error handling and status display

### LCD Driver
**hardware/LCD_Driver.cpp/h**
- Low-level ST7789 commands
- Display initialization sequence
- Window/cursor management
- Backlight PWM control
- Color format conversion
- SPI communication optimized for DMA

### Graphics Library
**hardware/GUI_Paint.cpp/h**
- Bitmap fonts (Font8, Font12, Font16, Font20, Font24)
- Text rendering with background/foreground colors
- Drawing shapes: points, lines, circles, rectangles
- Fill operations with fast algorithms
- RGB565 color definitions
- Coordinate system management

### Web Assets
**hardware/index_html_gz.h**
- Main control page HTML/CSS/JS
- Stitch theme with tropical gradient
- Animated elements (wiggling ears, floating flowers)
- Toggle switch for Instant/Countdown modes
- Camera preview placeholder
- Status message display
- GZIP compressed to ~2.9 KB

**hardware/gallery_html_gz.h**
- Photo gallery grid layout
- Responsive design
- Photo deletion buttons
- Download functionality
- GZIP compressed to ~2.5 KB

---

## 🔄 Dependency Graph

```
main.ino
  ├─ config.h
  ├─ hardware_manager.h
  │   └─ config.h
  ├─ camera_manager.h
  │   ├─ config.h
  │   ├─ ArduCAM.h (external)
  │   ├─ memorysaver.h
  │   └─ TJpg_Decoder.h (external)
  ├─ lcd_manager.h
  │   ├─ config.h
  │   ├─ DEV_Config.h
  │   ├─ LCD_Driver.h
  │   └─ GUI_Paint.h
  ├─ storage_manager.h
  │   ├─ config.h
  │   └─ SD.h (ESP32 core)
  ├─ wifi_manager.h
  │   ├─ config.h
  │   └─ WiFi.h (ESP32 core)
  ├─ web_server.h
  │   ├─ config.h
  │   ├─ WebServer.h (ESP32 core)
  │   ├─ camera_manager.h
  │   ├─ storage_manager.h
  │   ├─ ui_manager.h
  │   ├─ index_html_gz.h
  │   └─ gallery_html_gz.h
  ├─ ui_manager.h
  │   ├─ config.h
  │   └─ hardware_manager.h
  └─ task_manager.h
      ├─ config.h
      ├─ camera_manager.h
      ├─ lcd_manager.h
      ├─ storage_manager.h
      ├─ ui_manager.h
      └─ hardware_manager.h
```

---

## 📝 Modification Guide

### To Change WiFi
```
Edit: config.h
Lines: 18-19
Change: WIFI_SSID and WIFI_PASSWORD
```

### To Change Pins
```
Edit: config.h
Lines: 24-68 (Pins namespace)
Example: constexpr uint8_t BUTTON_CAPTURE = 1;
```

### To Add New Endpoint
```
Edit: web_server.h
Add handler function
Register in init(): server.on("/endpoint", handleEndpoint);
```

### To Modify LCD Display
```
Edit: lcd_manager.h
Add new display functions
Use Paint_DrawString_EN() for text
Use Paint_DrawRectangle() for shapes
```

### To Change Camera Settings
```
Edit: camera_manager.h
Resolution: OV2640_set_JPEG_size()
Quality: setQuality()
Effects: setEffect()
```

### To Add New Task
```
Edit: task_manager.h
Create task function
Register with xTaskCreatePinnedToCore()
Choose Core 0 (hardware) or Core 1 (network)
```

---

## ✅ File Verification Checklist

Use this to ensure all files are present:

```
□ main.ino
□ config.h
□ memorysaver.h
□ hardware_manager.h
□ camera_manager.h
□ lcd_manager.h
□ storage_manager.h
□ wifi_manager.h
□ web_server.h
□ ui_manager.h
□ task_manager.h
□ hardware/DEV_Config.h
□ hardware/DEV_Config.cpp
□ hardware/LCD_Driver.h
□ hardware/LCD_Driver.cpp
□ hardware/GUI_Paint.h
□ hardware/GUI_Paint.cpp
□ hardware/Debug.h
□ hardware/index_html_gz.h
□ hardware/gallery_html_gz.h
□ PROJECT_README.md
□ INSTALLATION_GUIDE.md
□ LIBRARIES.md
□ QUICK_REFERENCE.md
□ README.md (this file)
```

**Total: 25 files**

---

## 🎓 Learning Path

If you're new to this codebase, read in this order:

1. **PROJECT_README.md** - Understand overall architecture
2. **config.h** - See all configurable options
3. **main.ino** - Entry point and initialization flow
4. **hardware_manager.h** - How hardware is set up
5. **camera_manager.h** - Camera capture process
6. **lcd_manager.h** - Display operations
7. **task_manager.h** - How tasks coordinate
8. **web_server.h** - API endpoints
9. **INSTALLATION_GUIDE.md** - Build and troubleshoot

---

## 💡 Pro Tips

### Debugging
- Enable `#define DEBUG` in config.h
- Monitor Serial at 115200 baud
- Check return values in DEBUG_PRINTF statements

### Memory Optimization
- Reduce FRAME_WIDTH/HEIGHT if running low on PSRAM
- Decrease MAX_JPEG_SIZE if captures are smaller
- Adjust task stack sizes based on actual usage

### Performance Tuning
- Increase camera quality: setQuality(0) for best
- Adjust DMA_BUFFER_SIZE in DEV_Config.h
- Change task priorities in TaskConfig namespace

### Customization
- Decompress HTML files to modify web UI
- Add custom fonts in GUI_Paint.h
- Create new manager classes for additional features

---

<div align="center">

**📦 Complete Stitch Camera v2 File Manifest**

*All files accounted for and documented!*

</div>
