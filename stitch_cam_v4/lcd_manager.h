/**
 * @file lcd_manager.h
 * @brief LCD display management with ST7789 driver
 */

#ifndef LCD_MANAGER_H
#define LCD_MANAGER_H

#include "config.h"
#include "DEV_Config.h"
#include "LCD_Driver.h"
#include "GUI_Paint.h"

// CRITICAL: Undefine ALL color macros from GUI_Paint.h FIRST
#undef BLACK
#undef WHITE
#undef RED
#undef GREEN
#undef BLUE
#undef YELLOW
#undef CYAN
#undef MAGENTA

// Redefine as const values for LCD use (RGB565 format)
namespace LCDColor {
  constexpr uint16_t BLACK   = 0x0000;
  constexpr uint16_t WHITE   = 0xFFFF;
  constexpr uint16_t RED     = 0xF800;
  constexpr uint16_t GREEN   = 0x07E0;
  constexpr uint16_t BLUE    = 0x001F;
  constexpr uint16_t YELLOW  = 0xFFE0;
  constexpr uint16_t CYAN    = 0x7FFF;
  constexpr uint16_t MAGENTA = 0xF81F;
}

class LCDManager {
public:
  static void init() {
    DEBUG_PRINTLN("→ Initializing LCD...");
    
    // Initialize LCD hardware
    Config_Init();
    LCD_Init();
    LCD_SetBacklight(LCDConfig::BACKLIGHT_DEFAULT);
    
    // Clear screen
    clear(LCDColor::BLACK);
    
    DEBUG_PRINTLN("✓ LCD initialized");
  }
  
  /**
   * @brief Clear entire screen with color
   */
  static void clear(uint16_t color) {
    LCD_Clear(color);
  }
  
  /**
   * @brief Set backlight brightness (0-100%)
   */
  static void setBacklight(uint8_t percent) {
    if (percent > 100) percent = 100;
    LCD_SetBacklight(percent);
  }
  
  /**
   * @brief Display RGB565 frame buffer on LCD
   * Uses DMA for fast transfer
   */
  static void displayFrame(const uint8_t* frameBuffer) {
    if (!frameBuffer) return;
    
    // Set drawing area
    LCD_SetCursor(0, 0, CameraConfig::FRAME_WIDTH - 1, CameraConfig::FRAME_HEIGHT - 1);
    
    // Start bulk transfer mode (no CS toggling)
    DEV_SPI_Write_Bulk_Start();
    
    // Transfer frame buffer in chunks using DMA
    const uint32_t totalBytes = CameraConfig::FRAME_WIDTH * CameraConfig::FRAME_HEIGHT * 2;
    const uint32_t chunkSize = DMA_BUFFER_SIZE;
    
    for (uint32_t offset = 0; offset < totalBytes; offset += chunkSize) {
      uint32_t size = min(chunkSize, totalBytes - offset);
      DEV_SPI_Write_Bulk_Data(frameBuffer + offset, size);
    }
    
    // End bulk transfer
    DEV_SPI_Write_Bulk_End();
  }
  
  /**
   * @brief Display text message
   */
  static void displayText(uint16_t x, uint16_t y, const char* text, 
                          uint16_t fgColor = LCDColor::WHITE, uint16_t bgColor = LCDColor::BLACK) {
    Paint_DrawString_EN(x, y, text, &Font16, bgColor, fgColor);
  }
  
  /**
   * @brief Display centered text
   */
  static void displayTextCentered(uint16_t y, const char* text, 
                                   uint16_t fgColor = LCDColor::WHITE, uint16_t bgColor = LCDColor::BLACK) {
    uint16_t textWidth = strlen(text) * 8;  // Assuming Font16 width ~8px
    uint16_t x = (LCDConfig::WIDTH - textWidth) / 2;
    displayText(x, y, text, fgColor, bgColor);
  }
  
  /**
   * @brief Display countdown number (large)
   */
  static void displayCountdown(uint8_t number) {
    // Clear screen with semi-transparent overlay
    clear(0x0010);  // Dark blue
    
    // Display large number in center
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", number);
    
    // Calculate center position
    uint16_t x = (LCDConfig::WIDTH - 32) / 2;   // Font24 width ~32px
    uint16_t y = (LCDConfig::HEIGHT - 24) / 2;  // Font24 height 24px
    
    Paint_DrawString_EN(x, y, buf, &Font24, 0x0010, LCDColor::WHITE);
  }
  
  /**
   * @brief Display status message at bottom
   */
  static void displayStatus(const char* status, uint16_t color = LCDColor::GREEN) {
    // Draw filled rectangle for status bar
    Paint_DrawFilledRectangle_Fast(0, LCDConfig::HEIGHT - 30, 
                                     LCDConfig::WIDTH, LCDConfig::HEIGHT, 
                                     LCDColor::BLACK);
    
    // Draw text
    displayTextCentered(LCDConfig::HEIGHT - 20, status, color, LCDColor::BLACK);
  }
  
  /**
   * @brief Display save confirmation
   */
  static void displaySaveMessage(const char* filename) {
    displayStatus("Saved!", LCDColor::GREEN);
    
    // Show filename briefly
    displayTextCentered(LCDConfig::HEIGHT - 50, filename, LCDColor::WHITE, LCDColor::BLACK);
  }
  
  /**
   * @brief Display error message
   */
  static void displayError(const char* error) {
    clear(LCDColor::RED);
    displayTextCentered(LCDConfig::HEIGHT / 2, "ERROR", LCDColor::WHITE, LCDColor::RED);
    displayTextCentered(LCDConfig::HEIGHT / 2 + 20, error, LCDColor::WHITE, LCDColor::RED);
  }
  
  /**
   * @brief Display mode indicator
   */
  static void displayMode(CaptureMode mode) {
    const char* modeText = (mode == CaptureMode::INSTANT) ? "INSTANT" : "COUNTDOWN";
    
    // Draw mode indicator in top-right corner
    uint16_t x = LCDConfig::WIDTH - 80;
    Paint_DrawFilledRectangle_Fast(x, 5, LCDConfig::WIDTH - 5, 25, LCDColor::BLUE);
    displayText(x + 5, 10, modeText, LCDColor::WHITE, LCDColor::BLUE);
  }
  
  /**
   * @brief Display boot screen
   */
  static void displayBootScreen() {
    clear(0x0318);  // Stitch blue gradient
    
    displayTextCentered(100, "STITCH CAM", LCDColor::WHITE, 0x0318);
    displayTextCentered(120, "v2.0", LCDColor::CYAN, 0x0318);
    displayTextCentered(160, "Initializing...", LCDColor::WHITE, 0x0318);
  }
  
  /**
   * @brief Display WiFi connecting screen
   */
  static void displayWiFiConnecting(const char* ssid) {
    displayTextCentered(180, "Connecting WiFi:", LCDColor::YELLOW, LCDColor::BLACK);
    displayTextCentered(200, ssid, LCDColor::WHITE, LCDColor::BLACK);
  }
  
  /**
   * @brief Display WiFi connected with IP
   */
  static void displayWiFiConnected(const char* ip) {
    displayStatus("WiFi Connected!", LCDColor::GREEN);
    displayTextCentered(LCDConfig::HEIGHT - 50, ip, LCDColor::CYAN, LCDColor::BLACK);
  }
  
  /**
   * @brief Draw rectangle
   */
  static void drawRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, 
                            uint16_t color) {
    Paint_DrawRectangle(x1, y1, x2, y2, color, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  }
  
  /**
   * @brief Draw filled rectangle
   */
  static void drawFilledRectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, 
                                   uint16_t color) {
    Paint_DrawFilledRectangle_Fast(x1, y1, x2, y2, color);
  }
};

#endif // LCD_MANAGER_H
