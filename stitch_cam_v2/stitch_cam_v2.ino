/**
 * @file main.ino
 * @brief ESP32-S3 Stitch Cam - Main Entry Point
 * @version 2.0
 * 
 * Clean RTOS architecture:
 * - Core 0: Camera, LCD, UI (hardware tasks)
 * - Core 1: WiFi, Web Server (network tasks)
 */

// Include WiFi FIRST to prevent ArduCAM macro conflicts
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
#include "task_manager.h"

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║   STITCH CAM v2.0 - RTOS Edition   ║");
  Serial.println("╚════════════════════════════════════╝\n");
  
  // Initialize hardware (order matters!)
  HardwareManager::init();
  StorageManager::init();
  CameraManager::init();
  LCDManager::init();
  UIManager::init();
  
  // Initialize network
  WiFiManager::init();
  WebServer::init();
  
  // Create FreeRTOS tasks
  TaskManager::createTasks();
  
  Serial.println("✓ System ready!\n");
}

void loop() {
  // Core 1: Handle web server
  WebServer::handleClient();
  vTaskDelay(pdMS_TO_TICKS(1));
}
