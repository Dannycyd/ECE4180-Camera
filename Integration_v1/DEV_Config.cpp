//=============================================================================
// FILE: DEV_Config.cpp
//=============================================================================
#include "DEV_Config.h"

// DMA buffer for bulk transfers
static uint8_t dmaBuffer[DMA_BUFFER_SIZE];

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
    Serial.begin(115200);
    delay(100);

    // ESP32-C6 SPI initialization
    SPI.begin(DEV_SCK, DEV_MISO, DEV_MOSI, DEV_CS_PIN);

    // REMOVED: Do NOT call beginTransaction() here!
    // Transactions should be started/ended for each operation, not globally
    
    Serial.println("LCD Config Init - DMA Enabled");
}

void DEV_SPI_Write_DMA(const uint8_t *data, uint32_t len)
{
    if (len == 0) return;
    
    // Begin transaction for each DMA write
    SPI.beginTransaction(SPISettings(60000000, MSBFIRST, SPI_MODE3));
    SPI.writeBytes(data, len);
    SPI.endTransaction();
}

uint8_t* getDMABuffer()
{
    return dmaBuffer;
}