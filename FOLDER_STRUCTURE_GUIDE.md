# 📁 Minimal Demo - Folder Structure & File Templates

## 🗂️ Complete Folder Structure

```
camera_demo/                          ← Your project root
│
├── camera_demo.ino                   ← Main Arduino sketch (100 lines)
│
├── config/
│   └── pins.h                        ← Pin definitions (50 lines)
│
├── hardware/
│   ├── camera.cpp                    ← Camera operations (150 lines)
│   ├── camera.h                      ← Camera interface (20 lines)
│   ├── display.cpp                   ← Display operations (100 lines)
│   ├── display.h                     ← Display interface (15 lines)
│   ├── storage.cpp                   ← SD card operations (120 lines)
│   └── storage.h                     ← Storage interface (20 lines)
│
├── drivers/                          ← Keep your optimized drivers
│   ├── DEV_Config.cpp               ← From outputs folder
│   ├── DEV_Config.h                 ← From outputs folder
│   ├── LCD_Driver.cpp               ← From outputs folder
│   ├── LCD_Driver.h                 ← From outputs folder
│   ├── GUI_Paint.cpp                ← From outputs folder
│   ├── GUI_Paint.h                  ← From outputs folder
│   └── fonts.h                      ← Font definitions
│
└── libraries/                        ← External libraries (if needed)
    └── (ArduCAM, SD, etc.)

Total: ~600 lines of new code + existing drivers
```

---

## 📄 File Templates

### 1. `config/pins.h`

```cpp
/**
 * Pin Configuration for Camera Demo
 * ESP32-C6 with OV2640 Camera + 240x320 LCD + SD Card
 */

#ifndef PINS_H
#define PINS_H

//=============================================================================
// CAMERA PINS (SPI)
//=============================================================================
#define PIN_CAM_CS      18
#define PIN_CAM_SCK     21
#define PIN_CAM_MISO    20
#define PIN_CAM_MOSI    19

//=============================================================================
// I2C PINS (Camera control)
//=============================================================================
#define PIN_SDA         22
#define PIN_SCL         23

//=============================================================================
// LCD PINS (SPI via DEV_Config)
//=============================================================================
#define PIN_LCD_CS      2
#define PIN_LCD_DC      3
#define PIN_LCD_RST     10
#define PIN_LCD_BL      11
#define PIN_LCD_SCK     6
#define PIN_LCD_MOSI    5

//=============================================================================
// SD CARD PINS (Shared SPI with LCD)
//=============================================================================
#define PIN_SD_CS       7      // Separate CS
#define PIN_SD_SCK      6      // Shared with LCD
#define PIN_SD_MOSI     5      // Shared with LCD
#define PIN_SD_MISO     4      // Separate MISO

//=============================================================================
// INPUT BUTTONS
//=============================================================================
#define PIN_BTN_CAPTURE 12     // Capture photo
#define PIN_BTN_VIEW    13     // View last photo

//=============================================================================
// STATUS LED
//=============================================================================
#define PIN_LED         8      // Status indicator

//=============================================================================
// DISPLAY CONFIGURATION
//=============================================================================
#define LCD_WIDTH       240
#define LCD_HEIGHT      320

#define FRAME_WIDTH     240
#define FRAME_HEIGHT    320
#define FRAME_SIZE      (FRAME_WIDTH * FRAME_HEIGHT * 2)  // RGB565

//=============================================================================
// CAMERA CONFIGURATION
//=============================================================================
#define CAM_WIDTH       320
#define CAM_HEIGHT      240

#endif // PINS_H
```

---

### 2. `hardware/camera.h`

```cpp
/**
 * Camera Hardware Abstraction
 * Simple interface for OV2640 camera operations
 */

#ifndef CAMERA_H
#define CAMERA_H

#include <Arduino.h>

//=============================================================================
// INITIALIZATION
//=============================================================================
/**
 * Initialize camera hardware
 * @return true if successful
 */
bool Camera_Init();

//=============================================================================
// FRAME CAPTURE
//=============================================================================
/**
 * Capture frame to RGB565 buffer (pre-rotated for display)
 * @param buffer Output buffer (must be 240*320*2 bytes)
 * @param width Output width (240)
 * @param height Output height (320)
 * @return true if successful
 */
bool Camera_CaptureFrame(uint8_t* buffer, uint16_t width, uint16_t height);

/**
 * Capture frame directly to JPEG (for saving)
 * @param jpegBuffer Output buffer for JPEG data
 * @param jpegSize Output size of JPEG data
 * @param maxSize Maximum size of jpegBuffer
 * @return true if successful
 */
bool Camera_CaptureJPEG(uint8_t* jpegBuffer, size_t* jpegSize, size_t maxSize);

//=============================================================================
// SETTINGS
//=============================================================================
/**
 * Set image quality (0-100, higher = better quality, larger file)
 */
void Camera_SetQuality(uint8_t quality);

/**
 * Check if camera is ready
 */
bool Camera_IsReady();

#endif // CAMERA_H
```

---

### 3. `hardware/display.h`

```cpp
/**
 * Display Hardware Abstraction
 * Simple interface for LCD operations
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

//=============================================================================
// INITIALIZATION
//=============================================================================
bool Display_Init();
void Display_SetBrightness(uint8_t brightness);  // 0-100

//=============================================================================
// FRAME DISPLAY
//=============================================================================
/**
 * Display RGB565 frame buffer (fast DMA transfer)
 */
void Display_ShowFrame(const uint8_t* frameBuffer);

/**
 * Display JPEG image (decode and show)
 */
bool Display_ShowJPEG(const uint8_t* jpegData, size_t jpegSize);

/**
 * Clear screen to color
 */
void Display_Clear(uint16_t color);

//=============================================================================
// TEXT & OVERLAYS
//=============================================================================
/**
 * Draw text string
 */
void Display_DrawText(uint16_t x, uint16_t y, const char* text, 
                      uint16_t fgColor, uint16_t bgColor);

/**
 * Show notification message (centered, auto-clear)
 */
void Display_ShowNotification(const char* message, uint16_t durationMs);

/**
 * Update notification (call in loop to auto-clear)
 */
void Display_UpdateNotification();

//=============================================================================
// COLORS (RGB565)
//=============================================================================
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F

#endif // DISPLAY_H
```

---

### 4. `hardware/storage.h`

```cpp
/**
 * Storage Hardware Abstraction
 * Simple interface for SD card operations
 */

#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>

//=============================================================================
// INITIALIZATION
//=============================================================================
bool Storage_Init();
bool Storage_IsReady();
uint64_t Storage_GetFreeSpace();  // Bytes

//=============================================================================
// PHOTO OPERATIONS
//=============================================================================
/**
 * Save JPEG photo to SD card
 * @param jpegData JPEG data buffer
 * @param jpegSize Size of JPEG data
 * @param photoId Output photo ID (file number)
 * @return true if successful
 */
bool Storage_SavePhoto(const uint8_t* jpegData, size_t jpegSize, uint32_t* photoId);

/**
 * Load last saved photo
 * @param jpegData Output buffer pointer (allocated by function, must free!)
 * @param jpegSize Output size of JPEG data
 * @return true if successful
 */
bool Storage_LoadLastPhoto(uint8_t** jpegData, size_t* jpegSize);

/**
 * Load specific photo by ID
 */
bool Storage_LoadPhoto(uint32_t photoId, uint8_t** jpegData, size_t* jpegSize);

/**
 * Delete last photo
 */
bool Storage_DeleteLastPhoto();

//=============================================================================
// STATISTICS
//=============================================================================
/**
 * Get total number of photos saved
 */
uint32_t Storage_GetPhotoCount();

/**
 * Get total storage used by photos (bytes)
 */
uint64_t Storage_GetUsedSpace();

#endif // STORAGE_H
```

---

### 5. `camera_demo.ino` (Main Sketch Template)

```cpp
/**
 * ESP32-C6 Camera Demo - Minimal Version
 * 
 * Features:
 * - Live camera preview (10+ FPS)
 * - Capture photos to SD card (JPEG)
 * - View last captured photo
 * - Photo counter display
 * 
 * Hardware:
 * - ESP32-C6
 * - OV2640 Camera
 * - 240x320 LCD
 * - MicroSD Card
 * - 2 Buttons
 */

#include "config/pins.h"
#include "hardware/camera.h"
#include "hardware/display.h"
#include "hardware/storage.h"

//=============================================================================
// APPLICATION STATE
//=============================================================================
enum AppState {
    STATE_PREVIEW,    // Live camera preview
    STATE_CAPTURE,    // Capturing and saving photo
    STATE_VIEWING     // Viewing saved photo
};

AppState currentState = STATE_PREVIEW;
uint32_t photoCount = 0;
uint8_t frameBuffer[FRAME_SIZE];  // 240*320*2 bytes

//=============================================================================
// TIMING & STATS
//=============================================================================
uint32_t lastFrameTime = 0;
uint32_t frameCount = 0;
float currentFPS = 0;

//=============================================================================
// SETUP
//=============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n╔═══════════════════════════════════╗");
    Serial.println("║   ESP32-C6 Camera Demo v1.0       ║");
    Serial.println("╚═══════════════════════════════════╝\n");
    
    // Initialize inputs
    pinMode(PIN_BTN_CAPTURE, INPUT_PULLUP);
    pinMode(PIN_BTN_VIEW, INPUT_PULLUP);
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);
    
    // Initialize hardware
    Serial.print("→ Initializing Camera... ");
    if (!Camera_Init()) {
        Serial.println("FAILED!");
        while(1) { delay(1000); }
    }
    Serial.println("OK");
    
    Serial.print("→ Initializing Display... ");
    if (!Display_Init()) {
        Serial.println("FAILED!");
        while(1) { delay(1000); }
    }
    Serial.println("OK");
    
    Serial.print("→ Initializing Storage... ");
    if (!Storage_Init()) {
        Serial.println("FAILED!");
        Display_ShowNotification("No SD Card!", 0);
        // Continue without SD (preview only)
    } else {
        Serial.println("OK");
        photoCount = Storage_GetPhotoCount();
        Serial.printf("   Found %d photos\n", photoCount);
    }
    
    Display_Clear(COLOR_BLACK);
    Serial.println("\n✓ Ready! Starting preview...\n");
    
    lastFrameTime = millis();
}

//=============================================================================
// MAIN LOOP
//=============================================================================
void loop() {
    // Update notification system
    Display_UpdateNotification();
    
    // Handle current state
    switch(currentState) {
        case STATE_PREVIEW:
            handlePreview();
            break;
        case STATE_CAPTURE:
            handleCapture();
            break;
        case STATE_VIEWING:
            handleViewing();
            break;
    }
    
    // Calculate FPS every second
    frameCount++;
    uint32_t now = millis();
    if (now - lastFrameTime >= 1000) {
        currentFPS = frameCount * 1000.0 / (now - lastFrameTime);
        Serial.printf("FPS: %.1f\n", currentFPS);
        frameCount = 0;
        lastFrameTime = now;
    }
}

//=============================================================================
// STATE: PREVIEW MODE
//=============================================================================
void handlePreview() {
    // Capture frame
    if (!Camera_CaptureFrame(frameBuffer, FRAME_WIDTH, FRAME_HEIGHT)) {
        Serial.println("Capture failed!");
        delay(100);
        return;
    }
    
    // Display frame
    Display_ShowFrame(frameBuffer);
    
    // Draw overlay
    char text[32];
    sprintf(text, "Photos: %d", photoCount);
    Display_DrawText(10, 10, text, COLOR_WHITE, COLOR_BLACK);
    
    // Check buttons
    if (digitalRead(PIN_BTN_CAPTURE) == LOW) {
        delay(50);  // Debounce
        if (digitalRead(PIN_BTN_CAPTURE) == LOW) {
            currentState = STATE_CAPTURE;
        }
    }
    
    if (digitalRead(PIN_BTN_VIEW) == LOW && photoCount > 0) {
        delay(50);  // Debounce
        if (digitalRead(PIN_BTN_VIEW) == LOW) {
            currentState = STATE_VIEWING;
        }
    }
}

//=============================================================================
// STATE: CAPTURE MODE
//=============================================================================
void handleCapture() {
    Serial.println("→ Capturing photo...");
    Display_ShowNotification("Saving...", 0);
    digitalWrite(PIN_LED, HIGH);
    
    // Allocate JPEG buffer
    uint8_t* jpegBuffer = (uint8_t*)malloc(50000);  // 50KB
    if (!jpegBuffer) {
        Display_ShowNotification("Out of memory!", 2000);
        currentState = STATE_PREVIEW;
        return;
    }
    
    size_t jpegSize;
    bool success = false;
    
    // Capture JPEG
    if (Camera_CaptureJPEG(jpegBuffer, &jpegSize, 50000)) {
        Serial.printf("   JPEG size: %d bytes\n", jpegSize);
        
        // Save to SD
        uint32_t photoId;
        if (Storage_SavePhoto(jpegBuffer, jpegSize, &photoId)) {
            photoCount++;
            Serial.printf("   Saved as photo #%d\n", photoId);
            Display_ShowNotification("Saved!", 1000);
            success = true;
        } else {
            Serial.println("   Save failed!");
            Display_ShowNotification("Save failed!", 2000);
        }
    } else {
        Serial.println("   Capture failed!");
        Display_ShowNotification("Capture failed!", 2000);
    }
    
    free(jpegBuffer);
    digitalWrite(PIN_LED, LOW);
    
    // Wait for button release
    while (digitalRead(PIN_BTN_CAPTURE) == LOW) {
        delay(10);
    }
    
    delay(500);
    currentState = STATE_PREVIEW;
}

//=============================================================================
// STATE: VIEWING MODE
//=============================================================================
void handleViewing() {
    Serial.println("→ Loading last photo...");
    Display_ShowNotification("Loading...", 0);
    
    uint8_t* jpegData;
    size_t jpegSize;
    
    if (Storage_LoadLastPhoto(&jpegData, &jpegSize)) {
        Serial.printf("   Loaded %d bytes\n", jpegSize);
        
        if (Display_ShowJPEG(jpegData, jpegSize)) {
            Serial.println("   Displayed photo");
        } else {
            Display_ShowNotification("Display failed!", 2000);
        }
        
        free(jpegData);
    } else {
        Serial.println("   Load failed!");
        Display_ShowNotification("Load failed!", 2000);
        delay(2000);
        currentState = STATE_PREVIEW;
        return;
    }
    
    // Wait for button release
    while (digitalRead(PIN_BTN_VIEW) == LOW) {
        delay(10);
    }
    
    // Wait for any button press to return
    while (digitalRead(PIN_BTN_CAPTURE) == HIGH && 
           digitalRead(PIN_BTN_VIEW) == HIGH) {
        delay(10);
    }
    
    currentState = STATE_PREVIEW;
}
```

---

## 🔧 Implementation Notes

### Camera.cpp Key Points:
```cpp
// Wrap your existing optimized code:
bool Camera_CaptureFrame(uint8_t* buffer, uint16_t w, uint16_t h) {
    // Your existing captureFramePreRotated() code here
    // Keep the pre-rotation optimization!
    return true;
}

// For JPEG capture:
bool Camera_CaptureJPEG(uint8_t* jpegBuffer, size_t* jpegSize, size_t maxSize) {
    // Option 1: Capture RGB565 then encode
    uint8_t* rgbBuffer = (uint8_t*)malloc(FRAME_SIZE);
    Camera_CaptureFrame(rgbBuffer, 240, 320);
    // Use esp_jpg_encode() here
    
    // Option 2: If camera supports direct JPEG
    // Use camera's JPEG mode (faster but lower quality)
    
    free(rgbBuffer);
    return true;
}
```

### Display.cpp Key Points:
```cpp
// Keep your optimized DMA transfer:
void Display_ShowFrame(const uint8_t* frameBuffer) {
    LCD_SetCursor(0, 0, 239, 319);
    DEV_SPI_Write_Bulk_Start();
    
    // Send entire frame
    uint32_t sent = 0;
    while (sent < FRAME_SIZE) {
        uint32_t chunk = min(DMA_BUFFER_SIZE, FRAME_SIZE - sent);
        DEV_SPI_Write_Bulk_Data(&frameBuffer[sent], chunk);
        sent += chunk;
    }
    
    DEV_SPI_Write_Bulk_End();
}
```

### Storage.cpp Key Points:
```cpp
// Simple counter file for photo numbering:
uint32_t Storage_GetPhotoCount() {
    File f = SD.open("/counter.txt", FILE_READ);
    if (!f) return 0;
    
    uint32_t count = f.parseInt();
    f.close();
    return count;
}

bool Storage_SavePhoto(const uint8_t* jpegData, size_t jpegSize, uint32_t* photoId) {
    uint32_t count = Storage_GetPhotoCount();
    
    char filename[32];
    sprintf(filename, "/DCIM/IMG_%04d.jpg", count);
    
    File f = SD.open(filename, FILE_WRITE);
    if (!f) return false;
    
    f.write(jpegData, jpegSize);
    f.close();
    
    // Update counter
    f = SD.open("/counter.txt", FILE_WRITE);
    f.println(count + 1);
    f.close();
    
    *photoId = count;
    return true;
}
```

---

## 📋 Migration Checklist

### Step 1: Create Structure
- [ ] Create folders: config/, hardware/, drivers/
- [ ] Copy optimized drivers to drivers/
- [ ] Create empty .cpp/.h files in hardware/

### Step 2: Move Code
- [ ] Copy pin definitions to config/pins.h
- [ ] Move camera capture to hardware/camera.cpp
- [ ] Move display code to hardware/display.cpp
- [ ] Create new storage.cpp for SD operations

### Step 3: Simplify Main
- [ ] Reduce camera_demo.ino to ~100 lines
- [ ] Use simple state machine
- [ ] Call hardware/* functions

### Step 4: Test Each Part
- [ ] Test camera capture (10+ FPS?)
- [ ] Test SD card init
- [ ] Test button inputs
- [ ] Test full flow

---

This structure keeps your optimized code while organizing it for future expansion!
