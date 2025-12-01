/**
 * @file config.h
 * @brief Central configuration for Stitch Cam
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

//=============================================================================
// WIFI CONFIGURATION
//=============================================================================
#define WIFI_SSID     "KellyiPhone"
#define WIFI_PASSWORD "kelly200636"
#define WIFI_TIMEOUT_MS 30000

//=============================================================================
// PIN DEFINITIONS - ESP32-S3
//=============================================================================
namespace Pins {
  // Camera SPI
  constexpr uint8_t CAM_SCK  = 10;
  constexpr uint8_t CAM_MISO = 11;
  constexpr uint8_t CAM_MOSI = 12;
  constexpr uint8_t CAM_CS   = 13;
  
  // I2C (Camera control)
  constexpr uint8_t SDA = 9;
  constexpr uint8_t SCL = 8;
  
  // SD Card
  constexpr uint8_t SD_CS = 14;
  
  // LCD (separate SPI - FSPI)
  constexpr uint8_t LCD_SCK  = 5;
  constexpr uint8_t LCD_MOSI = 4;
  constexpr uint8_t LCD_MISO = -1;  // Not used
  constexpr uint8_t LCD_CS   = 17;
  constexpr uint8_t LCD_DC   = 18;
  constexpr uint8_t LCD_RST  = 15;
  constexpr uint8_t LCD_BL   = 16;
  
  // UI Controls
  constexpr uint8_t BUTTON_CAPTURE = 1;
  constexpr uint8_t BUTTON_MODE    = 45;
  
  // RGB LED
  constexpr uint8_t LED_RED   = 2;
  constexpr uint8_t LED_GREEN = 42;
  constexpr uint8_t LED_BLUE  = 41;
}

//=============================================================================
// CAMERA CONFIGURATION
//=============================================================================
namespace CameraConfig {
  constexpr uint16_t FRAME_WIDTH  = 320;
  constexpr uint16_t FRAME_HEIGHT = 240;
  constexpr uint32_t FRAME_BUFFER_SIZE = FRAME_WIDTH * FRAME_HEIGHT * 2; // RGB565
  constexpr uint32_t MAX_JPEG_SIZE = 32768;  // 32KB
}

//=============================================================================
// LCD CONFIGURATION
//=============================================================================
namespace LCDConfig {
  constexpr uint16_t WIDTH  = 240;
  constexpr uint16_t HEIGHT = 320;
  constexpr uint8_t  BACKLIGHT_DEFAULT = 100;  // 0-100%
}

//=============================================================================
// STORAGE CONFIGURATION
//=============================================================================
namespace StorageConfig {
  constexpr const char* PHOTO_DIR = "/photos";
  constexpr const char* PHOTO_PREFIX = "IMG_";
  constexpr uint32_t MAX_FILENAME_LEN = 32;
}

//=============================================================================
// UI CONFIGURATION
//=============================================================================
namespace UIConfig {
  constexpr uint32_t DEBOUNCE_MS = 200;
  constexpr uint32_t COUNTDOWN_SECONDS = 3;
  constexpr uint32_t STATUS_DISPLAY_MS = 2000;
}

//=============================================================================
// RTOS TASK CONFIGURATION
//=============================================================================
namespace TaskConfig {
  // Stack sizes (bytes)
  constexpr uint32_t CAMERA_STACK_SIZE = 8192;
  constexpr uint32_t WEB_STACK_SIZE    = 4096;
  constexpr uint32_t UI_STACK_SIZE     = 4096;
  
  // Priorities (0 = lowest, configMAX_PRIORITIES-1 = highest)
  constexpr UBaseType_t CAMERA_PRIORITY = 2;
  constexpr UBaseType_t WEB_PRIORITY    = 1;
  constexpr UBaseType_t UI_PRIORITY     = 2;
  
  // Core assignment
  constexpr BaseType_t CAMERA_CORE = 0;  // Hardware tasks on Core 0
  constexpr BaseType_t WEB_CORE    = 1;  // Network tasks on Core 1
  constexpr BaseType_t UI_CORE     = 0;  // UI with camera
}

//=============================================================================
// WEB SERVER CONFIGURATION
//=============================================================================
namespace WebConfig {
  constexpr uint16_t HTTP_PORT = 80;
  constexpr uint32_t STREAM_TIMEOUT_MS = 5000;
}

//=============================================================================
// DEBUG CONFIGURATION
//=============================================================================
#define DEBUG_ENABLED 1

#if DEBUG_ENABLED
  #define DEBUG_PRINT(x)   Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(...)Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

//=============================================================================
// SYSTEM STATES
//=============================================================================
enum class CaptureMode : uint8_t {
  INSTANT,
  COUNTDOWN
};

enum class SystemStatus : uint8_t {
  IDLE,
  CAPTURING,
  SAVING,
  STREAMING,
  ERROR
};

enum class LEDColor : uint8_t {
  OFF,
  RED,
  GREEN,
  BLUE,
  YELLOW,    // RED + GREEN
  CYAN,      // GREEN + BLUE
  MAGENTA,   // RED + BLUE
  WHITE      // ALL ON
};

#endif // CONFIG_H
