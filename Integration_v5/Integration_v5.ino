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
 * - Button 1 (PIN 1):  
 *     Camera Mode: Capture photo (instant or countdown based on mode)
 *     Gallery Mode: Previous photo | Long press: Exit to camera
 * - Button 2 (PIN 45): 
 *     Camera Mode: Toggle instant/countdown mode | Long press: Enter gallery
 *     Gallery Mode: Next photo
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
  
  // Photo saving flag (prevents camera task from capturing while saving)
  volatile bool isSaving = false;
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
  Serial.println("[OK] LCD initialized");
  
  // Initialize Paint canvas to match full LCD size (240x320)
  // This is needed for any Paint operations (though we minimize their use)
  Paint_NewImage(240, 320, 0, BLACK);
  Paint_SetRotate(ROTATE_0);
  Serial.println("[OK] Paint canvas initialized (240x320)");
  
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
    
    // Show WiFi ready message using DMA (not Paint)
    Serial.println("[INFO] Displaying welcome message with DMA...");
    
    uint8_t* dmaBuf = getDMABuffer();
    
    // Clear screen to black
    for (int i = 0; i < DMA_BUFFER_SIZE; i++) {
      dmaBuf[i] = 0x00;
    }
    
    LCD_SetCursor(0, 0, 239, 319);
    DEV_SPI_Write_Bulk_Start();
    int totalBytes = 240 * 320 * 2;
    int bytesWritten = 0;
    while (bytesWritten < totalBytes) {
      int chunk = (totalBytes - bytesWritten > DMA_BUFFER_SIZE) ? DMA_BUFFER_SIZE : (totalBytes - bytesWritten);
      DEV_SPI_Write_Bulk_Data(dmaBuf, chunk);
      bytesWritten += chunk;
    }
    DEV_SPI_Write_Bulk_End();
    
    // Draw a simple green block in center as "WiFi Ready" indicator
    LCD_SetCursor(80, 140, 159, 179);
    // Fill buffer with green
    for (int i = 0; i < 80 * 40 * 2 && i < DMA_BUFFER_SIZE; i += 2) {
      dmaBuf[i] = (GREEN >> 8) & 0xFF;
      dmaBuf[i + 1] = GREEN & 0xFF;
    }
    DEV_SPI_Write_Bulk_Start();
    DEV_SPI_Write_Bulk_Data(dmaBuf, 80 * 40 * 2);
    DEV_SPI_Write_Bulk_End();
    
    Serial.println("[INFO] Welcome indicator displayed");
  } else {
    // Show offline mode message using DMA (not Paint)
    Serial.println("[INFO] Displaying offline mode message with DMA...");
    
    uint8_t* dmaBuf = getDMABuffer();
    
    // Clear screen to black
    for (int i = 0; i < DMA_BUFFER_SIZE; i++) {
      dmaBuf[i] = 0x00;
    }
    
    LCD_SetCursor(0, 0, 239, 319);
    DEV_SPI_Write_Bulk_Start();
    int totalBytes = 240 * 320 * 2;
    int bytesWritten = 0;
    while (bytesWritten < totalBytes) {
      int chunk = (totalBytes - bytesWritten > DMA_BUFFER_SIZE) ? DMA_BUFFER_SIZE : (totalBytes - bytesWritten);
      DEV_SPI_Write_Bulk_Data(dmaBuf, chunk);
      bytesWritten += chunk;
    }
    DEV_SPI_Write_Bulk_End();
    
    // Draw a simple yellow block in center as "Offline Ready" indicator
    LCD_SetCursor(80, 140, 159, 179);
    // Fill buffer with yellow
    for (int i = 0; i < 80 * 40 * 2 && i < DMA_BUFFER_SIZE; i += 2) {
      dmaBuf[i] = (YELLOW >> 8) & 0xFF;
      dmaBuf[i + 1] = YELLOW & 0xFF;
    }
    DEV_SPI_Write_Bulk_Start();
    DEV_SPI_Write_Bulk_Data(dmaBuf, 80 * 40 * 2);
    DEV_SPI_Write_Bulk_End();
    
    Serial.println("[INFO] Offline indicator displayed");
    Serial.println("[INFO] Running in offline mode");
  }
  
  Serial.println("[DEBUG] About to delay 2000ms...");
  delay(2000);  // Show the indicator for 2 seconds
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
  Serial.println("[TASK] Camera task started on Core 0");
  
  // Clear the welcome message from startup using DMA fill
  Serial.println("[TASK] Clearing startup message...");
  
  uint8_t* dmaBuf = getDMABuffer();
  // Fill DMA buffer with black
  for (int i = 0; i < DMA_BUFFER_SIZE; i++) {
    dmaBuf[i] = 0x00;
  }
  
  // Fill entire LCD with black using DMA
  LCD_SetCursor(0, 0, 239, 319);
  DEV_SPI_Write_Bulk_Start();
  
  // Total pixels: 240 * 320 = 76,800 pixels = 153,600 bytes
  int totalBytes = 240 * 320 * 2;
  int bytesWritten = 0;
  while (bytesWritten < totalBytes) {
    int chunk = (totalBytes - bytesWritten > DMA_BUFFER_SIZE) ? DMA_BUFFER_SIZE : (totalBytes - bytesWritten);
    DEV_SPI_Write_Bulk_Data(dmaBuf, chunk);
    bytesWritten += chunk;
  }
  
  DEV_SPI_Write_Bulk_End();
  Serial.println("[TASK] Screen cleared with DMA");
  
  delay(500); // Give system time to settle
  
  Serial.println("[TASK] Beginning camera loop...");
  int frameCount = 0;
  
  // Countdown state (non-blocking)
  int countdownValue = 0;
  uint32_t countdownLastBlink = 0;
  bool countdownInProgress = false;
  
  for (;;) {
    // Process web capture requests (only in camera mode)
    if (state.webCaptureRequested) {
      state.webCaptureRequested = false;
      if (state.currentMode == AppMode::CAMERA && state.captureMode == CaptureMode::INSTANT) {
        handleCaptureInstant();
      }
    }
    
    // Web countdown requests trigger the inline countdown (only in camera mode)
    if (state.webCountdownRequested) {
      state.webCountdownRequested = false;
      if (state.currentMode == AppMode::CAMERA) {
        // Start inline countdown
        countdownInProgress = true;
        countdownValue = 3;
        countdownLastBlink = millis();
        state.lastStatus = "Countdown...";
        ledBlink(true, false, false, 1);
        Serial.println("[COUNTDOWN] Started via web - 3");
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
        // No need to call renderModeIndicator() - the UI updates automatically in drawUIOntoFrame()
        delay(200); // Brief delay for debouncing
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
          // Start countdown (non-blocking)
          countdownInProgress = true;
          countdownValue = 3;
          countdownLastBlink = millis();
          state.lastStatus = "Countdown...";
          ledBlink(true, false, false, 1);
          Serial.println("[COUNTDOWN] Started - 3");
        }
      } else {
        // In gallery: Navigate to previous photo
        Serial.println("[GALLERY] Button 1 - Previous photo");
        if (state.totalPhotos > 0) {
          state.currentGalleryIndex--;
          if (state.currentGalleryIndex < 0) {
            state.currentGalleryIndex = state.totalPhotos - 1;
          }
          displayGalleryPhoto(state.currentGalleryIndex);
        }
      }
    }
    
    // Long press detection for Button 1 (exit gallery to camera)
    static uint32_t button1PressStart = 0;
    if (digitalRead(Pin::BUTTON1) == LOW && state.currentMode == AppMode::GALLERY) {
      if (button1PressStart == 0) {
        button1PressStart = millis();
      } else if (millis() - button1PressStart > 1000) { // 1 second long press
        button1PressStart = 0;
        Serial.println("[GALLERY] Long press Button 1 - Exiting to camera mode");
        // LED feedback for mode change
        ledBlink(false, true, false, 2); // Green blinks = exiting to camera
        state.currentMode = AppMode::CAMERA;
      }
    } else {
      button1PressStart = 0;
    }
    
    // Process countdown (non-blocking - ONLY in camera mode)
    if (countdownInProgress && state.currentMode == AppMode::CAMERA && millis() - countdownLastBlink >= 1000) {
      countdownValue--;
      countdownLastBlink = millis();
      
      if (countdownValue > 0) {
        ledBlink(true, false, false, 1);
        Serial.print("[COUNTDOWN] ");
        Serial.println(countdownValue);
      } else {
        // Countdown complete - capture!
        Serial.println("[COUNTDOWN] Complete - capturing!");
        countdownInProgress = false;
        delay(100); // Brief stabilization
        
        if (state.sdCardAvailable) {
          setLED(false, true, false);
          bool success = savePhoto();
          if (success) {
            state.lastStatus = "Saved!";
            ledBlink(false, true, false, 2);
          } else {
            state.lastStatus = "Failed";
            ledBlink(true, false, false, 2);
          }
          delay(500);
        } else {
          ledBlink(true, false, false, 3);
          delay(500);
        }
        
        setLED(false, false, false);
        state.lastStatus = "Idle";
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
          // Cancel any active countdown when entering gallery
          if (countdownInProgress) {
            Serial.println("[COUNTDOWN] Cancelled - entering gallery mode");
            countdownInProgress = false;
            setLED(false, false, false);
          }
          
          Serial.println("[GALLERY] Long press Button 2 - Entering gallery mode");
          // LED feedback for mode change
          ledBlink(false, false, true, 2); // Blue blinks = entering gallery
          
          state.currentMode = AppMode::GALLERY;
          loadGalleryPhotoList();
          displayGalleryPhoto(0);
        } else {
          // No photos - indicate with LED blink instead of screen message
          ledBlink(true, true, false, 3); // Yellow blinks = no photos
        }
      }
    } else {
      button2PressStart = 0;
    }
    
    // Camera mode: stream live preview
    if (state.currentMode == AppMode::CAMERA) {
      // Skip frame capture if we're currently saving a photo
      if (state.isSaving) {
        if (frameCount % 30 == 0) {
          Serial.println("[TASK] Paused - waiting for save to complete...");
        }
        vTaskDelay(10);
        continue;
      }
      
      // Debug output every 30 frames
      if (frameCount % 30 == 0) {
        Serial.print("[TASK] Capturing frame ");
        Serial.println(frameCount);
      }
      
      if (!captureJpegToBuffer()) {
        if (frameCount % 30 == 0) {
          Serial.println("[ERROR] Failed to capture JPEG");
        }
        vTaskDelay(5);
        continue;
      }
      
      if (frameCount % 30 == 0) {
        Serial.println("[TASK] JPEG captured, decoding...");
      }
      
      if (!decodeJpegToRGB565()) {
        if (frameCount % 30 == 0) {
          Serial.println("[ERROR] Failed to decode JPEG");
        }
        vTaskDelay(5);
        continue;
      }
      
      if (frameCount % 30 == 0) {
        Serial.println("[TASK] Decoded, drawing UI onto frame...");
      }
      
      // Draw UI elements directly onto the frame buffer
      drawUIOntoFrame(buffers.frame);
      
      if (frameCount % 30 == 0) {
        Serial.println("[TASK] UI drawn, streaming complete frame to LCD...");
      }
      
      // Stream the complete frame (with UI) to LCD using DMA
      streamFrameToLCD(buffers.frame);
      
      if (frameCount % 30 == 0) {
        Serial.println("[TASK] Frame with UI streamed to LCD");
      }
      
      frameCount++;
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
 * @brief Draw gallery UI overlay directly onto frame buffer
 * 
 * Draws photo counter and navigation help into the RGB565 frame buffer.
 * Uses same rotation technique as camera UI to display horizontally on LCD.
 * 
 * @param frameData Pointer to RGB565 frame buffer (320x240)
 */
void drawGalleryUIOntoFrame(uint8_t* frameData) {
  // Helper to set a pixel in RGB565 format
  auto setPixel = [&](int x, int y, uint16_t color) {
    if (x < 0 || x >= 320 || y < 0 || y >= 240) return;
    uint32_t idx = (y * 320 + x) * 2;
    frameData[idx] = color >> 8;
    frameData[idx + 1] = color & 0xFF;
  };
  
  // Helper to fill a rectangle
  auto fillRect = [&](int x1, int y1, int x2, int y2, uint16_t color) {
    for (int y = y1; y < y2; y++) {
      for (int x = x1; x < x2; x++) {
        setPixel(x, y, color);
      }
    }
  };
  
  // 5x7 font (same as camera mode)
  auto drawChar = [&](int x, int y, char c, uint16_t color) {
    const uint8_t font5x7[][7] = {
      {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
      {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
      {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
      {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
      {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
      {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
      {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
      {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
      {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
      {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
      {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
      {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
      {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
      {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
      {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
      {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
      {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
      {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
      {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
      {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
      {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
      {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
      {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
      {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
      {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
      {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
      {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
      {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
      {0x46, 0x49, 0x49, 0x49, 0x31}, // S
      {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
      {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
      {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
      {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
      {0x63, 0x14, 0x08, 0x14, 0x63}, // X
      {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
      {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    };
    
    int charIndex = -1;
    if (c >= '0' && c <= '9') charIndex = c - '0';
    else if (c >= 'A' && c <= 'Z') charIndex = 10 + (c - 'A');
    else if (c >= 'a' && c <= 'z') charIndex = 10 + (c - 'a');
    else if (c == ' ') return;
    else if (c == '/') {
      // Draw slash manually (rotated)
      for (int i = 0; i < 7; i++) {
        setPixel(x + i, y + (6 - i), color);
      }
      return;
    }
    
    if (charIndex >= 0 && charIndex < 36) {
      // Draw character rotated 90° CW
      for (int col = 0; col < 5; col++) {
        uint8_t line = font5x7[charIndex][col];
        for (int row = 0; row < 7; row++) {
          if (line & (1 << row)) {
            setPixel(x + (6 - row), y + col, color);
          }
        }
      }
    }
  };
  
  // Draw string with proper spacing
  auto drawString = [&](int x, int y, const char* str, uint16_t color) {
    int offset = 0;
    while (*str) {
      if (*str == '/') {
        drawChar(x, y + offset, *str, color);
        offset += 8; // Slash is wider
      } else {
        drawChar(x, y + offset, *str, color);
        offset += 6; // 5 pixels width + 1 space
      }
      str++;
    }
  };
  
  // ========== TOP BAR: Photo Counter ==========
  fillRect(0, 0, 25, 240, BLACK);
  
  char countStr[32];
  snprintf(countStr, sizeof(countStr), "%d/%d", 
           state.currentGalleryIndex + 1, state.totalPhotos);
  
  // Center the counter
  int textLen = strlen(countStr) * 6;
  int startY = (240 - textLen) / 2;
  drawString(8, startY, countStr, CYAN);
  
  // ========== BOTTOM BAR: Navigation Help ==========
  fillRect(295, 0, 320, 240, BLACK);
  
  // Simple arrows or text
  drawString(303, 80, "PREV", WHITE);
  drawString(303, 140, "NEXT", WHITE);
}

/**
 * @brief Draw UI overlay directly onto frame buffer
 * 
 * Draws status bar and mode indicator into the RGB565 frame buffer.
 * 
 * Frame is 320x240, LCD is 240x320 (portrait).
 * Rotation: LCD[y][x] = Frame[240-x][y]
 * 
 * To make text appear horizontal on LCD, we write it rotated in the frame buffer.
 * 
 * @param frameData Pointer to RGB565 frame buffer (320x240)
 */
void drawUIOntoFrame(uint8_t* frameData) {
  // Helper to set a pixel in RGB565 format
  auto setPixel = [&](int x, int y, uint16_t color) {
    if (x < 0 || x >= 320 || y < 0 || y >= 240) return;
    uint32_t idx = (y * 320 + x) * 2;
    frameData[idx] = color >> 8;
    frameData[idx + 1] = color & 0xFF;
  };
  
  // Helper to fill a rectangle
  auto fillRect = [&](int x1, int y1, int x2, int y2, uint16_t color) {
    for (int y = y1; y < y2; y++) {
      for (int x = x1; x < x2; x++) {
        setPixel(x, y, color);
      }
    }
  };
  
  // 5x7 font (standard, will be rotated for display)
  auto drawChar = [&](int x, int y, char c, uint16_t color) {
    const uint8_t font5x7[][7] = {
      {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
      {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
      {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
      {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
      {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
      {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
      {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
      {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
      {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
      {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
      {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
      {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
      {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
      {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
      {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
      {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
      {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
      {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
      {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
      {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
      {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
      {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
      {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
      {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
      {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
      {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
      {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
      {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
      {0x46, 0x49, 0x49, 0x49, 0x31}, // S
      {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
      {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
      {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
      {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
      {0x63, 0x14, 0x08, 0x14, 0x63}, // X
      {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
      {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    };
    
    int charIndex = -1;
    if (c >= '0' && c <= '9') charIndex = c - '0';
    else if (c >= 'A' && c <= 'Z') charIndex = 10 + (c - 'A');
    else if (c >= 'a' && c <= 'z') charIndex = 10 + (c - 'a');
    else if (c == ' ') return;
    
    if (charIndex >= 0 && charIndex < 36) {
      // Draw character rotated 90° CW so it appears horizontal on LCD
      // Original font: 5 cols x 7 rows
      // After rotation: 7 cols x 5 rows
      for (int col = 0; col < 5; col++) {
        uint8_t line = font5x7[charIndex][col];
        for (int row = 0; row < 7; row++) {
          if (line & (1 << row)) {
            // Rotate 90° CW: (col, row) -> (6-row, col)
            setPixel(x + (6 - row), y + col, color);
          }
        }
      }
    }
  };
  
  // Draw string with proper spacing
  auto drawString = [&](int x, int y, const char* str, uint16_t color) {
    int offset = 0;
    while (*str) {
      drawChar(x, y + offset, *str, color);
      offset += 6; // 5 pixels width + 1 space
      str++;
    }
  };
  
  // ========== STATUS BAR (LEFT SIDE OF FRAME → TOP OF LCD) ==========
  fillRect(0, 0, 25, 240, BLACK);
  
  // WiFi status (will appear horizontal at top of LCD)
  uint16_t wifiColor = (WiFi.status() == WL_CONNECTED) ? GREEN : RED;
  drawString(8, 10, "WIFI", wifiColor);
  
  // SD status
  uint16_t sdColor = state.sdCardAvailable ? GREEN : RED;
  drawString(8, 70, "SD", sdColor);
  
  // Photo count
  char countStr[16];
  snprintf(countStr, sizeof(countStr), "%d", state.totalPhotos);
  drawString(8, 130, countStr, CYAN);
  
  // ========== MODE INDICATOR (RIGHT SIDE OF FRAME → BOTTOM OF LCD) ==========
  fillRect(295, 0, 320, 240, BLACK);
  
  // Mode text (will appear horizontal at bottom of LCD)
  const char* modeText = (state.captureMode == CaptureMode::INSTANT) ? "INSTANT" : "COUNTDOWN";
  uint16_t modeColor = (state.captureMode == CaptureMode::INSTANT) ? CYAN : YELLOW;
  
  // Center the text
  int textStartY = (state.captureMode == CaptureMode::INSTANT) ? 75 : 50;
  drawString(303, textStartY, modeText, modeColor);
}

/**
 * @brief Render camera mode UI overlay
 * 
 * This is now just a placeholder since UI is drawn directly onto frame buffer.
 */
void renderCameraUI() {
  // UI is now drawn directly onto the frame buffer in drawUIOntoFrame()
  // This function kept for compatibility but does nothing
}

/**
 * @brief Render status bar at top of screen
 * 
 * Shows WiFi status, SD card status, and photo count.
 */
void renderStatusBar() {
  // Use DMA buffer for faster drawing of black bar
  uint8_t* dmaBuf = getDMABuffer();
  
  // Fill buffer with black pixels (240 x 20 = 4800 pixels = 9600 bytes)
  for (int i = 0; i < 9600 && i < DMA_BUFFER_SIZE; i++) {
    dmaBuf[i] = 0x00; // Black
  }
  
  // Draw black background for status bar using DMA
  LCD_SetCursor(0, 0, 239, 19);
  DEV_Digital_Write(DEV_DC_PIN, 1); // Data mode
  DEV_Digital_Write(DEV_CS_PIN, 0); // CS Low
  
  // Write in chunks if needed
  int remaining = 9600;
  int offset = 0;
  while (remaining > 0) {
    int chunk = (remaining > DMA_BUFFER_SIZE) ? DMA_BUFFER_SIZE : remaining;
    for (int i = 0; i < chunk; i++) {
      DEV_SPI_WRITE(dmaBuf[i]);
    }
    remaining -= chunk;
    offset += chunk;
  }
  
  DEV_Digital_Write(DEV_CS_PIN, 1); // CS High
  
  // Small delay to ensure SPI completes
  delayMicroseconds(10);
  
  // Now draw text using Paint functions
  if (WiFi.status() == WL_CONNECTED) {
    Paint_DrawString_EN(5, 2, "WiFi", &Font12, BLACK, GREEN);
  } else {
    Paint_DrawString_EN(5, 2, "WiFi", &Font12, BLACK, RED);
  }
  
  if (state.sdCardAvailable) {
    Paint_DrawString_EN(50, 2, "SD", &Font12, BLACK, GREEN);
  } else {
    Paint_DrawString_EN(50, 2, "SD", &Font12, BLACK, RED);
  }
  
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
  // Use DMA buffer for faster drawing of black bar
  uint8_t* dmaBuf = getDMABuffer();
  
  // Fill buffer with black pixels (240 x 20 = 4800 pixels = 9600 bytes)
  for (int i = 0; i < 9600 && i < DMA_BUFFER_SIZE; i++) {
    dmaBuf[i] = 0x00; // Black
  }
  
  // Draw black background for mode indicator bar using DMA
  LCD_SetCursor(0, 300, 239, 319);
  DEV_Digital_Write(DEV_DC_PIN, 1); // Data mode
  DEV_Digital_Write(DEV_CS_PIN, 0); // CS Low
  
  // Write in chunks if needed
  int remaining = 9600;
  int offset = 0;
  while (remaining > 0) {
    int chunk = (remaining > DMA_BUFFER_SIZE) ? DMA_BUFFER_SIZE : remaining;
    for (int i = 0; i < chunk; i++) {
      DEV_SPI_WRITE(dmaBuf[i]);
    }
    remaining -= chunk;
    offset += chunk;
  }
  
  DEV_Digital_Write(DEV_CS_PIN, 1); // CS High
  
  // Small delay to ensure SPI completes
  delayMicroseconds(10);
  
  // Draw mode text using Paint functions
  const char* modeText = (state.captureMode == CaptureMode::INSTANT) 
                         ? "INSTANT" : "COUNTDOWN";
  uint16_t color = (state.captureMode == CaptureMode::INSTANT) 
                   ? CYAN : YELLOW;
  
  Paint_DrawString_EN(80, 302, modeText, &Font16, BLACK, color);
}

/**
 * @brief Render gallery mode UI
 * 
 * DEPRECATED: Now using drawGalleryUIOntoFrame() to avoid Paint/DMA conflicts.
 * This function kept for compatibility but does nothing.
 */
void renderGalleryUI() {
  // UI is now drawn directly onto the frame buffer in drawGalleryUIOntoFrame()
  // This function kept for compatibility but does nothing
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
  
  // IMPORTANT: Small delay to ensure DMA completes before Paint operations
  delayMicroseconds(100);
}

/**
 * @brief Handle instant photo capture
 * 
 * Captures photo immediately and saves to SD card.
 */
void handleCaptureInstant() {
  state.lastStatus = "Capturing...";
  setLED(true, false, false); // Red LED during capture
  
  if (state.sdCardAvailable) {
    // Save photo (LED feedback only, no screen messages)
    bool success = savePhoto();
    
    if (success) {
      state.lastStatus = "Saved!";
      ledBlink(false, true, false, 2); // Green blink = success
    } else {
      state.lastStatus = "Failed";
      ledBlink(true, false, false, 2); // Red blink = failed
    }
    
    delay(500); // Brief pause for LED feedback
  } else {
    ledBlink(true, false, false, 3); // Red blinks = no SD card
    delay(500);
  }
  
  setLED(false, false, false);
  state.lastStatus = "Idle";
  
  // No need to clear screen - camera loop will resume immediately
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
    // Use LED feedback instead of showMessage to avoid Paint/DMA conflict
    ledBlink(true, true, false, 3); // Yellow blinks = no photos
    return;
  }
  
  if (index < 0 || index >= state.totalPhotos) {
    return;
  }
  
  String photoPath = getPhotoPath(index);
  
  if (!SD.exists(photoPath)) {
    // File missing - red blinks
    ledBlink(true, false, false, 2);
    Serial.println("[ERROR] Photo not found!");
    delay(1000);
    return;
  }
  
  // Read JPEG file
  File file = SD.open(photoPath, FILE_READ);
  if (!file) {
    // Cannot open - red blinks
    ledBlink(true, false, false, 2);
    Serial.println("[ERROR] Cannot open file!");
    delay(1000);
    return;
  }
  
  size_t fileSize = file.size();
  if (fileSize > Config::MAX_JPEG_SIZE) {
    // File too large - red blinks
    ledBlink(true, false, false, 3);
    Serial.println("[ERROR] File too large!");
    file.close();
    delay(1000);
    return;
  }
  
  file.read(buffers.jpeg, fileSize);
  file.close();
  buffers.jpegLen = fileSize;
  
  // Decode and display
  if (decodeJpegToRGB565()) {
    // Draw gallery UI directly onto frame buffer (no Paint functions!)
    drawGalleryUIOntoFrame(buffers.frame);
    
    // Stream complete frame (with UI overlay) to LCD using DMA
    streamFrameToLCD(buffers.frame);
  } else {
    // Decode failed - red blinks
    ledBlink(true, false, false, 2);
    Serial.println("[ERROR] Decode error!");
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
  
  Serial.println("[SAVE] Starting photo save...");
  
  // Signal camera task to pause
  state.isSaving = true;
  Serial.println("[SAVE] Set isSaving=true, waiting for camera task to pause...");
  delay(50); // Give camera task time to finish current frame
  
  // Capture a fresh frame specifically for saving
  Serial.println("[SAVE] Capturing fresh frame...");
  if (!captureJpegToBuffer()) {
    Serial.println("[ERROR] Failed to capture frame for saving");
    state.isSaving = false;
    return false;
  }
  Serial.println("[SAVE] Frame captured successfully");
  
  digitalWrite(Pin::CAM_CS, HIGH); // Deselect camera
  
  // Ensure photos directory exists
  if (!SD.exists("/photos")) {
    SD.mkdir("/photos");
  }
  
  // Generate filename based on total photos count (not photoCounter)
  // This ensures sequential numbering: photo_1.jpg, photo_2.jpg, etc.
  char filename[32];
  snprintf(filename, sizeof(filename), "/photos/photo_%d.jpg", state.totalPhotos + 1);
  
  Serial.print("[SAVE] Opening file: ");
  Serial.println(filename);
  
  // Write file
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("[ERROR] Cannot create file");
    state.isSaving = false;
    return false;
  }
  
  Serial.print("[SAVE] Writing ");
  Serial.print(buffers.jpegLen);
  Serial.println(" bytes...");
  
  size_t written = file.write(buffers.jpeg, buffers.jpegLen);
  file.close();
  
  Serial.print("[SAVE] Wrote ");
  Serial.print(written);
  Serial.println(" bytes");
  
  bool success = false;
  if (written == buffers.jpegLen) {
    state.totalPhotos++;
    Serial.print("[INFO] Photo saved: ");
    Serial.println(filename);
    success = true;
  }
  
  // Resume camera task
  Serial.println("[SAVE] Setting isSaving=false to resume camera task...");
  state.isSaving = false;
  Serial.println("[SAVE] Save complete, camera task should resume now");
  
  return success;
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
