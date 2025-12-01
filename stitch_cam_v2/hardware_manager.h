/**
 * @file hardware_manager.h
 * @brief Hardware initialization and management
 */

#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include "config.h"
#include <Wire.h>
#include <SPI.h>

class HardwareManager {
public:
  static void init() {
    DEBUG_PRINTLN("→ Initializing Hardware...");
    
    initPins();
    initSPI();
    initI2C();
    initLED();
    
    DEBUG_PRINTLN("✓ Hardware initialized");
  }
  
private:
  static void initPins() {
    // Chip select pins
    pinMode(Pins::CAM_CS, OUTPUT);
    pinMode(Pins::SD_CS, OUTPUT);
    pinMode(Pins::LCD_CS, OUTPUT);
    
    // Set all CS high (inactive)
    digitalWrite(Pins::CAM_CS, HIGH);
    digitalWrite(Pins::SD_CS, HIGH);
    digitalWrite(Pins::LCD_CS, HIGH);
    
    // Buttons with pullup
    pinMode(Pins::BUTTON_CAPTURE, INPUT_PULLUP);
    pinMode(Pins::BUTTON_MODE, INPUT_PULLUP);
    
    DEBUG_PRINTLN("  ✓ GPIO pins configured");
  }
  
  static void initSPI() {
    // Camera SPI (HSPI)
    SPI.begin(Pins::CAM_SCK, Pins::CAM_MISO, Pins::CAM_MOSI);
    DEBUG_PRINTLN("  ✓ Camera SPI initialized");
  }
  
  static void initI2C() {
    Wire.begin(Pins::SDA, Pins::SCL);
    Wire.setClock(400000);  // 400kHz
    DEBUG_PRINTLN("  ✓ I2C initialized");
  }
  
  static void initLED() {
    pinMode(Pins::LED_RED, OUTPUT);
    pinMode(Pins::LED_GREEN, OUTPUT);
    pinMode(Pins::LED_BLUE, OUTPUT);
    
    // Configure LED PWM channels
    ledcSetup(0, 5000, 8);  // Channel 0, 5kHz, 8-bit
    ledcSetup(1, 5000, 8);  // Channel 1
    ledcSetup(2, 5000, 8);  // Channel 2
    
    ledcAttachPin(Pins::LED_RED, 0);
    ledcAttachPin(Pins::LED_GREEN, 1);
    ledcAttachPin(Pins::LED_BLUE, 2);
    
    setLED(LEDColor::OFF);
    DEBUG_PRINTLN("  ✓ RGB LED initialized");
  }
  
public:
  static void setLED(LEDColor color) {
    switch (color) {
      case LEDColor::OFF:
        ledcWrite(0, 0);
        ledcWrite(1, 0);
        ledcWrite(2, 0);
        break;
      case LEDColor::RED:
        ledcWrite(0, 255);
        ledcWrite(1, 0);
        ledcWrite(2, 0);
        break;
      case LEDColor::GREEN:
        ledcWrite(0, 0);
        ledcWrite(1, 255);
        ledcWrite(2, 0);
        break;
      case LEDColor::BLUE:
        ledcWrite(0, 0);
        ledcWrite(1, 0);
        ledcWrite(2, 255);
        break;
      case LEDColor::YELLOW:
        ledcWrite(0, 255);
        ledcWrite(1, 255);
        ledcWrite(2, 0);
        break;
      case LEDColor::CYAN:
        ledcWrite(0, 0);
        ledcWrite(1, 255);
        ledcWrite(2, 255);
        break;
      case LEDColor::MAGENTA:
        ledcWrite(0, 255);
        ledcWrite(1, 0);
        ledcWrite(2, 255);
        break;
      case LEDColor::WHITE:
        ledcWrite(0, 255);
        ledcWrite(1, 255);
        ledcWrite(2, 255);
        break;
    }
  }
};

#endif // HARDWARE_MANAGER_H
