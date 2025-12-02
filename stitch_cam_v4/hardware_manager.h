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
    
    // Configure LED PWM using ESP32 Arduino 3.x API
    // ledcAttach(pin, freq, resolution) - new simplified API
    ledcAttach(Pins::LED_RED, 5000, 8);    // 5kHz, 8-bit
    ledcAttach(Pins::LED_GREEN, 5000, 8);
    ledcAttach(Pins::LED_BLUE, 5000, 8);
    
    setLED(LEDColor::OFF);
    DEBUG_PRINTLN("  ✓ RGB LED initialized");
  }
  
public:
  static void setLED(LEDColor color) {
    // ESP32 Arduino 3.x uses pin-based ledcWrite(pin, value)
    switch (color) {
      case LEDColor::OFF:
        ledcWrite(Pins::LED_RED, 0);
        ledcWrite(Pins::LED_GREEN, 0);
        ledcWrite(Pins::LED_BLUE, 0);
        break;
      case LEDColor::RED:
        ledcWrite(Pins::LED_RED, 255);
        ledcWrite(Pins::LED_GREEN, 0);
        ledcWrite(Pins::LED_BLUE, 0);
        break;
      case LEDColor::GREEN:
        ledcWrite(Pins::LED_RED, 0);
        ledcWrite(Pins::LED_GREEN, 255);
        ledcWrite(Pins::LED_BLUE, 0);
        break;
      case LEDColor::BLUE:
        ledcWrite(Pins::LED_RED, 0);
        ledcWrite(Pins::LED_GREEN, 0);
        ledcWrite(Pins::LED_BLUE, 255);
        break;
      case LEDColor::YELLOW:
        ledcWrite(Pins::LED_RED, 255);
        ledcWrite(Pins::LED_GREEN, 255);
        ledcWrite(Pins::LED_BLUE, 0);
        break;
      case LEDColor::CYAN:
        ledcWrite(Pins::LED_RED, 0);
        ledcWrite(Pins::LED_GREEN, 255);
        ledcWrite(Pins::LED_BLUE, 255);
        break;
      case LEDColor::MAGENTA:
        ledcWrite(Pins::LED_RED, 255);
        ledcWrite(Pins::LED_GREEN, 0);
        ledcWrite(Pins::LED_BLUE, 255);
        break;
      case LEDColor::WHITE:
        ledcWrite(Pins::LED_RED, 255);
        ledcWrite(Pins::LED_GREEN, 255);
        ledcWrite(Pins::LED_BLUE, 255);
        break;
    }
  }
};

#endif // HARDWARE_MANAGER_H
