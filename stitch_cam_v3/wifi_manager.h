/**
 * @file wifi_manager.h
 * @brief WiFi connection management
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "config.h"
#include <WiFi.h>

class WiFiManager {
public:
  static void init() {
    DEBUG_PRINTLN("→ Connecting to WiFi...");
    DEBUG_PRINTF("  SSID: %s\n", WIFI_SSID);
    
    // Set WiFi mode
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("StitchCam");
    
    // Start connection
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // Wait for connection with timeout
    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
      if (millis() - startTime > WIFI_TIMEOUT_MS) {
        DEBUG_PRINTLN("  ❌ WiFi connection timeout");
        isConnected = false;
        return;
      }
      delay(250);
      DEBUG_PRINT(".");
    }
    
    DEBUG_PRINTLN();
    isConnected = true;
    
    // Print connection details
    ipAddress = WiFi.localIP().toString();
    DEBUG_PRINTF("✓ WiFi connected!\n");
    DEBUG_PRINTF("  IP Address: %s\n", ipAddress.c_str());
    DEBUG_PRINTF("  Signal: %d dBm\n", WiFi.RSSI());
  }
  
  /**
   * @brief Check if WiFi is connected
   */
  static bool connected() {
    return WiFi.status() == WL_CONNECTED;
  }
  
  /**
   * @brief Get IP address as string
   */
  static String getIPAddress() {
    return ipAddress;
  }
  
  /**
   * @brief Get signal strength (RSSI)
   */
  static int32_t getSignalStrength() {
    return WiFi.RSSI();
  }
  
  /**
   * @brief Get MAC address
   */
  static String getMACAddress() {
    return WiFi.macAddress();
  }
  
  /**
   * @brief Attempt to reconnect if disconnected
   */
  static void maintainConnection() {
    if (!connected() && isConnected) {
      DEBUG_PRINTLN("⚠ WiFi disconnected, attempting reconnect...");
      WiFi.reconnect();
      
      uint32_t startTime = millis();
      while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > 10000) {  // 10 second timeout
          DEBUG_PRINTLN("  ❌ Reconnection failed");
          isConnected = false;
          return;
        }
        delay(100);
      }
      
      DEBUG_PRINTLN("  ✓ Reconnected");
      isConnected = true;
    }
  }
  
  /**
   * @brief Disconnect WiFi
   */
  static void disconnect() {
    WiFi.disconnect();
    isConnected = false;
    DEBUG_PRINTLN("WiFi disconnected");
  }
  
  /**
   * @brief Get connection status as string
   */
  static String getStatusString() {
    switch (WiFi.status()) {
      case WL_CONNECTED:       return "Connected";
      case WL_NO_SHIELD:       return "No Shield";
      case WL_IDLE_STATUS:     return "Idle";
      case WL_NO_SSID_AVAIL:   return "No SSID";
      case WL_SCAN_COMPLETED:  return "Scan Done";
      case WL_CONNECT_FAILED:  return "Failed";
      case WL_CONNECTION_LOST: return "Lost";
      case WL_DISCONNECTED:    return "Disconnected";
      default:                 return "Unknown";
    }
  }
  
  /**
   * @brief Get network info as JSON
   */
  static String getInfoJSON() {
    String json = "{";
    json += "\"connected\":" + String(connected() ? "true" : "false") + ",";
    json += "\"ip\":\"" + ipAddress + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"ssid\":\"" + String(WIFI_SSID) + "\",";
    json += "\"mac\":\"" + WiFi.macAddress() + "\"";
    json += "}";
    return json;
  }
  
private:
  static bool isConnected;
  static String ipAddress;
};

// Static member initialization
bool WiFiManager::isConnected = false;
String WiFiManager::ipAddress = "";

#endif // WIFI_MANAGER_H
