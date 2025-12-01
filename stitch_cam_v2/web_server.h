/**
 * @file web_server.h
 * @brief HTTP web server with RESTful API
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "config.h"
#include <WebServer.h>
#include "camera_manager.h"
#include "storage_manager.h"
#include "ui_manager.h"

// External HTML (compressed)
#include "index_html_gz.h"
#include "gallery_html_gz.h"

class WebServer {
public:
  static void init() {
    DEBUG_PRINTLN("→ Initializing Web Server...");
    
    // Register routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/gallery", HTTP_GET, handleGallery);
    server.on("/capture", HTTP_GET, handleCapture);
    server.on("/toggle", HTTP_GET, handleToggleMode);
    server.on("/countdown_start", HTTP_GET, handleCountdownStart);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/stream", HTTP_GET, handleStream);
    server.on("/photos", HTTP_GET, handlePhotoList);
    server.on("/photo", HTTP_GET, handlePhoto);
    server.on("/delete", HTTP_GET, handleDeletePhoto);
    
    // Start server
    server.begin();
    DEBUG_PRINTF("✓ Web server started on port %d\n", WebConfig::HTTP_PORT);
  }
  
  /**
   * @brief Handle client requests (call from loop)
   */
  static void handleClient() {
    server.handleClient();
  }
  
private:
  static WebServer server;
  
  //===========================================================================
  // Route Handlers
  //===========================================================================
  
  /**
   * @brief Serve main page (GZIP compressed)
   */
  static void handleRoot() {
    server.sendHeader("Content-Encoding", "gzip");
    server.send_P(200, "text/html",
      (const char*)index_html_gz,
      index_html_gz_len
    );
  }
  
  /**
   * @brief Serve gallery page (GZIP compressed)
   */
  static void handleGallery() {
    server.sendHeader("Content-Encoding", "gzip");
    server.send_P(200, "text/html",
      (const char*)gallery_html_gz,
      gallery_html_gz_len
    );
  }
  
  /**
   * @brief Trigger instant capture
   */
  static void handleCapture() {
    UIManager::requestCapture();
    server.send(200, "text/plain", "OK");
  }
  
  /**
   * @brief Toggle capture mode
   */
  static void handleToggleMode() {
    UIManager::requestModeToggle();
    server.send(200, "text/plain", "OK");
  }
  
  /**
   * @brief Start countdown capture
   */
  static void handleCountdownStart() {
    UIManager::requestCountdown();
    server.send(200, "text/plain", "OK");
  }
  
  /**
   * @brief Get system status (JSON)
   */
  static void handleStatus() {
    String mode = (UIManager::getCurrentMode() == CaptureMode::INSTANT) 
                  ? "Instant" : "Countdown";
    
    String json = "{";
    json += "\"mode\":\"" + mode + "\",";
    json += "\"status\":\"" + UIManager::getStatusString() + "\",";
    json += "\"photos\":" + String(StorageManager::getPhotoCount()) + ",";
    json += "\"sdAvailable\":" + String(StorageManager::available() ? "true" : "false") + ",";
    json += "\"cameraAvailable\":" + String(CameraManager::available() ? "true" : "false");
    json += "}";
    
    server.send(200, "application/json", json);
  }
  
  /**
   * @brief Get current camera frame (JPEG)
   */
  static void handleStream() {
    uint32_t jpegLen = CameraManager::getJPEGLength();
    
    if (jpegLen == 0 || jpegLen > CameraConfig::MAX_JPEG_SIZE) {
      server.send(503, "text/plain", "No frame available");
      return;
    }
    
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    
    server.send_P(200, "image/jpeg", 
                  (const char*)CameraManager::getJPEGBuffer(), 
                  jpegLen);
  }
  
  /**
   * @brief Get list of photos (JSON)
   */
  static void handlePhotoList() {
    if (!StorageManager::available()) {
      server.send(200, "application/json", "{\"photos\":[]}");
      return;
    }
    
    // Get photo list
    const uint32_t MAX_FILES = 100;
    String fileList[MAX_FILES];
    uint32_t count = StorageManager::getPhotoList(fileList, MAX_FILES);
    
    // Build JSON array
    String json = "{\"photos\":[";
    for (uint32_t i = 0; i < count; i++) {
      if (i > 0) json += ",";
      json += "\"" + fileList[i] + "\"";
    }
    json += "]}";
    
    server.send(200, "application/json", json);
  }
  
  /**
   * @brief Download specific photo
   */
  static void handlePhoto() {
    if (!server.hasArg("name")) {
      server.send(400, "text/plain", "Missing 'name' parameter");
      return;
    }
    
    String filename = server.arg("name");
    
    // Security: prevent directory traversal
    if (filename.indexOf("..") >= 0 || filename.indexOf("/") >= 0) {
      server.send(400, "text/plain", "Invalid filename");
      return;
    }
    
    // Build full path
    String fullPath = String(StorageConfig::PHOTO_DIR) + "/" + filename;
    
    // Check if file exists
    uint32_t fileSize = StorageManager::getFileSize(fullPath.c_str());
    if (fileSize == 0) {
      server.send(404, "text/plain", "Photo not found");
      return;
    }
    
    // Allocate buffer
    uint8_t* buffer = (uint8_t*)malloc(fileSize);
    if (!buffer) {
      server.send(500, "text/plain", "Memory allocation failed");
      return;
    }
    
    // Read file
    uint32_t bytesRead = StorageManager::readPhoto(fullPath.c_str(), buffer, fileSize);
    
    if (bytesRead != fileSize) {
      free(buffer);
      server.send(500, "text/plain", "Read error");
      return;
    }
    
    // Send file
    server.send_P(200, "image/jpeg", (const char*)buffer, fileSize);
    free(buffer);
  }
  
  /**
   * @brief Delete specific photo
   */
  static void handleDeletePhoto() {
    if (!server.hasArg("name")) {
      server.send(400, "text/plain", "Missing 'name' parameter");
      return;
    }
    
    String filename = server.arg("name");
    
    // Security check
    if (filename.indexOf("..") >= 0 || filename.indexOf("/") >= 0) {
      server.send(400, "text/plain", "Invalid filename");
      return;
    }
    
    // Build full path
    String fullPath = String(StorageConfig::PHOTO_DIR) + "/" + filename;
    
    // Delete file
    if (StorageManager::deletePhoto(fullPath.c_str())) {
      server.send(200, "text/plain", "Deleted");
    } else {
      server.send(500, "text/plain", "Delete failed");
    }
  }
};

// Static member initialization
::WebServer WebServer::server(WebConfig::HTTP_PORT);

#endif // WEB_SERVER_H
