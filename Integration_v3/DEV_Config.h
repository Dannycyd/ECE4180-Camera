//=============================================================================
// FILE: DEV_Config.h - Memory Optimized
//=============================================================================
#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include <stdint.h>
#include <stdio.h>
#include <SPI.h>
#include "Debug.h"
#include <Arduino.h>

// ESP32-S3 pin mapping
#define DEV_CS_PIN   17
#define DEV_DC_PIN   18
#define DEV_RST_PIN  15
#define DEV_BL_PIN   16

// ESP32-S3 SPI pins
#define DEV_SCK  5
#define DEV_MOSI 4
#define DEV_MISO -1

#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

//-----------------------------------------------
// DMA Configuration - MEMORY OPTIMIZED
//-----------------------------------------------
#define USE_DMA_TRANSFER
#define DMA_BUFFER_SIZE 16384  // 16KB - balanced performance/memory

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
// DMA SPI write functions
//-----------------------------------------------
void DEV_SPI_Write_DMA(const uint8_t *data, uint32_t len);

//-----------------------------------------------
// BULK transfer mode (fastest - no CS toggling)
//-----------------------------------------------
void DEV_SPI_Write_Bulk_Start();
void DEV_SPI_Write_Bulk_Data(const uint8_t *data, uint32_t len);
void DEV_SPI_Write_Bulk_End();

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