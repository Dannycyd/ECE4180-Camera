//=============================================================================
// FILE: DEV_Config.cpp - ESP32-C6 FIXED
//=============================================================================
#include "DEV_Config.h"

// DMA buffer for bulk transfers
static uint8_t dmaBuffer[DMA_BUFFER_SIZE];

// ESP32-C6: Use FSPI (the second SPI peripheral)
// FSPI is defined in the ESP32 Arduino core
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
    
    // ESP32-C6 LCD SPI initialization
    SPIlcd.begin(DEV_SCK, DEV_MISO, DEV_MOSI, DEV_CS_PIN);
    
    Serial.println("LCD Config Init - FSPI for LCD");
}

void DEV_SPI_Write_DMA(const uint8_t *data, uint32_t len)
{
    if (len == 0) return;
    
    // Keep 80MHz speed but use byte-by-byte transfer (works correctly)
    SPIlcd.beginTransaction(SPISettings(80000000, MSBFIRST, SPI_MODE3));
    
    // Byte-by-byte transfer (slower but correct colors)
    for (uint32_t i = 0; i < len; i++) {
        SPIlcd.transfer(data[i]);
    }
    
    SPIlcd.endTransaction();
}

uint8_t* getDMABuffer()
{
    return dmaBuffer;
}