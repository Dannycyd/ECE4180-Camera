/**
 * @file ui_manager.h
 * @brief User interface management (buttons, modes, status)
 */

#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "config.h"
#include "hardware_manager.h"

class UIManager {
public:
  static void init() {
    DEBUG_PRINTLN("→ Initializing UI...");
    
    // Attach button interrupts
    attachInterrupt(digitalPinToInterrupt(Pins::BUTTON_CAPTURE), 
                    captureButtonISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(Pins::BUTTON_MODE), 
                    modeButtonISR, FALLING);
    
    // Set initial mode
    currentMode = CaptureMode::INSTANT;
    currentStatus = SystemStatus::IDLE;
    
    DEBUG_PRINTLN("✓ UI initialized");
  }
  
  /**
   * @brief Get current capture mode
   */
  static CaptureMode getCurrentMode() {
    return currentMode;
  }
  
  /**
   * @brief Get current system status
   */
  static SystemStatus getCurrentStatus() {
    return currentStatus;
  }
  
  /**
   * @brief Get status as string
   */
  static String getStatusString() {
    switch (currentStatus) {
      case SystemStatus::IDLE:       return "Idle";
      case SystemStatus::CAPTURING:  return "Capturing";
      case SystemStatus::SAVING:     return "Saving";
      case SystemStatus::STREAMING:  return "Streaming";
      case SystemStatus::ERROR:      return "Error";
      default:                       return "Unknown";
    }
  }
  
  /**
   * @brief Set system status
   */
  static void setStatus(SystemStatus status) {
    currentStatus = status;
    updateLED();
  }
  
  /**
   * @brief Request capture (from web or button)
   */
  static void requestCapture() {
    captureRequested = true;
  }
  
  /**
   * @brief Request mode toggle
   */
  static void requestModeToggle() {
    modeToggleRequested = true;
  }
  
  /**
   * @brief Request countdown
   */
  static void requestCountdown() {
    countdownRequested = true;
  }
  
  /**
   * @brief Check if capture was requested
   */
  static bool isCaptureRequested() {
    if (captureRequested) {
      captureRequested = false;
      return true;
    }
    return false;
  }
  
  /**
   * @brief Check if mode toggle was requested
   */
  static bool isModeToggleRequested() {
    if (modeToggleRequested) {
      modeToggleRequested = false;
      return true;
    }
    return false;
  }
  
  /**
   * @brief Check if countdown was requested
   */
  static bool isCountdownRequested() {
    if (countdownRequested) {
      countdownRequested = false;
      return true;
    }
    return false;
  }
  
  /**
   * @brief Toggle capture mode
   */
  static void toggleMode() {
    currentMode = (currentMode == CaptureMode::INSTANT) 
                  ? CaptureMode::COUNTDOWN 
                  : CaptureMode::INSTANT;
    
    DEBUG_PRINTF("Mode: %s\n", 
                 currentMode == CaptureMode::INSTANT ? "INSTANT" : "COUNTDOWN");
    
    // Visual feedback
    HardwareManager::setLED(LEDColor::BLUE);
    delay(200);
    updateLED();
  }
  
  /**
   * @brief Update LED based on current status
   */
  static void updateLED() {
    switch (currentStatus) {
      case SystemStatus::IDLE:
        HardwareManager::setLED(LEDColor::GREEN);
        break;
      case SystemStatus::CAPTURING:
        HardwareManager::setLED(LEDColor::RED);
        break;
      case SystemStatus::SAVING:
        HardwareManager::setLED(LEDColor::YELLOW);
        break;
      case SystemStatus::STREAMING:
        HardwareManager::setLED(LEDColor::CYAN);
        break;
      case SystemStatus::ERROR:
        HardwareManager::setLED(LEDColor::RED);
        break;
    }
  }
  
  /**
   * @brief Countdown animation (3-2-1)
   * @return true when countdown completes
   */
  static bool performCountdown() {
    for (uint8_t i = UIConfig::COUNTDOWN_SECONDS; i > 0; i--) {
      DEBUG_PRINTF("Countdown: %d\n", i);
      
      // Blink LED
      HardwareManager::setLED(LEDColor::RED);
      delay(500);
      HardwareManager::setLED(LEDColor::OFF);
      delay(500);
    }
    
    HardwareManager::setLED(LEDColor::GREEN);
    return true;
  }
  
  /**
   * @brief Flash LED briefly
   */
  static void flashLED(LEDColor color, uint32_t durationMs = 200) {
    HardwareManager::setLED(color);
    delay(durationMs);
    updateLED();
  }
  
private:
  // State variables
  static CaptureMode currentMode;
  static SystemStatus currentStatus;
  
  // Request flags
  static volatile bool captureRequested;
  static volatile bool modeToggleRequested;
  static volatile bool countdownRequested;
  
  // Debouncing
  static volatile uint32_t lastCaptureButtonTime;
  static volatile uint32_t lastModeButtonTime;
  
  /**
   * @brief Capture button ISR
   */
  static void IRAM_ATTR captureButtonISR() {
    uint32_t now = millis();
    if (now - lastCaptureButtonTime > UIConfig::DEBOUNCE_MS) {
      captureRequested = true;
      lastCaptureButtonTime = now;
    }
  }
  
  /**
   * @brief Mode button ISR
   */
  static void IRAM_ATTR modeButtonISR() {
    uint32_t now = millis();
    if (now - lastModeButtonTime > UIConfig::DEBOUNCE_MS) {
      modeToggleRequested = true;
      lastModeButtonTime = now;
    }
  }
};

// Static member initialization
CaptureMode UIManager::currentMode = CaptureMode::INSTANT;
SystemStatus UIManager::currentStatus = SystemStatus::IDLE;
volatile bool UIManager::captureRequested = false;
volatile bool UIManager::modeToggleRequested = false;
volatile bool UIManager::countdownRequested = false;
volatile uint32_t UIManager::lastCaptureButtonTime = 0;
volatile uint32_t UIManager::lastModeButtonTime = 0;

#endif // UI_MANAGER_H
