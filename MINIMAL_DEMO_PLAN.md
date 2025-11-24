# 📸 Minimal Camera Demo - Quick Win Plan

## 🎯 Goal: Simple Working Demo in 2 Weeks

Build a **minimal but impressive demo** that shows the core concept:
- ✅ Live preview (get to 10+ FPS)
- ✅ Capture button saves photo to SD
- ✅ Simple counter shows photos taken
- ✅ One button to view last photo

**No gallery, no WiFi, no RTOS - just the essentials that work!**

---

## 🚨 First: Fix Your 2.3 FPS Issue!

Your FPS dropped from 12-18 to 2.3! Let's diagnose:

### Quick Checks:

1. **Are you using the memory-optimized files?**
   ```cpp
   // Check in DEV_Config.h:
   #define DMA_BUFFER_SIZE 16384  // Should be 16KB, not 8KB
   
   // Check you're using writeBytes():
   SPIlcd.writeBytes(data, len);  // NOT byte-by-byte transfer()
   ```

2. **Is pre-rotation enabled?**
   ```cpp
   // In your .ino file, should have:
   captureFramePreRotated(frameBuffer);  // NOT captureFrameToBuffer()
   streamFrameToLcd_Fast(frameBuffer);   // NOT streamFrameToLcd()
   ```

3. **Check SPI speed:**
   ```cpp
   // Should be 80MHz:
   SPISettings(80000000, MSBFIRST, SPI_MODE3)
   // NOT 8000000 or lower!
   ```

### Likely Problem: Wrong Files

You may be using the old files instead of the optimized ones. 

**Solution:** Use these exact files:
- DEV_Config.h (from outputs - 16KB buffer)
- DEV_Config.cpp (from outputs - writeBytes DMA)
- Integration_v2_memory_optimized.ino (pre-rotation)

---

## 📋 Minimal Demo Spec

### What It Does:
1. **Live Preview** - Camera feed on LCD at 10+ FPS
2. **Capture Button** - Press to save photo to SD card
3. **Photo Counter** - Shows "Photos: 5" on screen
4. **View Button** - Shows last captured photo
5. **Status LED** - Blinks when saving

### What It Doesn't Do (yet):
- ❌ No gallery/browsing (just last photo)
- ❌ No WiFi/Bluetooth
- ❌ No RTOS (simple loop is fine)
- ❌ No fancy UI (just text overlay)
- ❌ No thumbnails

**This is enough for a convincing demo!**

---

## 🗂️ Simplified File Structure

```
camera_demo/
│
├── camera_demo.ino              ← Main program (simple!)
│
├── config/
│   └── pins.h                   ← All pin definitions
│
├── hardware/
│   ├── camera.cpp/h             ← Camera operations
│   ├── display.cpp/h            ← LCD operations
│   └── storage.cpp/h            ← SD card operations
│
├── drivers/                     ← Keep optimized drivers
│   ├── DEV_Config.cpp/h
│   ├── LCD_Driver.cpp/h
│   └── GUI_Paint.cpp/h
│
└── utils/
    └── jpeg.cpp/h               ← JPEG encode/decode
```

**Just 10 files total!** Much simpler than the full plan.

---

## 📝 File-by-File Breakdown

### 1. `config/pins.h`
**Purpose:** Central pin configuration
```cpp
#ifndef PINS_H
#define PINS_H

// Camera pins
#define PIN_CAM_CS      18
#define PIN_CAM_SCK     21
#define PIN_CAM_MISO    20
#define PIN_CAM_MOSI    19

// I2C pins
#define PIN_SDA         22
#define PIN_SCL         23

// LCD pins (via DEV_Config)
#define PIN_LCD_CS      2
#define PIN_LCD_DC      3
#define PIN_LCD_RST     10
#define PIN_LCD_BL      11

// SD card pins
#define PIN_SD_CS       7
#define PIN_SD_SCK      6    // Shared with LCD
#define PIN_SD_MOSI     5    // Shared with LCD
#define PIN_SD_MISO     4

// Input buttons
#define PIN_BTN_CAPTURE 12
#define PIN_BTN_VIEW    13

// LED
#define PIN_LED         8

#endif
```

---

### 2. `hardware/camera.h`
**Purpose:** Simple camera operations
```cpp
#ifndef CAMERA_H
#define CAMERA_H

#include <Arduino.h>

// Simple interface
bool Camera_Init();
bool Camera_CaptureToBuffer(uint8_t* buffer, uint16_t width, uint16_t height);
bool Camera_CaptureToJPEG(uint8_t* jpegBuffer, size_t* jpegSize);

#endif
```

**What goes in camera.cpp:**
- Wrap your current ArduCAM code
- Keep the pre-rotation optimization
- Add JPEG capture method (for saving)

---

### 3. `hardware/display.h`
**Purpose:** Simple display operations
```cpp
#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

// Simple interface
bool Display_Init();
void Display_ShowFrame(const uint8_t* frameBuffer);
void Display_ShowPhoto(const uint8_t* jpegData, size_t jpegSize);
void Display_DrawText(uint16_t x, uint16_t y, const char* text, uint16_t color);
void Display_ShowNotification(const char* message);

#endif
```

**What goes in display.cpp:**
- Wrap your optimized LCD_Driver code
- Keep DMA performance
- Add simple text overlay

---

### 4. `hardware/storage.h`
**Purpose:** SD card operations
```cpp
#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>

// Simple interface
bool Storage_Init();
bool Storage_SavePhoto(const uint8_t* jpegData, size_t jpegSize, uint32_t* photoId);
bool Storage_LoadLastPhoto(uint8_t** jpegData, size_t* jpegSize);
uint32_t Storage_GetPhotoCount();
void Storage_DeleteLastPhoto();

#endif
```

**What goes in storage.cpp:**
- Initialize SD card (SPI mode is fine for demo)
- Save JPEG files as IMG_0001.jpg, IMG_0002.jpg
- Keep track of photo count in a simple counter file

---

### 5. `camera_demo.ino`
**Purpose:** Main program - keep it simple!
```cpp
#include "config/pins.h"
#include "hardware/camera.h"
#include "hardware/display.h"
#include "hardware/storage.h"

// States
enum AppState {
    STATE_PREVIEW,
    STATE_CAPTURE,
    STATE_VIEWING
};

AppState currentState = STATE_PREVIEW;
uint32_t photoCount = 0;
uint8_t frameBuffer[240 * 320 * 2];

void setup() {
    Serial.begin(115200);
    
    // Initialize hardware
    pinMode(PIN_BTN_CAPTURE, INPUT_PULLUP);
    pinMode(PIN_BTN_VIEW, INPUT_PULLUP);
    pinMode(PIN_LED, OUTPUT);
    
    Camera_Init();
    Display_Init();
    Storage_Init();
    
    photoCount = Storage_GetPhotoCount();
    
    Serial.println("Demo Ready!");
}

void loop() {
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
}

void handlePreview() {
    // Capture and display at max FPS
    Camera_CaptureToBuffer(frameBuffer, 240, 320);
    Display_ShowFrame(frameBuffer);
    
    // Show photo count overlay
    char text[32];
    sprintf(text, "Photos: %d", photoCount);
    Display_DrawText(10, 10, text, 0xFFFF);
    
    // Check buttons
    if (digitalRead(PIN_BTN_CAPTURE) == LOW) {
        currentState = STATE_CAPTURE;
    }
    if (digitalRead(PIN_BTN_VIEW) == LOW && photoCount > 0) {
        currentState = STATE_VIEWING;
    }
}

void handleCapture() {
    Display_ShowNotification("Saving...");
    digitalWrite(PIN_LED, HIGH);
    
    // Capture JPEG
    uint8_t jpegBuffer[50000];  // 50KB buffer
    size_t jpegSize;
    
    if (Camera_CaptureToJPEG(jpegBuffer, &jpegSize)) {
        uint32_t photoId;
        if (Storage_SavePhoto(jpegBuffer, jpegSize, &photoId)) {
            photoCount++;
            Display_ShowNotification("Saved!");
        } else {
            Display_ShowNotification("Error!");
        }
    }
    
    digitalWrite(PIN_LED, LOW);
    delay(1000);
    currentState = STATE_PREVIEW;
}

void handleViewing() {
    uint8_t* jpegData;
    size_t jpegSize;
    
    if (Storage_LoadLastPhoto(&jpegData, &jpegSize)) {
        Display_ShowPhoto(jpegData, jpegSize);
        free(jpegData);
    }
    
    // Wait for button release
    while (digitalRead(PIN_BTN_VIEW) == LOW) {
        delay(10);
    }
    
    // Back to preview on any button
    while (digitalRead(PIN_BTN_CAPTURE) == HIGH && 
           digitalRead(PIN_BTN_VIEW) == HIGH) {
        delay(10);
    }
    
    currentState = STATE_PREVIEW;
}
```

**That's the whole main program! ~100 lines.**

---

## 🔧 Code Optimizations for Future

### Current Code Issues to Fix:

#### 1. **Modularize Camera Code**
**Current:** Everything in main .ino file
**Better:** Separate camera.cpp
```cpp
// Move this into camera.cpp:
void captureFramePreRotated(uint8_t* buffer) {
    // All the SPI camera reading code
}

// Expose simple interface:
bool Camera_CaptureToBuffer(uint8_t* buffer, uint16_t w, uint16_t h) {
    captureFramePreRotated(buffer);
    return true;
}
```

#### 2. **Separate Display Code**
**Current:** Direct LCD_Driver calls everywhere
**Better:** Wrap in display.cpp
```cpp
void Display_ShowFrame(const uint8_t* frameBuffer) {
    LCD_SetCursor(0, 0, 239, 319);
    DEV_SPI_Write_Bulk_Start();
    // ... send frame
    DEV_SPI_Write_Bulk_End();
}

void Display_DrawText(uint16_t x, uint16_t y, const char* text, uint16_t color) {
    Paint_DrawString_EN(x, y, text, &Font16, BLACK, color);
}
```

#### 3. **Add SD Card Layer**
**New:** Create storage.cpp
```cpp
bool Storage_SavePhoto(const uint8_t* jpegData, size_t jpegSize, uint32_t* photoId) {
    // Read counter
    uint32_t count = readPhotoCounter();
    
    // Generate filename
    char filename[32];
    sprintf(filename, "/DCIM/IMG_%04d.jpg", count);
    
    // Write file
    File file = SD.open(filename, FILE_WRITE);
    file.write(jpegData, jpegSize);
    file.close();
    
    // Update counter
    writePhotoCounter(count + 1);
    
    *photoId = count;
    return true;
}
```

#### 4. **Fix Performance**
**Check these in order:**

**A. Verify DMA is working:**
```cpp
// In DEV_Config.cpp, should see:
void DEV_SPI_Write_Bulk_Data(const uint8_t *data, uint32_t len) {
    SPIlcd.writeBytes(data, len);  // ← This is fast!
    // NOT: for loop with transfer()  // ← This is slow!
}
```

**B. Verify pre-rotation:**
```cpp
// In main loop, should call:
captureFramePreRotated(frameBuffer);  // ← Rotation during capture
streamFrameToLcd_Fast(frameBuffer);   // ← Just DMA dump

// NOT:
captureFrameToBuffer();               // ← No rotation
streamFrameToLcd();                   // ← Rotation during display (slow!)
```

**C. Check buffer size:**
```cpp
// DEV_Config.h should have:
#define DMA_BUFFER_SIZE 16384  // 16KB

// NOT:
#define DMA_BUFFER_SIZE 8192   // 8KB (slower)
```

---

## 📅 2-Week Demo Timeline

### Week 1: Get FPS Back + SD Card

**Monday-Tuesday: Fix Performance**
- [ ] Verify using optimized files
- [ ] Check DMA_BUFFER_SIZE = 16384
- [ ] Confirm pre-rotation is enabled
- [ ] Test: Should get 10+ FPS

**Wednesday-Thursday: Add SD Card**
- [ ] Wire SD card module (CS=7, shared SPI)
- [ ] Test SD initialization
- [ ] Implement Storage_SavePhoto()
- [ ] Test: Can save dummy file

**Friday: Refactor Code**
- [ ] Create hardware/camera.cpp (move camera code)
- [ ] Create hardware/display.cpp (wrap LCD calls)
- [ ] Create hardware/storage.cpp (SD operations)
- [ ] Test: Preview still works

### Week 2: Add Buttons + JPEG

**Monday: Add Buttons**
- [ ] Wire two buttons (GPIO 12, 13)
- [ ] Add button reading to main loop
- [ ] Test state machine (preview/capture/view)

**Tuesday-Wednesday: JPEG Encoding**
- [ ] Add esp_jpg_encode library
- [ ] Implement Camera_CaptureToJPEG()
- [ ] Test: Save JPEG to SD

**Thursday: JPEG Decoding**
- [ ] Add TJpgDec library
- [ ] Implement Display_ShowPhoto()
- [ ] Test: Can view saved photo

**Friday: Polish**
- [ ] Add text overlay (photo counter)
- [ ] Add "Saving..." notification
- [ ] Test all features
- [ ] Demo ready! 🎉

---

## 🛠️ Required Hardware (Minimal)

### Essential:
- ✅ Your current ESP32-C6, camera, LCD
- 🛒 **MicroSD card module** ($3-5)
- 🛒 **2 push buttons** ($1-2)
- 🛒 **MicroSD card** (8GB is plenty) ($5)

### Optional:
- LED for status indicator
- Breadboard and wires
- Case/enclosure

**Total cost: ~$10**

---

## 📊 Expected Demo Performance

With optimized code:
- **Preview FPS:** 10-15 (smooth!)
- **Capture time:** 500ms (button to saved)
- **View time:** 300ms (load and display)
- **Storage:** ~20-30KB per photo (JPEG)
- **Capacity:** 8GB = ~400 photos

---

## 🚀 Quick Start Commands

### Step 1: Fix Current Performance
```bash
# Make sure you're using these files:
cp DEV_Config_memory.h DEV_Config.h
cp DEV_Config_memory.cpp DEV_Config.cpp
cp Integration_v2_memory_optimized.ino camera_demo.ino
```

### Step 2: Test Current FPS
```cpp
// In loop(), add timing:
uint32_t t0 = millis();
// ... capture and display
uint32_t t1 = millis();
Serial.printf("FPS: %.1f\n", 1000.0 / (t1 - t0));

// Should see: FPS: 10-15
// If you see: FPS: 2-3, something is wrong!
```

### Step 3: Initialize SD Card
```cpp
#include <SD.h>

void setup() {
    // ... other init
    
    if (!SD.begin(PIN_SD_CS)) {
        Serial.println("SD Card failed!");
    } else {
        Serial.println("SD Card OK");
    }
}
```

---

## 📋 Checklist for Demo

### Before Demo Day:
- [ ] Preview runs at 10+ FPS
- [ ] Capture button saves photo
- [ ] Counter shows correct number
- [ ] View button shows last photo
- [ ] SD card has at least 3 saved photos
- [ ] All buttons respond quickly
- [ ] No crashes after 5 minutes

### Demo Script:
1. Show live preview (smooth motion)
2. Press capture button (LED blinks, counter increments)
3. Repeat 2-3 times
4. Press view button (shows last photo)
5. Press view again (back to preview)

**Done! Impressive demo in 30 seconds.**

---

## 🎯 What This Demo Proves

✅ **Camera works** - Live preview is smooth
✅ **Storage works** - Photos saved to SD
✅ **Display works** - Can show captured photos
✅ **Input works** - Buttons trigger actions
✅ **Integration works** - All parts work together

**This is enough to prove the concept and secure buy-in for the full project!**

---

## 🔮 After Demo: Easy Upgrades

Once demo works, add features one at a time:

### Easy Adds (1-2 hours each):
1. **Delete button** - Remove last photo
2. **Prev/Next buttons** - Browse all photos
3. **Timestamp** - Show date/time on photos
4. **Settings** - Adjust quality, resolution

### Medium Adds (1 day each):
5. **Simple gallery** - Grid of 4 thumbnails
6. **WiFi AP** - Create network for phone
7. **Web page** - View photos in browser

### Hard Adds (1 week each):
8. **RTOS** - Parallel capture and display
9. **Bluetooth** - Transfer photos to phone
10. **Video** - Record short clips

---

## 💡 Tips for Success

### 1. **Test After Every Change**
Don't add 5 features at once! Add one, test, commit.

### 2. **Keep FPS Monitoring**
Always print FPS in Serial. If it drops, you broke something.

### 3. **Start with Dummy Data**
Test SD saving with dummy JPEG before camera integration.

### 4. **Use Small Test Images**
Test photo display with a small (320×240) JPEG first.

### 5. **Comment Your Code**
You'll forget what you did. Comment liberally.

---

## 🎉 Summary

**Forget the big plan for now!**

Build a **minimal demo** in 2 weeks:
- 10+ FPS preview ✅
- Capture saves to SD ✅
- View last photo ✅
- Simple button control ✅

**Only 10 files, ~500 lines of code total.**

**First priority: Fix your 2.3 FPS issue!**
- Use memory-optimized files
- Enable pre-rotation
- Check DMA buffer size

**Then add SD card and buttons.**

**Demo ready in 2 weeks. Expand from there!**

Good luck! 🚀
