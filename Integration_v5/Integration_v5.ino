/******************************************************************************
 * ESP32-S3 WiFi Camera System with LCD UI
 * 
 * @file    WiFiCamera_Enhanced.ino
 * @brief   Professional WiFi camera system with intuitive LCD UI for camera
 *          and gallery modes, web streaming, and photo management
 * 
 * @author  Enhanced by User
 * @date    2024
 * @version 2.0
 * 
 * HARDWARE:
 * - ESP32-S3 microcontroller
 * - ArduCAM OV2640 camera module
 * - 240x320 LCD display (ST7789)
 * - SD card for photo storage
 * - RGB LED indicator
 * - 2 push buttons for user input
 * 
 * FEATURES:
 * - Real-time camera preview on LCD
 * - Gallery mode to view saved photos
 * - Web interface for remote viewing
 * - Instant and countdown capture modes
 * - Intuitive button-based navigation
 * 
 * PIN CONFIGURATION:
 * - See "Pin Definitions" section below
 * 
 * BUTTON FUNCTIONS:
 * - Button 1 (PIN 1):  Capture photo / Select in gallery
 * - Button 2 (PIN 45): Toggle camera/gallery mode / Navigate in gallery
 * 
 * DEPENDENCIES:
 * - WiFi.h - ESP32 WiFi library
 * - WebServer.h - HTTP server
 * - Wire.h - I2C communication
 * - SPI.h - SPI communication
 * - SD.h - SD card interface
 * - ArduCAM.h - Camera library
 * - TJpg_Decoder.h - JPEG decoding
 * - Custom LCD and GUI libraries (see includes)
 * 
 * CREDITS:
 * - ArduCAM library: Copyright (c) 2016 ArduCAM (www.arducam.com)
 * - Waveshare LCD drivers: Copyright (c) 2018 Waveshare
 * - Original integration: Previous version
 * 
 * LICENSE:
 * This code combines multiple open-source components. Please respect
 * the individual licenses of each component.
 * 
 ******************************************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <ArduCAM.h>
#include "memorysaver.h"

// Avoid conflicts with existing swap definitions
#ifdef swap
  #undef swap
#endif

#include <TJpg_Decoder.h>
#include "DEV_Config.h"
#include "LCD_Driver.h"
#include "GUI_Paint.h"
#include "fonts.h"

// Web interface HTML (compressed)
#include "index_html_gz.h"
#include "gallery_html_gz.h"

//=============================================================================
// CONFIGURATION
//=============================================================================

/**
 * @brief WiFi credentials
 * @note Change these to match your network
 */
const char* WIFI_SSID     = "KellyiPhone";
const char* WIFI_PASSWORD = "kelly200636";

//=============================================================================
// PIN DEFINITIONS
//=============================================================================

/**
 * @brief Hardware pin assignments for ESP32-S3
 */
namespace Pin {
  // SPI pins for SD card and camera
  constexpr int SPI_SCK  = 10;
  constexpr int SPI_MISO = 11;
  constexpr int SPI_MOSI = 12;
  constexpr int SD_CS    = 14;  ///< SD card chip select
  constexpr int CAM_CS   = 13;  ///< Camera chip select
  
  // I2C pins for camera control
  constexpr int SDA = 9;
  constexpr int SCL = 8;
  
  // User interface
  constexpr int BUTTON1 = 1;   ///< Capture/Select button
  constexpr int BUTTON2 = 45;  ///< Mode/Navigate button
  
  // Status LED (RGB common cathode)
  constexpr int LED_RED   = 2;
  constexpr int LED_GREEN = 42;
  constexpr int LED_BLUE  = 41;
}

//=============================================================================
// SYSTEM CONSTANTS
//=============================================================================

/**
 * @brief Buffer sizes and frame parameters
 */
namespace Config {
  constexpr uint32_t MAX_JPEG_SIZE = 32768;  ///< Maximum JPEG buffer (32KB)
  constexpr uint16_t FRAME_WIDTH   = 320;    ///< Camera frame width
  constexpr uint16_t FRAME_HEIGHT  = 240;    ///< Camera frame height
  constexpr uint32_t FRAME_BYTES   = FRAME_WIDTH * FRAME_HEIGHT * 2; ///< RGB565
  constexpr uint32_t DEBOUNCE_MS   = 200;    ///< Button debounce time
}

//=============================================================================
// GLOBAL VARIABLES
//=============================================================================

/**
 * @brief Memory buffers for image processing
 */
struct Buffers {
  uint8_t jpeg[Config::MAX_JPEG_SIZE];  ///< Compressed JPEG data
  uint8_t frame[Config::FRAME_BYTES];   ///< Decoded RGB565 frame
  volatile uint32_t jpegLen = 0;        ///< Current JPEG size
} buffers;

/**
 * @brief System operation modes
 */
enum class AppMode {
  CAMERA,   ///< Live camera preview and capture
  GALLERY   ///< View saved photos from SD card
};

enum class CaptureMode {
  INSTANT,    ///< Immediate photo capture
  COUNTDOWN   ///< 3-second countdown before capture
};

/**
 * @brief Application state
 */
struct AppState {
  AppMode currentMode = AppMode::CAMERA;
  CaptureMode captureMode = CaptureMode::INSTANT;
  bool sdCardAvailable = false;
  uint32_t photoCounter = 1;
  uint32_t totalPhotos = 0;
  int currentGalleryIndex = 0;
  String lastStatus = "Idle";
  
  // Button state
  volatile bool button1Pressed = false;
  volatile bool button2Pressed = false;
  volatile uint32_t lastButton1Time = 0;
  volatile uint32_t lastButton2Time = 0;
  
  // Web interface triggers
  volatile bool webCaptureRequested = false;
  volatile bool webCountdownRequested = false;
} state;

/**
 * @brief Hardware objects
 */
ArduCAM camera(OV2640, Pin::CAM_CS);
WebServer webServer(80);
TaskHandle_t cameraTaskHandle = NULL;

//=============================================================================
// FUNCTION DECLARATIONS
//=============================================================================

// Hardware initialization
void initLED();
void initSDCard();
void initCamera();
void initWiFi();

// UI rendering
void renderCameraUI();
void renderGalleryUI();
void renderStatusBar();
void renderModeIndicator();
void showMessage(const char* message, uint16_t color = WHITE);

// Camera operations
bool captureJpegToBuffer();
bool decodeJpegToRGB565();
void streamFrameToLCD(const uint8_t* frameData);
void handleCaptureInstant();
void handleCaptureCountdown();

// Gallery operations
void loadGalleryPhotoList();
void displayGalleryPhoto(int index);
void deleteCurrentPhoto();

// Photo management
bool savePhoto();
String getPhotoPath(int index);
int countPhotosInSD();

// LED control
void setLED(bool red, bool green, bool blue);
void ledBlink(bool red, bool green, bool blue, int times = 1);

// Button handlers (ISR)
void IRAM_ATTR button1ISR();
void IRAM_ATTR button2ISR();

// RTOS tasks
void cameraTask(void* parameter);

// Web handlers
void handleRoot();
void handleGallery();
void handleCapture();
void handleToggleMode();
void handleCountdownStart();
void handleStatus();
void handleStream();
void handlePhotoList();
void handlePhoto();
void handleDeletePhoto();

// JPEG decoder callback
bool tjpgOutputCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);

//=============================================================================
// SETUP
//=============================================================================

/**
 * @brief Arduino setup function - runs once on boot
 * 
 * Initializes all hardware peripherals, sensors, displays, and network
 * connections. Sets up the RTOS task structure for dual-core operation.
 */
void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=================================");
  Serial.println("WiFi Camera System Starting...");
  Serial.println("=================================\n");
  
  // Configure buttons with interrupts
  pinMode(Pin::BUTTON1, INPUT_PULLUP);
  pinMode(Pin::BUTTON2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(Pin::BUTTON1), button1ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(Pin::BUTTON2), button2ISR, FALLING);
  Serial.println("[OK] Buttons configured");
  
  // Initialize LED
  initLED();
  setLED(false, false, true); // Blue = initializing
  Serial.println("[OK] LED initialized");
  
  // Configure SPI chip selects
  pinMode(Pin::SD_CS, OUTPUT);
  pinMode(Pin::CAM_CS, OUTPUT);
  digitalWrite(Pin::SD_CS, HIGH);
  digitalWrite(Pin::CAM_CS, HIGH);
  
  // Initialize I2C and SPI buses
  Wire.begin(Pin::SDA, Pin::SCL);
  SPI.begin(Pin::SPI_SCK, Pin::SPI_MISO, Pin::SPI_MOSI);
  Serial.println("[OK] Communication buses initialized");
  
  // Initialize SD card
  initSDCard();
  
  // Initialize camera
  initCamera();
  
  // Initialize LCD display
  Config_Init();
  LCD_Init();
  LCD_SetBacklight(100);
  LCD_Clear(BLACK);
  Serial.println("[OK] LCD initialized");
  
  // Configure JPEG decoder
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(tjpgOutputCallback);
  Serial.println("[OK] JPEG decoder configured");
  
  // Connect to WiFi (non-blocking, continues even if fails)
  initWiFi();
  
  // Setup web server routes (only if WiFi connected)
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[INFO] Setting up web server...");
    webServer.on("/", handleRoot);
    webServer.on("/gallery", handleGallery);
    webServer.on("/capture", handleCapture);
    webServer.on("/toggle", handleToggleMode);
    webServer.on("/countdown_start", handleCountdownStart);
    webServer.on("/status", handleStatus);
    webServer.on("/stream", handleStream);
    webServer.on("/photos", handlePhotoList);
    webServer.on("/photo", handlePhoto);
    webServer.on("/delete", handleDeletePhoto);
    webServer.begin();
    Serial.println("[OK] Web server started");
    
    // Show WiFi ready message
    Serial.println("[INFO] Displaying welcome message...");
    LCD_Clear(BLACK);
    Serial.println("[DEBUG] LCD cleared");
    Paint_DrawString_EN(10, 150, "WiFi Camera Ready!", &Font16, BLACK, GREEN);
    Serial.println("[DEBUG] Text drawn");
    Serial.println("[INFO] Welcome message displayed");
  } else {
    // Show offline mode message
    Serial.println("[INFO] Displaying offline mode message...");
    LCD_Clear(BLACK);
    Serial.println("[DEBUG] LCD cleared");
    Paint_DrawString_EN(30, 150, "Camera Ready!", &Font16, BLACK, GREEN);
    Serial.println("[DEBUG] Text drawn");
    Serial.println("[INFO] Offline message displayed");
    Serial.println("[INFO] Running in offline mode");
  }
  
  Serial.println("[DEBUG] About to delay 1500ms...");
  delay(1500);
  Serial.println("[DEBUG] Delay complete");
  
  Serial.println("[INFO] Creating camera task...");
  xTaskCreatePinnedToCore(
    cameraTask,           // Task function
    "CameraTask",         // Task name
    8192,                 // Stack size
    NULL,                 // Parameters
    1,                    // Priority
    &cameraTaskHandle,    // Task handle
    0                     // Core 0
  );
  
  Serial.println("[OK] System ready!\n");
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi Mode - IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("Web interface available");
  } else {
    Serial.println("Offline Mode - Camera and LCD fully functional");
    Serial.println("Web interface not available");
  }
  
  setLED(false, true, false); // Green = ready
  delay(500);
  setLED(false, false, false);
}

//=============================================================================
// MAIN LOOP (Core 1)
//=============================================================================

/**
 * @brief Arduino loop function - runs continuously on Core 1
 * 
 * Handles web server requests only if WiFi is connected. 
 * The camera and LCD operations run independently on Core 0 via the cameraTask.
 */
void loop() {
  // Only process web requests if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    webServer.handleClient();
  }
  delay(1);
}

//=============================================================================
// CAMERA TASK (Core 0)
//=============================================================================

/**
 * @brief Main camera task running on Core 0
 * 
 * Handles:
 * - Button press processing
 * - Mode switching
 * - Camera capture and display
 * - Gallery navigation
 * - Web interface triggers
 * 
 * @param parameter Task parameter (unused)
 */
void cameraTask(void* parameter) {
  for (;;) {
    // Process web capture requests
    if (state.webCaptureRequested) {
      state.webCaptureRequested = false;
      if (state.captureMode == CaptureMode::INSTANT) {
        handleCaptureInstant();
      }
    }
    
    if (state.webCountdownRequested) {
      state.webCountdownRequested = false;
      if (state.captureMode == CaptureMode::COUNTDOWN) {
        handleCaptureCountdown();
      }
    }
    
    // Handle button presses
    if (state.button2Pressed) {
      state.button2Pressed = false;
      
      if (state.currentMode == AppMode::CAMERA) {
        // Toggle capture mode
        state.captureMode = (state.captureMode == CaptureMode::INSTANT) 
                          ? CaptureMode::COUNTDOWN 
                          : CaptureMode::INSTANT;
        renderModeIndicator();
        delay(500);
      } else {
        // Navigate gallery (next photo)
        state.currentGalleryIndex++;
        if (state.currentGalleryIndex >= state.totalPhotos) {
          state.currentGalleryIndex = 0;
        }
        displayGalleryPhoto(state.currentGalleryIndex);
      }
    }
    
    if (state.button1Pressed) {
      state.button1Pressed = false;
      
      if (state.currentMode == AppMode::CAMERA) {
        // Capture photo
        if (state.captureMode == CaptureMode::INSTANT) {
          handleCaptureInstant();
        } else {
          handleCaptureCountdown();
        }
      } else {
        // Toggle between camera and gallery
        state.currentMode = AppMode::CAMERA;
        renderCameraUI();
      }
    }
    
    // Long press detection for Button 2 (enter gallery from camera)
    static uint32_t button2PressStart = 0;
    if (digitalRead(Pin::BUTTON2) == LOW && state.currentMode == AppMode::CAMERA) {
      if (button2PressStart == 0) {
        button2PressStart = millis();
      } else if (millis() - button2PressStart > 1000) { // 1 second long press
        button2PressStart = 0;
        if (state.totalPhotos > 0) {
          state.currentMode = AppMode::GALLERY;
          loadGalleryPhotoList();
          displayGalleryPhoto(0);
        } else {
          showMessage("No photos!", YELLOW);
          delay(1000);
        }
      }
    } else {
      button2PressStart = 0;
    }
    
    // Camera mode: stream live preview
    if (state.currentMode == AppMode::CAMERA) {
      if (!captureJpegToBuffer()) {
        vTaskDelay(5);
        continue;
      }
      if (!decodeJpegToRGB565()) {
        vTaskDelay(5);
        continue;
      }
      
      streamFrameToLCD(buffers.frame);
      renderCameraUI();
    }
    
    vTaskDelay(1);
  }
}

//=============================================================================
// HARDWARE INITIALIZATION
//=============================================================================

/**
 * @brief Initialize RGB LED
 * 
 * Configures LED pins as outputs and turns off all colors.
 * LED is common cathode (LOW = ON, HIGH = OFF).
 */
void initLED() {
  pinMode(Pin::LED_RED, OUTPUT);
  pinMode(Pin::LED_GREEN, OUTPUT);
  pinMode(Pin::LED_BLUE, OUTPUT);
  setLED(false, false, false);
}

/**
 * @brief Initialize SD card
 * 
 * Attempts to mount SD card and counts existing photos.
 * Updates state.sdCardAvailable flag.
 */
void initSDCard() {
  digitalWrite(Pin::CAM_CS, HIGH); // Deselect camera
  
  if (!SD.begin(Pin::SD_CS)) {
    Serial.println("[WARN] SD card initialization failed!");
    state.sdCardAvailable = false;
    return;
  }
  
  state.sdCardAvailable = true;
  Serial.println("[OK] SD card mounted");
  
  // Create photos directory if it doesn't exist
  if (!SD.exists("/photos")) {
    SD.mkdir("/photos");
    Serial.println("[INFO] Created /photos directory");
  }
  
  // Count existing photos
  state.totalPhotos = countPhotosInSD();
  Serial.print("[INFO] Found ");
  Serial.print(state.totalPhotos);
  Serial.println(" existing photos");
}

/**
 * @brief Initialize ArduCAM OV2640
 * 
 * Configures camera for JPEG output at 320x240 resolution.
 * Based on ArduCAM library (www.arducam.com)
 */
void initCamera() {
  digitalWrite(Pin::SD_CS, HIGH); // Deselect SD card
  
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  camera.set_format(JPEG);
  camera.InitCAM();
  camera.OV2640_set_JPEG_size(OV2640_320x240);
  SPI.endTransaction();
  
  delay(200);
  Serial.println("[OK] Camera initialized (320x240 JPEG)");
}

/**
 * @brief Initialize WiFi connection
 * 
 * Connects to configured WiFi network and displays IP address.
 */
void initWiFi() {
  Serial.println("[INFO] Starting WiFi initialization...");
  
  // Ensure WiFi is in station mode before attempting connection
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);  // Clear any previous connection
  delay(100);
  
  Serial.print("[INFO] Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  
  // Start WiFi connection (non-blocking)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Try for only 5 seconds max (10 attempts × 500ms)
  // This ensures the LCD and camera start quickly even if WiFi fails
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println(); // New line after dots
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[OK] WiFi connected");
    Serial.print("[INFO] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WARN] WiFi connection failed - continuing in offline mode");
    Serial.println("[INFO] Camera and LCD will work normally");
    Serial.println("[INFO] Web interface will not be available");
    WiFi.mode(WIFI_OFF);  // Turn off WiFi to save power
  }
}

//=============================================================================
// UI RENDERING
//=============================================================================

/**
 * @brief Render camera mode UI overlay
 * 
 * Displays status bar and mode indicator on top of live preview.
 */
void renderCameraUI() {
  static uint32_t lastUpdate = 0;
  if (millis() - lastUpdate < 1000) return; // Update every 1 second
  lastUpdate = millis();
  
  renderStatusBar();
  renderModeIndicator();
}

/**
 * @brief Render status bar at top of screen
 * 
 * Shows WiFi status, SD card status, and photo count.
 */
void renderStatusBar() {
  // Draw semi-transparent background
  Paint_DrawFilledRectangle_Fast(0, 0, 240, 20, 0x0000); // Black bar
  
  // WiFi status
  if (WiFi.status() == WL_CONNECTED) {
    Paint_DrawString_EN(5, 2, "WiFi", &Font12, BLACK, GREEN);
  } else {
    Paint_DrawString_EN(5, 2, "WiFi", &Font12, BLACK, RED);
  }
  
  // SD card status
  if (state.sdCardAvailable) {
    Paint_DrawString_EN(50, 2, "SD", &Font12, BLACK, GREEN);
  } else {
    Paint_DrawString_EN(50, 2, "SD", &Font12, BLACK, RED);
  }
  
  // Photo count
  char countStr[16];
  snprintf(countStr, sizeof(countStr), "%d", state.totalPhotos);
  Paint_DrawString_EN(180, 2, countStr, &Font12, BLACK, WHITE);
}

/**
 * @brief Render mode indicator at bottom of screen
 * 
 * Shows current capture mode (Instant/Countdown).
 */
void renderModeIndicator() {
  // Draw background
  Paint_DrawFilledRectangle_Fast(0, 300, 240, 320, 0x0000);
  
  // Mode text
  const char* modeText = (state.captureMode == CaptureMode::INSTANT) 
                         ? "INSTANT" : "COUNTDOWN";
  uint16_t color = (state.captureMode == CaptureMode::INSTANT) 
                   ? CYAN : YELLOW;
  
  Paint_DrawString_EN(80, 302, modeText, &Font16, BLACK, color);
}

/**
 * @brief Render gallery mode UI
 * 
 * Shows current photo with navigation info.
 */
void renderGalleryUI() {
  // Display current photo number
  char infoStr[32];
  snprintf(infoStr, sizeof(infoStr), "%d / %d", 
           state.currentGalleryIndex + 1, state.totalPhotos);
  
  Paint_DrawFilledRectangle_Fast(0, 0, 240, 20, 0x0000);
  Paint_DrawString_EN(80, 2, infoStr, &Font16, BLACK, CYAN);
  
  // Controls info at bottom
  Paint_DrawFilledRectangle_Fast(0, 300, 240, 320, 0x0000);
  Paint_DrawString_EN(20, 302, "BTN1:Back BTN2:Next", &Font12, BLACK, WHITE);
}

/**
 * @brief Show full-screen message
 * 
 * @param message Text to display
 * @param color Text color (default: WHITE)
 */
void showMessage(const char* message, uint16_t color) {
  LCD_Clear(BLACK);
  int len = strlen(message);
  int x = (240 - len * 12) / 2; // Center text (Font16 width ≈ 12px)
  Paint_DrawString_EN(x, 150, message, &Font16, BLACK, color);
}

//=============================================================================
// CAMERA OPERATIONS
//=============================================================================

/**
 * @brief Capture JPEG image from camera to buffer
 * 
 * Triggers camera capture and reads JPEG data into buffers.jpeg.
 * Based on ArduCAM library (www.arducam.com)
 * 
 * @return true if capture successful, false otherwise
 */
bool captureJpegToBuffer() {
  digitalWrite(Pin::SD_CS, HIGH); // Deselect SD card
  
  // Start capture
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  camera.flush_fifo();
  camera.clear_fifo_flag();
  camera.start_capture();
  SPI.endTransaction();
  
  // Wait for capture done (timeout 5 seconds)
  uint32_t startTime = millis();
  while (millis() - startTime < 5000) {
    SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    bool done = camera.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK);
    SPI.endTransaction();
    
    if (done) break;
    delay(5);
  }
  
  // Read FIFO length
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  uint32_t len = camera.read_fifo_length();
  SPI.endTransaction();
  
  if (len == 0 || len > Config::MAX_JPEG_SIZE) {
    Serial.println("[ERROR] Invalid JPEG length");
    return false;
  }
  
  // Read JPEG data
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
  camera.CS_LOW();
  camera.set_fifo_burst();
  for (uint32_t i = 0; i < len; i++) {
    buffers.jpeg[i] = SPI.transfer(0x00);
  }
  camera.CS_HIGH();
  SPI.endTransaction();
  
  buffers.jpegLen = len;
  
  // Validate JPEG header
  if (buffers.jpeg[0] != 0xFF || buffers.jpeg[1] != 0xD8) {
    Serial.println("[ERROR] Invalid JPEG header");
    return false;
  }
  
  return true;
}

/**
 * @brief Decode JPEG to RGB565 frame buffer
 * 
 * Uses TJpg_Decoder library to decompress JPEG into buffers.frame.
 * 
 * @return true if decode successful, false otherwise
 */
bool decodeJpegToRGB565() {
  memset(buffers.frame, 0, Config::FRAME_BYTES);
  
  uint16_t w, h;
  if (TJpgDec.getJpgSize(&w, &h, buffers.jpeg, buffers.jpegLen) != JDR_OK) {
    return false;
  }
  
  return TJpgDec.drawJpg(0, 0, buffers.jpeg, buffers.jpegLen) == JDR_OK;
}

/**
 * @brief Stream RGB565 frame to LCD with rotation
 * 
 * Optimized DMA transfer with 90-degree rotation for portrait display.
 * Based on Waveshare LCD drivers.
 * 
 * @param frameData Pointer to RGB565 frame buffer
 */
void streamFrameToLCD(const uint8_t* frameData) {
  uint8_t* dmaBuf = getDMABuffer();
  const uint16_t LCD_W = 240;
  const uint16_t LCD_H = 320;
  
  LCD_SetCursor(0, 0, LCD_W - 1, LCD_H - 1);
  DEV_SPI_Write_Bulk_Start();
  
  // Rotate 90 degrees: LCD[y][x] = Frame[240-x][y]
  for (uint16_t y = 0; y < LCD_H; y++) {
    uint16_t ptr = 0;
    
    for (uint16_t x = 0; x < LCD_W; x++) {
      uint16_t srcRow = LCD_W - 1 - x;
      uint16_t srcCol = y;
      uint32_t idx = (srcRow * Config::FRAME_WIDTH + srcCol) * 2;
      
      dmaBuf[ptr++] = frameData[idx];
      dmaBuf[ptr++] = frameData[idx + 1];
      
      if (ptr >= DMA_BUFFER_SIZE) {
        DEV_SPI_Write_Bulk_Data(dmaBuf, ptr);
        ptr = 0;
      }
    }
    
    if (ptr > 0) {
      DEV_SPI_Write_Bulk_Data(dmaBuf, ptr);
    }
  }
  
  DEV_SPI_Write_Bulk_End();
}

/**
 * @brief Handle instant photo capture
 * 
 * Captures photo immediately and saves to SD card.
 */
void handleCaptureInstant() {
  state.lastStatus = "Capturing...";
  setLED(true, false, false); // Red LED
  
  if (state.sdCardAvailable) {
    showMessage("SAVING...", YELLOW);
    bool success = savePhoto();
    
    if (success) {
      state.lastStatus = "Saved!";
      ledBlink(false, true, false, 2); // Green blink
      showMessage("Photo Saved!", GREEN);
    } else {
      state.lastStatus = "Failed";
      ledBlink(true, false, false, 2); // Red blink
      showMessage("Save Failed!", RED);
    }
    
    delay(1000);
  } else {
    showMessage("No SD Card!", RED);
    delay(1000);
  }
  
  setLED(false, false, false);
  state.lastStatus = "Idle";
}

/**
 * @brief Handle countdown photo capture
 * 
 * Shows 3-second countdown with LED blinks, then captures.
 */
void handleCaptureCountdown() {
  state.lastStatus = "Countdown...";
  
  // 3-second countdown
  for (int count = 3; count >= 1; count--) {
    char countStr[8];
    snprintf(countStr, sizeof(countStr), "%d", count);
    showMessage(countStr, YELLOW);
    
    ledBlink(true, false, false, 1);
    delay(1000);
  }
  
  // Capture
  captureJpegToBuffer();
  decodeJpegToRGB565();
  streamFrameToLCD(buffers.frame);
  
  // Save
  if (state.sdCardAvailable) {
    setLED(false, true, false);
    showMessage("SAVING...", YELLOW);
    
    bool success = savePhoto();
    if (success) {
      showMessage("Photo Saved!", GREEN);
    } else {
      showMessage("Save Failed!", RED);
    }
    
    delay(1000);
  }
  
  setLED(false, false, false);
  state.lastStatus = "Idle";
}

//=============================================================================
// GALLERY OPERATIONS
//=============================================================================

/**
 * @brief Load list of photos from SD card
 * 
 * Updates state.totalPhotos count.
 */
void loadGalleryPhotoList() {
  state.totalPhotos = countPhotosInSD();
  state.currentGalleryIndex = 0;
}

/**
 * @brief Display photo from gallery
 * 
 * @param index Photo index (0-based)
 */
void displayGalleryPhoto(int index) {
  if (!state.sdCardAvailable || state.totalPhotos == 0) {
    showMessage("No photos!", YELLOW);
    return;
  }
  
  if (index < 0 || index >= state.totalPhotos) {
    return;
  }
  
  String photoPath = getPhotoPath(index);
  
  if (!SD.exists(photoPath)) {
    showMessage("Photo not found!", RED);
    delay(1000);
    return;
  }
  
  // Read JPEG file
  File file = SD.open(photoPath, FILE_READ);
  if (!file) {
    showMessage("Cannot open file!", RED);
    delay(1000);
    return;
  }
  
  size_t fileSize = file.size();
  if (fileSize > Config::MAX_JPEG_SIZE) {
    showMessage("File too large!", RED);
    file.close();
    delay(1000);
    return;
  }
  
  file.read(buffers.jpeg, fileSize);
  file.close();
  buffers.jpegLen = fileSize;
  
  // Decode and display
  if (decodeJpegToRGB565()) {
    streamFrameToLCD(buffers.frame);
    renderGalleryUI();
  } else {
    showMessage("Decode error!", RED);
    delay(1000);
  }
}

/**
 * @brief Delete currently displayed photo
 */
void deleteCurrentPhoto() {
  if (!state.sdCardAvailable || state.totalPhotos == 0) {
    return;
  }
  
  String photoPath = getPhotoPath(state.currentGalleryIndex);
  
  if (SD.remove(photoPath)) {
    state.totalPhotos--;
    showMessage("Deleted!", GREEN);
    
    if (state.totalPhotos == 0) {
      state.currentMode = AppMode::CAMERA;
    } else {
      if (state.currentGalleryIndex >= state.totalPhotos) {
        state.currentGalleryIndex = state.totalPhotos - 1;
      }
      displayGalleryPhoto(state.currentGalleryIndex);
    }
  } else {
    showMessage("Delete failed!", RED);
  }
  
  delay(1000);
}

//=============================================================================
// PHOTO MANAGEMENT
//=============================================================================

/**
 * @brief Save current JPEG buffer to SD card
 * 
 * @return true if save successful, false otherwise
 */
bool savePhoto() {
  if (!state.sdCardAvailable) return false;
  
  digitalWrite(Pin::CAM_CS, HIGH); // Deselect camera
  
  // Ensure photos directory exists
  if (!SD.exists("/photos")) {
    SD.mkdir("/photos");
  }
  
  // Generate filename
  char filename[32];
  snprintf(filename, sizeof(filename), "/photos/photo_%d.jpg", state.photoCounter++);
  
  // Write file
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("[ERROR] Cannot create file");
    return false;
  }
  
  size_t written = file.write(buffers.jpeg, buffers.jpegLen);
  file.close();
  
  if (written == buffers.jpegLen) {
    state.totalPhotos++;
    Serial.print("[INFO] Photo saved: ");
    Serial.println(filename);
    return true;
  }
  
  return false;
}

/**
 * @brief Get path to photo by index
 * 
 * @param index Photo index (0-based)
 * @return Full path to photo file
 */
String getPhotoPath(int index) {
  char filename[32];
  snprintf(filename, sizeof(filename), "/photos/photo_%d.jpg", index + 1);
  return String(filename);
}

/**
 * @brief Count total photos in SD card
 * 
 * @return Number of photos in /photos directory
 */
int countPhotosInSD() {
  if (!state.sdCardAvailable) return 0;
  
  int count = 0;
  File root = SD.open("/photos");
  if (!root) return 0;
  
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String name = file.name();
      if (name.endsWith(".jpg") || name.endsWith(".JPG")) {
        count++;
      }
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  
  return count;
}

//=============================================================================
// LED CONTROL
//=============================================================================

/**
 * @brief Set LED state
 * 
 * @param red Red LED state
 * @param green Green LED state
 * @param blue Blue LED state
 * 
 * @note LED is common cathode: LOW = ON, HIGH = OFF
 */
void setLED(bool red, bool green, bool blue) {
  digitalWrite(Pin::LED_RED,   red   ? LOW : HIGH);
  digitalWrite(Pin::LED_GREEN, green ? LOW : HIGH);
  digitalWrite(Pin::LED_BLUE,  blue  ? LOW : HIGH);
}

/**
 * @brief Blink LED
 * 
 * @param red Red LED state
 * @param green Green LED state
 * @param blue Blue LED state
 * @param times Number of blinks (default: 1)
 */
void ledBlink(bool red, bool green, bool blue, int times) {
  for (int i = 0; i < times; i++) {
    setLED(red, green, blue);
    delay(150);
    setLED(false, false, false);
    if (i < times - 1) delay(150);
  }
}

//=============================================================================
// BUTTON HANDLERS (ISR)
//=============================================================================

/**
 * @brief Button 1 interrupt service routine
 * 
 * Handles capture/select button with debouncing.
 */
void IRAM_ATTR button1ISR() {
  uint32_t now = millis();
  if (now - state.lastButton1Time > Config::DEBOUNCE_MS) {
    state.button1Pressed = true;
    state.lastButton1Time = now;
  }
}

/**
 * @brief Button 2 interrupt service routine
 * 
 * Handles mode/navigate button with debouncing.
 */
void IRAM_ATTR button2ISR() {
  uint32_t now = millis();
  if (now - state.lastButton2Time > Config::DEBOUNCE_MS) {
    state.button2Pressed = true;
    state.lastButton2Time = now;
  }
}

//=============================================================================
// WEB HANDLERS
//=============================================================================

/**
 * @brief Serve main web interface
 * 
 * Serves compressed HTML for camera control page.
 */
void handleRoot() {
  webServer.sendHeader("Content-Encoding", "gzip");
  webServer.send_P(200, "text/html",
    (const char*)index_html_gz,
    index_html_gz_len
  );
}

/**
 * @brief Serve gallery web interface
 * 
 * Serves compressed HTML for photo gallery page.
 */
void handleGallery() {
  webServer.sendHeader("Content-Encoding", "gzip");
  webServer.send_P(200, "text/html",
    (const char*)gallery_html_gz,
    gallery_html_gz_len
  );
}

/**
 * @brief Handle web capture request
 */
void handleCapture() {
  state.webCaptureRequested = true;
  webServer.send(200, "text/plain", "OK");
}

/**
 * @brief Handle mode toggle request
 */
void handleToggleMode() {
  state.button2Pressed = true;
  webServer.send(200, "text/plain", "OK");
}

/**
 * @brief Handle countdown start request
 */
void handleCountdownStart() {
  state.webCountdownRequested = true;
  webServer.send(200, "text/plain", "OK");
}

/**
 * @brief Handle status request
 * 
 * Returns JSON with current system state.
 */
void handleStatus() {
  const char* mode = (state.captureMode == CaptureMode::INSTANT) 
                     ? "Instant" : "Countdown";
  
  String json = "{";
  json += "\"mode\":\"" + String(mode) + "\",";
  json += "\"status\":\"" + state.lastStatus + "\",";
  json += "\"photos\":" + String(state.totalPhotos);
  json += "}";
  
  webServer.send(200, "application/json", json);
}

/**
 * @brief Handle MJPEG stream request
 * 
 * Serves current JPEG frame for web streaming.
 */
void handleStream() {
  if (buffers.jpegLen == 0 || buffers.jpegLen > Config::MAX_JPEG_SIZE) {
    webServer.send(503, "text/plain", "No frame available");
    return;
  }
  
  webServer.sendHeader("Cache-Control", "no-cache");
  webServer.send_P(200, "image/jpeg", 
    (const char*)buffers.jpeg, 
    buffers.jpegLen
  );
}

/**
 * @brief Handle photo list request
 * 
 * Returns JSON array of photo filenames.
 */
void handlePhotoList() {
  if (!state.sdCardAvailable) {
    webServer.send(200, "application/json", "{\"photos\":[]}");
    return;
  }
  
  String json = "{\"photos\":[";
  File root = SD.open("/photos");
  bool first = true;
  
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String name = file.name();
      if (name.endsWith(".jpg") || name.endsWith(".JPG")) {
        if (!first) json += ",";
        json += "\"" + name + "\"";
        first = false;
      }
    }
    file.close();
    file = root.openNextFile();
  }
  
  json += "]}";
  webServer.send(200, "application/json", json);
}

/**
 * @brief Handle photo download request
 * 
 * Streams requested photo file to client.
 */
void handlePhoto() {
  if (!webServer.hasArg("file")) {
    webServer.send(400, "text/plain", "Missing file parameter");
    return;
  }
  
  String path = "/photos/" + webServer.arg("file");
  
  if (!SD.exists(path)) {
    webServer.send(404, "text/plain", "Photo not found");
    return;
  }
  
  File file = SD.open(path, FILE_READ);
  webServer.streamFile(file, "image/jpeg");
  file.close();
}

/**
 * @brief Handle photo deletion request
 * 
 * Deletes specified photo from SD card.
 */
void handleDeletePhoto() {
  if (!webServer.hasArg("file")) {
    webServer.send(400, "text/plain", "Missing file parameter");
    return;
  }
  
  String path = "/photos/" + webServer.arg("file");
  
  if (SD.remove(path)) {
    if (state.totalPhotos > 0) {
      state.totalPhotos--;
    }
    webServer.send(200, "text/plain", "Deleted successfully");
  } else {
    webServer.send(500, "text/plain", "Delete failed");
  }
}

//=============================================================================
// JPEG DECODER CALLBACK
//=============================================================================

/**
 * @brief TJpg_Decoder output callback
 * 
 * Called by JPEG decoder to write decoded pixels to frame buffer.
 * 
 * @param x X coordinate of block
 * @param y Y coordinate of block
 * @param w Width of block
 * @param h Height of block
 * @param bitmap Pointer to RGB565 pixel data
 * @return true to continue decoding
 */
bool tjpgOutputCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, 
                        uint16_t* bitmap) {
  for (uint16_t row = 0; row < h; row++) {
    if ((y + row) >= Config::FRAME_HEIGHT) break;
    
    for (uint16_t col = 0; col < w; col++) {
      if ((x + col) >= Config::FRAME_WIDTH) break;
      
      uint32_t dstIdx = ((y + row) * Config::FRAME_WIDTH + (x + col)) * 2;
      uint16_t pixel = bitmap[row * w + col];
      
      // Store in big-endian format
      buffers.frame[dstIdx] = pixel >> 8;
      buffers.frame[dstIdx + 1] = pixel & 0xFF;
    }
  }
  
  return true;
}

//=============================================================================
// END OF FILE
//=============================================================================
