//=============================================================================
// FILE: DEV_Config.cpp - Memory Optimized with Hardware DMA
//=============================================================================
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

//=============================================================================
// Hardware DMA transfer using ESP32's built-in DMA
//=============================================================================
void DEV_SPI_Write_DMA(const uint8_t *data, uint32_t len)
{
    if (len == 0) return;
    
    // Use 80MHz SPI with hardware DMA
    SPIlcd.beginTransaction(SPISettings(80000000, MSBFIRST, SPI_MODE3));
    
    // ESP32's writeBytes() uses DMA internally for large transfers
    SPIlcd.writeBytes(data, len);
    
    SPIlcd.endTransaction();
}

//=============================================================================
// BULK TRANSFER - Continuous mode (no CS toggling between chunks)
//=============================================================================
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