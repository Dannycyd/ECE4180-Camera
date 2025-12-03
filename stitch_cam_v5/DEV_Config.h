/*****************************************************************************
* | File        :   DEV_Config.h
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*                Used to shield the underlying layers of each master 
*                and enhance portability
*                
*                ESP32-S3/C6 port with DMA optimization
*----------------
* | This version:   V2.0 (ESP32 port)
* | Date        :   2018-11-22 (Original)
* | Info        :   Modified for ESP32 with hardware DMA support
*
* NEW FUNCTIONS ADDED:
*   - DEV_SPI_Write_DMA() - Hardware DMA transfer for large data blocks
*   - DEV_SPI_Write_Bulk_Start() - Begin continuous bulk transfer mode
*   - DEV_SPI_Write_Bulk_Data() - Stream data in bulk mode (no CS toggling)
*   - DEV_SPI_Write_Bulk_End() - Complete bulk transfer mode
*   - getDMABuffer() - Access to aligned DMA buffer for efficient transfers
* OPTIMIZATIONS:
*   - Hardware DMA support using ESP32's built-in SPI DMA
*   - Bulk transfer mode (DEV_SPI_Write_Bulk_*) for continuous data streaming

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/

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


// DMA Configuration - MEMORY OPTIMIZED
#define USE_DMA_TRANSFER
#define DMA_BUFFER_SIZE 16384  // 16KB

// GPIO helpers
#define DEV_Digital_Write(_pin, _value) digitalWrite(_pin, (_value)?HIGH:LOW)
#define DEV_Digital_Read(_pin)          digitalRead(_pin)

// SPI write (single byte)
#define DEV_SPI_WRITE(_dat)  SPI.transfer(_dat)

// DMA SPI write functions
void DEV_SPI_Write_DMA(const uint8_t *data, uint32_t len);

// BULK transfer mode (fastest - no CS toggling)
void DEV_SPI_Write_Bulk_Start();
void DEV_SPI_Write_Bulk_Data(const uint8_t *data, uint32_t len);
void DEV_SPI_Write_Bulk_End();

#define DEV_Delay_ms(__xms) delay(__xms)

// PWM (ESP32 uses ledcWrite)
#define DEV_Set_BL(_Pin, _Value) ledcWrite(0, _Value)

void Config_Init();

// DMA buffer access
uint8_t* getDMABuffer();

#endif