/**
 * @file stitch_cam_v4.ino
 * @brief ESP32-S3 Stitch Cam - Simple Edition
 * @version 4.0
 */

#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include <SPI.h>

#include "config.h"
#include "hardware_manager.h"
#include "camera_manager.h"
#include "lcd_manager.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "ui_manager.h"
#include "storage_manager.h"

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║        STITCH CAM v4.0             ║");
  Serial.println("╚════════════════════════════════════╝\n");
  
  HardwareManager::init();
  StorageManager::init();
  CameraManager::init();
  LCDManager::init();
  UIManager::init();
  WiFiManager::init();
  StitchWebServer::init();
  
  Serial.println("✓ Ready!\n");
  UIManager::setStatus(SystemStatus::READY);
}

void loop() {
  // Button handling
  if (UIManager::checkCaptureButton()) {
    capturePhoto();
  }
  UIManager::checkModeButton();
  
  // Web server
  StitchWebServer::handleClient();
  delay(10);
}

void capturePhoto() {
  UIManager::setStatus(SystemStatus::CAPTURING);
  
  if (UIManager::getMode() == CaptureMode::COUNTDOWN) {
    UIManager::performCountdown();
  }
  
  if (CameraManager::capture()) {
    String filename = StorageManager::savePhoto(
      CameraManager::getImageData(), 
      CameraManager::getImageSize()
    );
    
    if (filename.length() > 0) {
      LCDManager::displaySaveMessage(filename.c_str());
      delay(1500);
    }
  }
  
  UIManager::setStatus(SystemStatus::READY);
}