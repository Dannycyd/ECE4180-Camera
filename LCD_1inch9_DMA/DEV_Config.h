//=============================================================================
// FILE: DEV_Config.h
//=============================================================================
#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include <stdint.h>
#include <stdio.h>
#include <SPI.h>
#include "Debug.h"
#include <Arduino.h>

// ESP32-C6 pin mapping
#define DEV_CS_PIN   2
#define DEV_DC_PIN   3
#define DEV_RST_PIN  10
#define DEV_BL_PIN   11

// ESP32-C6 SPI pins
#define DEV_SCK  6
#define DEV_MOSI 5
#define DEV_MISO -1

#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

//-----------------------------------------------
// DMA Configuration
//-----------------------------------------------
#define USE_DMA_TRANSFER
#define DMA_BUFFER_SIZE 8192  // 8KB buffer (4096 pixels)

//-----------------------------------------------
// GPIO helpers
//-----------------------------------------------
#define DEV_Digital_Write(_pin, _value) digitalWrite(_pin, (_value)?HIGH:LOW)
#define DEV_Digital_Read(_pin)          digitalRead(_pin)

//-----------------------------------------------
// SPI write (single byte)
//-----------------------------------------------
#define DEV_SPI_WRITE(_dat)  SPI.transfer(_dat)

//-----------------------------------------------
// DMA SPI write (bulk transfer)
//-----------------------------------------------
void DEV_SPI_Write_DMA(const uint8_t *data, uint32_t len);

//-----------------------------------------------
#define DEV_Delay_ms(__xms) delay(__xms)

//-----------------------------------------------
// PWM (ESP32 uses ledcWrite)
//-----------------------------------------------
#define DEV_Set_BL(_Pin, _Value) ledcWrite(0, _Value)

void Config_Init();

// DMA buffer access
uint8_t* getDMABuffer();

#endif