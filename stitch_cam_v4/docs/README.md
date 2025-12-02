# 🌺 Stitch Cam v2.0 - Clean RTOS Architecture

## 📁 Project Structure

```
stitch_cam_v2/
├── main.ino                    # Main entry point
├── config.h                    # Central configuration
│
├── Managers/                   # Modular system components
│   ├── hardware_manager.h      # GPIO, SPI, I2C, LED init
│   ├── camera_manager.h        # Camera capture & control
│   ├── lcd_manager.h           # LCD display & graphics
│   ├── storage_manager.h       # SD card & file operations
│   ├── wifi_manager.h          # WiFi connection
│   ├── web_server.h            # HTTP server & routes
│   ├── ui_manager.h            # Buttons & user interface
│   └── task_manager.h          # FreeRTOS task creation
│
├── Hardware/                   # Low-level drivers (your existing files)
│   ├── DEV_Config.h/.cpp       # LCD SPI & DMA
│   ├── LCD_Driver.h/.cpp       # ST7789 LCD driver
│   ├── GUI_Paint.h/.cpp        # Graphics library
│   └── Debug.h                 # Debug macros
│
└── Web/                        # Web interface
    ├── index_html_gz.h         # Main page (gzipped)
    └── gallery_html_gz.h       # Gallery page (gzipped)
```

---

## 🏗️ Architecture Overview

### **Dual-Core RTOS Design**

```
┌─────────────────────────────────────────────────────────────┐
│                        ESP32-S3                              │
├──────────────────────────┬──────────────────────────────────┤
│         CORE 0           │           CORE 1                 │
│    (Hardware Tasks)      │      (Network Tasks)             │
├──────────────────────────┼──────────────────────────────────┤
│                          │                                  │
│  📷 Camera Task          │  🌐 WiFi Manager                │
│   - Capture frames       │   - Connection handling         │
│   - JPEG encode          │                                  │
│   - Live preview         │  🖥️  Web Server                 │
│                          │   - HTTP routes                  │
│  🖼️  LCD Task            │   - API endpoints               │
│   - Display frames       │   - File serving                │
│   - UI rendering         │                                  │
│                          │  📊 Status API                  │
│  🎛️  UI Task             │   - JSON responses              │
│   - Button handling      │   - Real-time updates           │
│   - Mode switching       │                                  │
│                          │                                  │
│  💾 Storage Task         │                                  │
│   - Photo saving         │                                  │
│   - SD card ops          │                                  │
└──────────────────────────┴──────────────────────────────────┘
```

---

## 🎯 Key Features

### ✅ **Clean Separation of Concerns**
- Each manager handles ONE responsibility
- Easy to test, modify, and extend
- No circular dependencies

### ✅ **True RTOS Multitasking**
- FreeRTOS tasks for parallel execution
- Core 0: Camera/LCD/UI (real-time)
- Core 1: WiFi/Web (network)

### ✅ **Memory Optimized**
- DMA transfers for LCD (16KB buffer)
- GZIP compressed HTML (saves RAM)
- Efficient frame buffers

### ✅ **Professional Code Quality**
- Namespace organization
- Const correctness
- Type-safe enums
- Comprehensive comments

---

## 🔧 Component Details

### **1. Hardware Manager** (`hardware_manager.h`)
- Initialize GPIO pins
- Configure SPI buses (Camera + LCD)
- Setup I2C for camera control
- RGB LED control with PWM

### **2. Camera Manager** (`camera_manager.h`)
- ArduCAM OV2640 control
- JPEG capture to buffer
- Frame grabbing & streaming
- Auto-exposure & settings

### **3. LCD Manager** (`lcd_manager.h`)
- ST7789 display driver
- RGB565 frame rendering
- DMA accelerated transfers
- UI overlays & messages

### **4. Storage Manager** (`storage_manager.h`)
- SD card initialization
- Photo save/delete operations
- Gallery file listing
- Filename generation

### **5. WiFi Manager** (`wifi_manager.h`)
- Connect to WiFi
- Auto-reconnect
- Status monitoring
- IP address display

### **6. Web Server** (`web_server.h`)
- HTTP server on port 80
- RESTful API endpoints
- GZIP compressed pages
- MJPEG streaming

### **7. UI Manager** (`ui_manager.h`)
- Button interrupt handling
- Debouncing logic
- Mode switching
- Countdown timers

### **8. Task Manager** (`task_manager.h`)
- Create FreeRTOS tasks
- Configure priorities
- Assign cores
- Inter-task communication

---

## 📡 API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Main control page |
| `/gallery` | GET | Photo gallery page |
| `/capture` | GET | Trigger instant capture |
| `/toggle` | GET | Switch capture mode |
| `/countdown_start` | GET | Start 3-second countdown |
| `/status` | GET | System status (JSON) |
| `/stream` | GET | Live MJPEG frame |
| `/photos` | GET | List all photos (JSON) |
| `/photo?name=X` | GET | Download specific photo |
| `/delete?name=X` | GET | Delete photo |

---

## 🎨 Stitch Cam Features

### **Capture Modes**
- **Instant**: Immediate photo capture
- **Countdown**: 3-second timer with LED feedback

### **User Interface**
- Button 1: Capture photo
- Button 2: Toggle mode
- RGB LED status indicators:
  - 🔴 Red: Countdown/Capturing
  - 🟢 Green: Photo saved
  - 🔵 Blue: Idle/Ready
  - 🟡 Yellow: SD card error

### **Web Interface**
- Live camera preview
- Gallery browser
- Remote capture control
- Photo download/delete

---

## 🚀 Getting Started

### **1. Hardware Setup**

Connect your components according to pin definitions in `config.h`:

```cpp
ESP32-S3          Component
----------------------------------------
GPIO 10-13    →   ArduCAM OV2640 (HSPI)
GPIO 9-8      →   Camera I2C
GPIO 14       →   SD Card CS
GPIO 4-5,17-18→   ST7789 LCD (FSPI)
GPIO 1,45     →   Buttons
GPIO 2,42,41  →   RGB LED
```

### **2. Software Setup**

1. Install Arduino IDE
2. Install ESP32 board support (v3.0+)
3. Install required libraries:
   - ArduCAM
   - TJpg_Decoder
   - SD
   - WiFi

4. Update `config.h`:
```cpp
#define WIFI_SSID     "YourWiFi"
#define WIFI_PASSWORD "YourPassword"
```

5. Upload to ESP32-S3

### **3. Usage**

1. Power on → Auto-connects to WiFi
2. Serial Monitor shows IP address
3. Open browser: `http://192.168.x.xxx`
4. Use buttons or web interface to capture photos!

---

## 🔬 Technical Specifications

| Feature | Specification |
|---------|--------------|
| MCU | ESP32-S3 (Dual-core 240MHz) |
| Camera | OV2640 (2MP) |
| Display | ST7789 240×320 |
| Storage | MicroSD (FAT32) |
| Network | WiFi 2.4GHz |
| Frame Rate | ~10-15 FPS (320×240) |
| JPEG Size | ~10-30KB per photo |

---

## 🧪 Advantages of This Architecture

### **vs. Monolithic Code**

| Aspect | Monolithic | Modular (This) |
|--------|-----------|----------------|
| Readability | ❌ Difficult | ✅ Clear |
| Debugging | ❌ Hard | ✅ Easy |
| Testing | ❌ Complex | ✅ Simple |
| Collaboration | ❌ Conflicts | ✅ Smooth |
| Scalability | ❌ Limited | ✅ Excellent |

### **Performance Benefits**

- ⚡ **True parallelism**: Camera + Web run simultaneously
- ⚡ **No blocking**: Web server doesn't freeze camera
- ⚡ **Efficient**: DMA transfers + hardware acceleration
- ⚡ **Responsive**: Instant button feedback

---

## 📊 Memory Usage

```
Flash:  ~700KB (program)
SRAM:   ~120KB (runtime)
  - Camera buffer: 32KB
  - Frame buffer:  153KB (320×240×2)
  - DMA buffer:    16KB
  - Stack/heap:    ~50KB
PSRAM:  ~200KB (if available - for larger frames)
```

---

## 🛠️ Customization

### **Change Resolution**
Edit `config.h`:
```cpp
constexpr uint16_t FRAME_WIDTH  = 640;   // Higher resolution
constexpr uint16_t FRAME_HEIGHT = 480;
```

### **Adjust Task Priorities**
Edit `config.h`:
```cpp
constexpr UBaseType_t CAMERA_PRIORITY = 3;  // Higher = more priority
```

### **Add New Features**

1. Create new manager (e.g., `sensor_manager.h`)
2. Add to `task_manager.h` if needs own task
3. Include in `main.ino`
4. Done! No spaghetti code!

---

## 🐛 Debugging

Enable detailed logging in `config.h`:
```cpp
#define DEBUG_ENABLED 1
```

Check Serial Monitor (115200 baud) for:
- Initialization status
- Task execution
- Error messages
- Performance metrics

---

## 🎓 Learning Resources

This project demonstrates:
- ✅ FreeRTOS multitasking
- ✅ Dual-core programming
- ✅ Hardware abstraction
- ✅ Clean architecture patterns
- ✅ Web API design
- ✅ Memory optimization

Perfect for learning embedded systems development!

---

## 📝 License

Based on RandomNerdTutorials ESP32-CAM examples
Enhanced with clean architecture & RTOS design

---

**Made with 💙 for clean code enthusiasts!**
