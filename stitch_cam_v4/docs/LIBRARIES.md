# Library Dependencies

## Required Libraries

### 1. ArduCAM
- **Name**: ArduCAM
- **Author**: Lee
- **Version**: Latest
- **Installation**: Arduino Library Manager → Search "ArduCAM"
- **Purpose**: Interface with OV2640 camera module
- **GitHub**: https://github.com/ArduCAM/Arduino
- **Notes**: 
  - Must include WiFi.h BEFORE ArduCAM headers to avoid macro conflicts
  - memorysaver.h configures which camera models to compile

### 2. TJpg_Decoder
- **Name**: TJpg_Decoder
- **Author**: Bodmer
- **Version**: Latest
- **Installation**: Arduino Library Manager → Search "TJpg_Decoder"
- **Purpose**: Decode JPEG images to RGB565 format for LCD display
- **GitHub**: https://github.com/Bodmer/TJpg_Decoder
- **Notes**: 
  - Optimized for ESP32
  - Supports callback-based decoding for streaming to LCD

### 3. ESP32 Core Libraries (Built-in)
These are included with the ESP32 board package:

#### WiFi
- **Purpose**: WiFi connectivity and management
- **Header**: `<WiFi.h>`
- **Included in**: esp32 board package

#### WebServer
- **Purpose**: HTTP web server for camera control interface
- **Header**: `<WebServer.h>`
- **Included in**: esp32 board package

#### SD
- **Purpose**: SD card file system operations
- **Header**: `<SD.h>`
- **Included in**: esp32 board package

#### SPI
- **Purpose**: SPI communication with camera and SD card
- **Header**: `<SPI.h>`
- **Included in**: esp32 board package

## Hardware Driver Libraries (Included in Project)

These are custom drivers included in the `hardware/` folder:

### LCD_Driver
- **Files**: LCD_Driver.h, LCD_Driver.cpp
- **Purpose**: ST7789 LCD control with DMA support
- **Resolution**: 240×320 pixels
- **Interface**: FSPI (Fast SPI)

### DEV_Config
- **Files**: DEV_Config.h, DEV_Config.cpp
- **Purpose**: Hardware configuration for SPI, GPIO
- **Features**: DMA buffer management, bulk SPI transfers

### GUI_Paint
- **Files**: GUI_Paint.h, GUI_Paint.cpp
- **Purpose**: Graphics primitives (text, shapes, colors)
- **Features**: 
  - Font rendering (Font16, Font24)
  - Rectangle/line drawing
  - Fast filled rectangle operations

### Debug
- **File**: Debug.h
- **Purpose**: Debug macros for serial output
- **Features**: Conditional compilation based on DEBUG flag

## Web Interface (Compressed HTML)

### index_html_gz.h
- **Purpose**: Main camera control page (GZIP compressed)
- **Size**: ~2.9KB compressed
- **Theme**: Disney Stitch tropical theme
- **Features**: 
  - Live camera preview
  - Instant/countdown capture
  - Mode toggle switch
  - Status display

### gallery_html_gz.h
- **Purpose**: Photo gallery page (GZIP compressed)
- **Size**: ~2.5KB compressed
- **Features**:
  - Grid photo display
  - Photo deletion
  - Download individual photos

## Installation Methods

### Method 1: Arduino Library Manager (Recommended)

1. Open Arduino IDE
2. Go to **Sketch → Include Library → Manage Libraries**
3. Search for each library by name
4. Click **Install**

### Method 2: Manual Installation

```bash
cd ~/Documents/Arduino/libraries/

# Clone ArduCAM
git clone https://github.com/ArduCAM/Arduino.git ArduCAM

# Clone TJpg_Decoder
git clone https://github.com/Bodmer/TJpg_Decoder.git

# Restart Arduino IDE
```

### Method 3: ZIP Import

1. Download library ZIP from GitHub
2. In Arduino IDE: **Sketch → Include Library → Add .ZIP Library**
3. Select downloaded ZIP file

## Version Compatibility

| Library | Minimum Version | Tested Version | Notes |
|---------|----------------|----------------|-------|
| ArduCAM | 1.0.0 | Latest | Use latest for best OV2640 support |
| TJpg_Decoder | 1.0.0 | Latest | ESP32 optimizations in recent versions |
| ESP32 Core | 2.0.14 | 2.0.17 | Requires 2.0.14+ for full PSRAM support |

## Troubleshooting Library Issues

### ArduCAM Compilation Errors

**Error**: `macro "swap" requires 3 arguments`

**Solution**: Ensure WiFi.h is included BEFORE ArduCAM headers:
```cpp
#include <WiFi.h>  // MUST BE FIRST
#include <ArduCAM.h>
```

### TJpg_Decoder Not Found

**Solution**:
1. Verify library installed: `~/Documents/Arduino/libraries/TJpg_Decoder/`
2. Restart Arduino IDE
3. Check library examples appear in **File → Examples → TJpg_Decoder**

### SD Card Library Conflicts

**Solution**: ESP32 SD library is different from Arduino SD library.
- Use `#include <SD.h>` (ESP32 version)
- NOT `#include <SD_MMC.h>` (different interface)

### SPI Conflicts

The project uses TWO SPI buses:
- **HSPI**: Camera + SD Card (pins 10-13)
- **FSPI**: LCD Display (pins 4-5)

If you get SPI conflicts, verify both buses are initialized in hardware_manager.h.

## Memory Requirements

| Component | Flash | SRAM | PSRAM |
|-----------|-------|------|-------|
| ArduCAM | ~50KB | ~5KB | 0 |
| TJpg_Decoder | ~30KB | ~10KB | Optional |
| WiFi/WebServer | ~200KB | ~40KB | 0 |
| LCD Drivers | ~80KB | ~20KB | 0 |
| Application Code | ~300KB | ~50KB | 0 |
| **Total** | **~700KB** | **~120KB** | **200KB** (buffers) |

ESP32-S3 N16R8 has:
- **16MB Flash** (plenty of room)
- **512KB SRAM** (sufficient)
- **8MB PSRAM** (optional for larger frame buffers)

## Verifying Installation

After installing libraries, compile this test sketch:

```cpp
#include <WiFi.h>
#include <ArduCAM.h>
#include <SD.h>
#include <TJpg_Decoder.h>

void setup() {
  Serial.begin(115200);
  Serial.println("All libraries found!");
}

void loop() {}
```

If it compiles successfully, all libraries are installed correctly.

## Updating Libraries

### Arduino Library Manager
1. **Sketch → Include Library → Manage Libraries**
2. Search for library name
3. Click **Update** if available

### Manual Update
```bash
cd ~/Documents/Arduino/libraries/ArduCAM/
git pull origin master

cd ~/Documents/Arduino/libraries/TJpg_Decoder/
git pull origin master
```

## License Information

- **ArduCAM**: LGPL 2.1
- **TJpg_Decoder**: BSD License
- **ESP32 Core**: Apache 2.0
- **This Project**: MIT (see LICENSE file)

## Additional Resources

- **ArduCAM Documentation**: http://www.arducam.com/
- **TJpg_Decoder Examples**: Included in library
- **ESP32 Arduino Core**: https://github.com/espressif/arduino-esp32
- **ESP32-S3 Datasheet**: https://www.espressif.com/en/products/socs/esp32-s3
