/**
 * @file ui_manager.h
 * @brief User interface manager with button handling
 */

#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include "config.h"
#include "hardware_manager.h"
#include "lcd_manager.h"

// CRITICAL: Volatile flags for ISR (must be in IRAM or regular RAM)
static volatile bool captureRequested = false;
static volatile bool modeToggleRequested = false;

class UIManager {
private:
  static CaptureMode currentMode;
  static SystemStatus currentStatus;
  
public:
  // Simplified ISR functions - ONLY set flags!
  static void IRAM_ATTR captureButtonISR() {
    captureRequested = true;
  }
  
  static void IRAM_ATTR modeButtonISR() {
    modeToggleRequested = true;
  }
  
  // Initialize UI (setup buttons)
  static void init() {
    DEBUG_PRINTLN("→ Initializing UI...");
    
    // Setup buttons with pull-ups
    pinMode(Pins::BTN_CAPTURE, INPUT_PULLUP);
    pinMode(Pins::BTN_MODE, INPUT_PULLUP);
    
    // Attach interrupts
    attachInterrupt(digitalPinToInterrupt(Pins::BTN_CAPTURE), 
                    captureButtonISR, FALLING);
    attachInterrupt(digitalPinToInterrupt(Pins::BTN_MODE), 
                    modeButtonISR, FALLING);
    
    currentMode = CaptureMode::INSTANT;
    currentStatus = SystemStatus::READY;
    
    DEBUG_PRINTLN("  ✓ UI initialized");
  }
  
  // Check and handle button presses (call from main loop)
  static bool checkCaptureButton() {
    static unsigned long lastPress = 0;
    
    if (captureRequested) {
      unsigned long now = millis();
      if (now - lastPress > UIConfig::BUTTON_DEBOUNCE_MS) {
        captureRequested = false;
        lastPress = now;
        return true;
      }
      captureRequested = false; // Clear if debounce failed
    }
    return false;
  }
  
  static bool checkModeButton() {
    static unsigned long lastPress = 0;
    
    if (modeToggleRequested) {
      unsigned long now = millis();
      if (now - lastPress > UIConfig::BUTTON_DEBOUNCE_MS) {
        modeToggleRequested = false;
        lastPress = now;
        toggleMode();
        return true;
      }
      modeToggleRequested = false; // Clear if debounce failed
    }
    return false;
  }
  
  // Toggle capture mode
  static void toggleMode() {
    if (currentMode == CaptureMode::INSTANT) {
      currentMode = CaptureMode::COUNTDOWN;
    } else {
      currentMode = CaptureMode::INSTANT;
    }
    
    DEBUG_PRINT("Mode: ");
    DEBUG_PRINTLN(currentMode == CaptureMode::INSTANT ? "INSTANT" : "COUNTDOWN");
    
    // Visual feedback
    HardwareManager::setLED(LEDColor::BLUE);
    LCDManager::displayMode(currentMode);
    delay(300);
    updateLED();
  }
  
  // Get current mode
  static CaptureMode getMode() {
    return currentMode;
  }
  
  // Set system status
  static void setStatus(SystemStatus status) {
    currentStatus = status;
    updateLED();
  }
  
  // Update LED based on status
  static void updateLED() {
    switch (currentStatus) {
      case SystemStatus::READY:
        HardwareManager::setLED(LEDColor::GREEN);
        break;
      case SystemStatus::ERROR:
        HardwareManager::setLED(LEDColor::RED);
        break;
      case SystemStatus::CAPTURING:
        HardwareManager::setLED(LEDColor::YELLOW);
        break;
      case SystemStatus::COUNTDOWN:
        HardwareManager::setLED(LEDColor::CYAN);
        break;
      case SystemStatus::BUSY:
        HardwareManager::setLED(LEDColor::RED);
        break;
    }
  }
  
  // Perform countdown (blocking)
  static bool performCountdown() {
    setStatus(SystemStatus::COUNTDOWN);
    
    for (uint8_t i = UIConfig::COUNTDOWN_DURATION; i > 0; i--) {
      LCDManager::displayCountdown(i);
      HardwareManager::setLED(LEDColor::RED);
      delay(500);
      HardwareManager::setLED(LEDColor::OFF);
      delay(500);
    }
    
    HardwareManager::setLED(LEDColor::GREEN);
    return true;
  }
};

// Initialize static members
CaptureMode UIManager::currentMode = CaptureMode::INSTANT;
SystemStatus UIManager::currentStatus = SystemStatus::READY;

#endif // UI_MANAGER_H