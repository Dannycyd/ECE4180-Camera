/**
 * @file camera_manager.h
 * @brief Camera capture and control using ArduCAM OV2640
 */

#ifndef CAMERA_MANAGER_H
#define CAMERA_MANAGER_H

#include "config.h"
#include <ArduCAM.h>
#include "memorysaver.h"

// Undefine swap macro conflict from ArduCAM
#ifdef swap
  #undef swap
#endif

#include <TJpg_Decoder.h>

class CameraManager {
public:
  static void init() {
    DEBUG_PRINTLN("→ Initializing Camera...");
    
    // Create ArduCAM instance
    camera = new ArduCAM(OV2640, Pins::CAM_CS);
    
    // Reset camera
    camera->write_reg(0x07, 0x80);
    delay(100);
    camera->write_reg(0x07, 0x00);
    delay(100);
    
    // Check SPI connection
    uint8_t vid, pid;
    camera->wrSensorReg8_8(0xff, 0x01);
    camera->rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    camera->rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
    
    if ((vid != 0x26) || (pid != 0x42)) {
      DEBUG_PRINTF("  ❌ Camera detection failed! VID=0x%02X PID=0x%02X\n", vid, pid);
      isAvailable = false;
      return;
    }
    
    DEBUG_PRINTF("  ✓ OV2640 detected (VID=0x%02X PID=0x%02X)\n", vid, pid);
    
    // Initialize camera
    camera->set_format(JPEG);
    camera->InitCAM();
    camera->OV2640_set_JPEG_size(OV2640_320x240);
    camera->clear_fifo_flag();
    
    isAvailable = true;
    DEBUG_PRINTLN("✓ Camera initialized");
  }
  
  /**
   * @brief Capture JPEG image to buffer
   * @return true if capture successful
   */
  static bool captureJPEG() {
    if (!isAvailable) return false;
    
    // Start capture
    camera->flush_fifo();
    camera->clear_fifo_flag();
    camera->start_capture();
    
    // Wait for capture done (timeout 1 second)
    uint32_t startTime = millis();
    while (!camera->get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)) {
      if (millis() - startTime > 1000) {
        DEBUG_PRINTLN("  ❌ Capture timeout");
        return false;
      }
      delay(1);
    }
    
    // Read JPEG size
    jpegLength = camera->read_fifo_length();
    
    if (jpegLength == 0 || jpegLength > CameraConfig::MAX_JPEG_SIZE) {
      DEBUG_PRINTF("  ❌ Invalid JPEG size: %d bytes\n", jpegLength);
      return false;
    }
    
    // Read JPEG data from FIFO
    camera->CS_LOW();
    camera->set_fifo_burst();
    
    for (uint32_t i = 0; i < jpegLength; i++) {
      jpegBuffer[i] = SPI.transfer(0x00);
    }
    
    camera->CS_HIGH();
    
    DEBUG_PRINTF("  ✓ Captured JPEG: %d bytes\n", jpegLength);
    return true;
  }
  
  /**
   * @brief Decode JPEG to RGB565 frame buffer
   * @return true if decode successful
   */
  static bool decodeToFrameBuffer() {
    if (jpegLength == 0) return false;
    
    // Clear frame buffer
    memset(frameBuffer, 0, CameraConfig::FRAME_BUFFER_SIZE);
    
    // Decode JPEG
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(false);
    TJpgDec.setCallback(tjpgCallback);
    
    uint16_t result = TJpgDec.drawJpg(0, 0, jpegBuffer, jpegLength);
    
    if (result != JDR_OK) {
      DEBUG_PRINTF("  ❌ JPEG decode failed: %d\n", result);
      return false;
    }
    
    return true;
  }
  
  /**
   * @brief Get pointer to JPEG buffer
   */
  static uint8_t* getJPEGBuffer() {
    return jpegBuffer;
  }
  
  /**
   * @brief Get JPEG data length
   */
  static uint32_t getJPEGLength() {
    return jpegLength;
  }
  
  /**
   * @brief Get pointer to RGB565 frame buffer
   */
  static uint8_t* getFrameBuffer() {
    return frameBuffer;
  }
  
  /**
   * @brief Check if camera is available
   */
  static bool available() {
    return isAvailable;
  }
  
  /**
   * @brief Set JPEG quality (0-8, 0=highest quality)
   */
  static void setQuality(uint8_t quality) {
    if (quality > 8) quality = 8;
    camera->OV2640_set_JPEG_size(OV2640_320x240);
  }
  
  /**
   * @brief Set special effects
   */
  static void setEffect(uint8_t effect) {
    // 0=Normal, 1=B&W, 2=Sepia, 3=Negative, etc.
    camera->OV2640_set_Special_effects(effect);
  }
  
private:
  static ArduCAM* camera;
  static bool isAvailable;
  
  // Buffers
  static uint8_t jpegBuffer[CameraConfig::MAX_JPEG_SIZE];
  static volatile uint32_t jpegLength;
  static uint8_t frameBuffer[CameraConfig::FRAME_BUFFER_SIZE];
  
  /**
   * @brief TJpgDec callback to write decoded pixels
   */
  static bool tjpgCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    // Write RGB565 pixels to frame buffer
    for (uint16_t row = 0; row < h; row++) {
      if ((y + row) >= CameraConfig::FRAME_HEIGHT) break;
      
      for (uint16_t col = 0; col < w; col++) {
        if ((x + col) >= CameraConfig::FRAME_WIDTH) break;
        
        // Calculate destination index
        uint32_t dstIdx = ((y + row) * CameraConfig::FRAME_WIDTH + (x + col)) * 2;
        
        // Get pixel (RGB565)
        uint16_t pixel = bitmap[row * w + col];
        
        // Store as big-endian
        frameBuffer[dstIdx] = pixel >> 8;
        frameBuffer[dstIdx + 1] = pixel & 0xFF;
      }
    }
    return true;
  }
};

// Static member initialization
ArduCAM* CameraManager::camera = nullptr;
bool CameraManager::isAvailable = false;
uint8_t CameraManager::jpegBuffer[CameraConfig::MAX_JPEG_SIZE];
volatile uint32_t CameraManager::jpegLength = 0;
uint8_t CameraManager::frameBuffer[CameraConfig::FRAME_BUFFER_SIZE];

#endif // CAMERA_MANAGER_H
