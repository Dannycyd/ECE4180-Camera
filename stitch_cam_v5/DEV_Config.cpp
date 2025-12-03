/*****************************************************************************
* | File        :   DEV_Config.cpp
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
#include "DEV_Config.h"

// Single DMA buffer - 16KB (balanced memory/performance)
static uint8_t dmaBuffer[DMA_BUFFER_SIZE] __attribute__((aligned(4)));

// ESP32-C6: Use FSPI (the second SPI peripheral)
SPIClass SPIlcd(FSPI);

void GPIO_Init()
{
    pinMode(DEV_CS_PIN, OUTPUT);
    pinMode(DEV_RST_PIN, OUTPUT);
    pinMode(DEV_DC_PIN, OUTPUT);
    pinMode(DEV_BL_PIN, OUTPUT);

    digitalWrite(DEV_CS_PIN,  HIGH);
    digitalWrite(DEV_RST_PIN, HIGH);
    digitalWrite(DEV_DC_PIN,  HIGH);
    digitalWrite(DEV_BL_PIN,  HIGH);
}

void Config_Init()
{
    GPIO_Init();
    
    // ESP32-C6 LCD SPI initialization with high speed
    SPIlcd.begin(DEV_SCK, DEV_MISO, DEV_MOSI, DEV_CS_PIN);
    
    Serial.println("LCD Config Init - Memory Optimized DMA");
}

// Hardware DMA transfer using ESP32's built-in DMA
void DEV_SPI_Write_DMA(const uint8_t *data, uint32_t len)
{
    if (len == 0) return;
    
    // Use 80MHz SPI with hardware DMA
    SPIlcd.beginTransaction(SPISettings(80000000, MSBFIRST, SPI_MODE3));
    
    // ESP32's writeBytes() uses DMA internally for large transfers
    SPIlcd.writeBytes(data, len);
    
    SPIlcd.endTransaction();
}

// BULK TRANSFER - Continuous mode (no CS toggling between chunks)
static bool bulk_active = false;

void DEV_SPI_Write_Bulk_Start()
{
    DEV_Digital_Write(DEV_CS_PIN, 0);
    DEV_Digital_Write(DEV_DC_PIN, 1);
    SPIlcd.beginTransaction(SPISettings(80000000, MSBFIRST, SPI_MODE3));
    bulk_active = true;
}

void DEV_SPI_Write_Bulk_Data(const uint8_t *data, uint32_t len)
{
    if (len == 0 || !bulk_active) return;
    
    // Direct hardware DMA - no transaction overhead
    SPIlcd.writeBytes(data, len);
}

void DEV_SPI_Write_Bulk_End()
{
    if (bulk_active) {
        SPIlcd.endTransaction();
        DEV_Digital_Write(DEV_CS_PIN, 1);
        bulk_active = false;
    }
}

uint8_t* getDMABuffer()
{
    return dmaBuffer;
}