/**
 * @file task_manager.h
 * @brief FreeRTOS task creation and management
 */

#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "config.h"
#include "camera_manager.h"
#include "lcd_manager.h"
#include "storage_manager.h"
#include "ui_manager.h"
#include "hardware_manager.h"

class TaskManager {
public:
  /**
   * @brief Create all FreeRTOS tasks
   */
  static void createTasks() {
    DEBUG_PRINTLN("→ Creating FreeRTOS Tasks...");
    
    // Create camera task on Core 0 (hardware)
    xTaskCreatePinnedToCore(
      cameraTask,                        // Task function
      "CameraTask",                      // Name
      TaskConfig::CAMERA_STACK_SIZE,     // Stack size
      NULL,                              // Parameters
      TaskConfig::CAMERA_PRIORITY,       // Priority
      &cameraTaskHandle,                 // Task handle
      TaskConfig::CAMERA_CORE            // Core ID
    );
    DEBUG_PRINTLN("  ✓ Camera task created (Core 0)");
    
    // UI task runs on same core as camera for hardware access
    xTaskCreatePinnedToCore(
      uiTask,
      "UITask",
      TaskConfig::UI_STACK_SIZE,
      NULL,
      TaskConfig::UI_PRIORITY,
      &uiTaskHandle,
      TaskConfig::UI_CORE
    );
    DEBUG_PRINTLN("  ✓ UI task created (Core 0)");
    
    DEBUG_PRINTLN("✓ All tasks created");
    DEBUG_PRINTLN("\n╔════════════════════════════════════╗");
    DEBUG_PRINTLN("║       System Ready! 🌺💙          ║");
    DEBUG_PRINTLN("╚════════════════════════════════════╝");
  }
  
private:
  static TaskHandle_t cameraTaskHandle;
  static TaskHandle_t uiTaskHandle;
  
  //===========================================================================
  // CAMERA TASK (Core 0) - Hardware operations
  //===========================================================================
  static void cameraTask(void* parameter) {
    DEBUG_PRINTLN("Camera task started");
    
    // Show boot screen
    LCDManager::displayBootScreen();
    delay(1000);
    
    // Main loop
    for (;;) {
      // Capture frame for live preview
      if (CameraManager::captureJPEG()) {
        // Decode to RGB565
        if (CameraManager::decodeToFrameBuffer()) {
          // Display on LCD
          LCDManager::displayFrame(CameraManager::getFrameBuffer());
          
          // Update UI overlays
          LCDManager::displayMode(UIManager::getCurrentMode());
        }
      }
      
      // Small delay to prevent watchdog timeout
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  
  //===========================================================================
  // UI TASK (Core 0) - User interaction
  //===========================================================================
  static void uiTask(void* parameter) {
    DEBUG_PRINTLN("UI task started");
    
    char filename[StorageConfig::MAX_FILENAME_LEN];
    
    for (;;) {
      // Handle mode toggle
      if (UIManager::isModeToggleRequested()) {
        UIManager::toggleMode();
        LCDManager::displayMode(UIManager::getCurrentMode());
      }
      
      // Handle capture request
      if (UIManager::isCaptureRequested()) {
        handleCapture(filename);
      }
      
      // Handle countdown request
      if (UIManager::isCountdownRequested()) {
        handleCountdownCapture(filename);
      }
      
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
  
  //===========================================================================
  // Helper Functions
  //===========================================================================
  
  /**
   * @brief Handle instant capture
   */
  static void handleCapture(char* filename) {
    DEBUG_PRINTLN("→ Instant Capture");
    
    UIManager::setStatus(SystemStatus::CAPTURING);
    HardwareManager::setLED(LEDColor::RED);
    
    // Capture JPEG
    if (!CameraManager::captureJPEG()) {
      DEBUG_PRINTLN("  ❌ Capture failed");
      UIManager::setStatus(SystemStatus::ERROR);
      LCDManager::displayError("Capture Failed");
      delay(1000);
      UIManager::setStatus(SystemStatus::IDLE);
      return;
    }
    
    // Save to SD card
    UIManager::setStatus(SystemStatus::SAVING);
    HardwareManager::setLED(LEDColor::YELLOW);
    
    if (StorageManager::savePhoto(
          CameraManager::getJPEGBuffer(),
          CameraManager::getJPEGLength(),
          filename)) {
      
      DEBUG_PRINTF("  ✓ Saved: %s\n", filename);
      
      // Show success
      HardwareManager::setLED(LEDColor::GREEN);
      LCDManager::displaySaveMessage(filename);
      delay(UIConfig::STATUS_DISPLAY_MS);
    } else {
      DEBUG_PRINTLN("  ❌ Save failed");
      UIManager::setStatus(SystemStatus::ERROR);
      LCDManager::displayError("Save Failed");
      delay(1000);
    }
    
    UIManager::setStatus(SystemStatus::IDLE);
  }
  
  /**
   * @brief Handle countdown capture
   */
  static void handleCountdownCapture(char* filename) {
    DEBUG_PRINTLN("→ Countdown Capture");
    
    UIManager::setStatus(SystemStatus::CAPTURING);
    
    // Perform countdown
    for (uint8_t i = UIConfig::COUNTDOWN_SECONDS; i > 0; i--) {
      DEBUG_PRINTF("  Countdown: %d\n", i);
      
      // Display countdown number on LCD
      LCDManager::displayCountdown(i);
      
      // Flash LED
      HardwareManager::setLED(LEDColor::RED);
      delay(500);
      HardwareManager::setLED(LEDColor::OFF);
      delay(500);
    }
    
    // Capture!
    HardwareManager::setLED(LEDColor::RED);
    
    if (!CameraManager::captureJPEG()) {
      DEBUG_PRINTLN("  ❌ Capture failed");
      UIManager::setStatus(SystemStatus::ERROR);
      LCDManager::displayError("Capture Failed");
      delay(1000);
      UIManager::setStatus(SystemStatus::IDLE);
      return;
    }
    
    // Save
    UIManager::setStatus(SystemStatus::SAVING);
    HardwareManager::setLED(LEDColor::YELLOW);
    
    if (StorageManager::savePhoto(
          CameraManager::getJPEGBuffer(),
          CameraManager::getJPEGLength(),
          filename)) {
      
      DEBUG_PRINTF("  ✓ Saved: %s\n", filename);
      
      // Show success
      HardwareManager::setLED(LEDColor::GREEN);
      LCDManager::displaySaveMessage(filename);
      delay(UIConfig::STATUS_DISPLAY_MS);
    } else {
      DEBUG_PRINTLN("  ❌ Save failed");
      UIManager::setStatus(SystemStatus::ERROR);
      LCDManager::displayError("Save Failed");
      delay(1000);
    }
    
    UIManager::setStatus(SystemStatus::IDLE);
  }
};

// Static member initialization
TaskHandle_t TaskManager::cameraTaskHandle = NULL;
TaskHandle_t TaskManager::uiTaskHandle = NULL;

#endif // TASK_MANAGER_H
