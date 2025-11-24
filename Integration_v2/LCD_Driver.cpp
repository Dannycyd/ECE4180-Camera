//=============================================================================
// FILE: LCD_Driver.cpp - Fixed for separate SPI
//=============================================================================
#include "LCD_Driver.h"

// External LCD SPI instance from DEV_Config.cpp
extern SPIClass SPIlcd;

static void LCD_Reset(void)
{
    DEV_Digital_Write(DEV_CS_PIN, 1);
    DEV_Digital_Write(DEV_DC_PIN, 0);
    DEV_Delay_ms(10);

    DEV_Digital_Write(DEV_RST_PIN, 0);
    DEV_Delay_ms(120);

    DEV_Digital_Write(DEV_RST_PIN, 1);
    DEV_Delay_ms(150);
}

void LCD_SetBacklight(UWORD Value)
{
    if(Value > 100)
        Value=100;
    DEV_Set_BL(DEV_BL_PIN, (Value * 2.55));
}

void LCD_WriteData_Byte(UBYTE da) 
{ 
    DEV_Digital_Write(DEV_CS_PIN,0);
    DEV_Digital_Write(DEV_DC_PIN,1);
    
    SPIlcd.beginTransaction(SPISettings(60000000, MSBFIRST, SPI_MODE3));
    SPIlcd.transfer(da);
    SPIlcd.endTransaction();
    
    DEV_Digital_Write(DEV_CS_PIN,1);
}  

void LCD_WriteData_Word(UWORD da)
{
    UBYTE i=(da>>8)&0xff;
    DEV_Digital_Write(DEV_CS_PIN,0);
    DEV_Digital_Write(DEV_DC_PIN,1);
    
    SPIlcd.beginTransaction(SPISettings(60000000, MSBFIRST, SPI_MODE3));
    SPIlcd.transfer(i);
    SPIlcd.transfer(da);
    SPIlcd.endTransaction();
    
    DEV_Digital_Write(DEV_CS_PIN,1);
}   

void LCD_WriteReg(UBYTE da)  
{ 
    DEV_Digital_Write(DEV_CS_PIN,0);
    DEV_Digital_Write(DEV_DC_PIN,0);
    
    SPIlcd.beginTransaction(SPISettings(60000000, MSBFIRST, SPI_MODE3));
    SPIlcd.transfer(da);
    SPIlcd.endTransaction();
    
    DEV_Digital_Write(DEV_CS_PIN,1);
}

void LCD_Init(void)
{
    LCD_Reset();
    
    LCD_WriteReg(0x36);
    if (HORIZONTAL)
        LCD_WriteData_Byte(0x00);
    else
        LCD_WriteData_Byte(0x00);

    LCD_WriteReg(0x3A);
    LCD_WriteData_Byte(0x55);

    LCD_WriteReg(0xB2);
    LCD_WriteData_Byte(0x0C);
    LCD_WriteData_Byte(0x0C);
    LCD_WriteData_Byte(0x00);
    LCD_WriteData_Byte(0x33);
    LCD_WriteData_Byte(0x33);

    LCD_WriteReg(0xB7);
    LCD_WriteData_Byte(0x35);

    LCD_WriteReg(0xBB);
    LCD_WriteData_Byte(0x13);

    LCD_WriteReg(0xC0);
    LCD_WriteData_Byte(0x2C);

    LCD_WriteReg(0xC2);
    LCD_WriteData_Byte(0x01);

    LCD_WriteReg(0xC3);
    LCD_WriteData_Byte(0x0B);

    LCD_WriteReg(0xC4);
    LCD_WriteData_Byte(0x20);

    LCD_WriteReg(0xC6);
    LCD_WriteData_Byte(0x0F);

    LCD_WriteReg(0xD0);
    LCD_WriteData_Byte(0xA4);
    LCD_WriteData_Byte(0xA1);

    LCD_WriteReg(0xD6);
    LCD_WriteData_Byte(0xA1);

    LCD_WriteReg(0xE0);
    LCD_WriteData_Byte(0x00);
    LCD_WriteData_Byte(0x03);
    LCD_WriteData_Byte(0x07);
    LCD_WriteData_Byte(0x08);
    LCD_WriteData_Byte(0x07);
    LCD_WriteData_Byte(0x15);
    LCD_WriteData_Byte(0x2A);
    LCD_WriteData_Byte(0x44);
    LCD_WriteData_Byte(0x42);
    LCD_WriteData_Byte(0x0A);
    LCD_WriteData_Byte(0x17);
    LCD_WriteData_Byte(0x18);
    LCD_WriteData_Byte(0x25);
    LCD_WriteData_Byte(0x27);

    LCD_WriteReg(0xE1);
    LCD_WriteData_Byte(0x00);
    LCD_WriteData_Byte(0x03);
    LCD_WriteData_Byte(0x08);
    LCD_WriteData_Byte(0x07);
    LCD_WriteData_Byte(0x07);
    LCD_WriteData_Byte(0x23);
    LCD_WriteData_Byte(0x2A);
    LCD_WriteData_Byte(0x43);
    LCD_WriteData_Byte(0x42);
    LCD_WriteData_Byte(0x09);
    LCD_WriteData_Byte(0x18);
    LCD_WriteData_Byte(0x17);
    LCD_WriteData_Byte(0x25);
    LCD_WriteData_Byte(0x27);

    LCD_WriteReg(0x21);

    LCD_WriteReg(0x11);
    delay(120);
    LCD_WriteReg(0x29); 
} 

void LCD_SetCursor(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend)
{ 
    if (HORIZONTAL) {
        LCD_WriteReg(0x2A);
        LCD_WriteData_Byte(Xstart >> 8);
        LCD_WriteData_Byte(Xstart);
        LCD_WriteData_Byte(Xend >> 8);
        LCD_WriteData_Byte(Xend);
        
        LCD_WriteReg(0x2B);
        LCD_WriteData_Byte(Ystart >> 8);
        LCD_WriteData_Byte(Ystart);
        LCD_WriteData_Byte(Yend >> 8);
        LCD_WriteData_Byte(Yend);
    }
    else {
        LCD_WriteReg(0x2A);
        LCD_WriteData_Byte(Xstart >> 8);
        LCD_WriteData_Byte(Xstart);
        LCD_WriteData_Byte(Xend >> 8);
        LCD_WriteData_Byte(Xend);
        
        LCD_WriteReg(0x2B);
        LCD_WriteData_Byte(Ystart >> 8);
        LCD_WriteData_Byte(Ystart);
        LCD_WriteData_Byte(Yend >> 8);
        LCD_WriteData_Byte(Yend);
    }

    LCD_WriteReg(0x2C);
}

void LCD_Clear(UWORD Color)
{
    uint32_t totalPixels = (uint32_t)LCD_WIDTH * LCD_HEIGHT;
    uint32_t totalBytes = totalPixels * 2;
    
    LCD_SetCursor(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    
    DEV_Digital_Write(DEV_CS_PIN, 0);
    DEV_Digital_Write(DEV_DC_PIN, 1);
    
    UBYTE colorHigh = Color >> 8;
    UBYTE colorLow = Color & 0xFF;
    
#ifdef USE_DMA_TRANSFER
    uint8_t* dmaBuffer = getDMABuffer();
    for (uint16_t i = 0; i < DMA_BUFFER_SIZE; i += 2) {
        dmaBuffer[i] = colorHigh;
        dmaBuffer[i + 1] = colorLow;
    }
    
    uint32_t remaining = totalBytes;
    while (remaining > 0) {
        uint32_t chunkSize = (remaining > DMA_BUFFER_SIZE) ? DMA_BUFFER_SIZE : remaining;
        DEV_SPI_Write_DMA(dmaBuffer, chunkSize);
        remaining -= chunkSize;
    }
#else
    SPIlcd.beginTransaction(SPISettings(60000000, MSBFIRST, SPI_MODE3));
    for(uint32_t i = 0; i < totalPixels; i++) {
        SPIlcd.transfer(colorHigh);
        SPIlcd.transfer(colorLow);
    }
    SPIlcd.endTransaction();
#endif
    
    DEV_Digital_Write(DEV_CS_PIN, 1);
}

void LCD_ClearWindow(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD color)
{          
    uint32_t width = Xend - Xstart;
    uint32_t height = Yend - Ystart;
    uint32_t totalPixels = width * height;
    uint32_t totalBytes = totalPixels * 2;
    
    LCD_SetCursor(Xstart, Ystart, Xend - 1, Yend - 1);
    
    DEV_Digital_Write(DEV_CS_PIN, 0);
    DEV_Digital_Write(DEV_DC_PIN, 1);
    
    UBYTE colorHigh = color >> 8;
    UBYTE colorLow = color & 0xFF;
    
#ifdef USE_DMA_TRANSFER
    uint8_t* dmaBuffer = getDMABuffer();
    for (uint16_t i = 0; i < DMA_BUFFER_SIZE; i += 2) {
        dmaBuffer[i] = colorHigh;
        dmaBuffer[i + 1] = colorLow;
    }
    
    uint32_t remaining = totalBytes;
    while (remaining > 0) {
        uint32_t chunkSize = (remaining > DMA_BUFFER_SIZE) ? DMA_BUFFER_SIZE : remaining;
        DEV_SPI_Write_DMA(dmaBuffer, chunkSize);
        remaining -= chunkSize;
    }
#else
    SPIlcd.beginTransaction(SPISettings(60000000, MSBFIRST, SPI_MODE3));
    for(uint32_t i = 0; i < totalPixels; i++) {
        SPIlcd.transfer(colorHigh);
        SPIlcd.transfer(colorLow);
    }
    SPIlcd.endTransaction();
#endif
    
    DEV_Digital_Write(DEV_CS_PIN, 1);
}

void LCD_SetUWORD(UWORD x, UWORD y, UWORD Color)
{
    LCD_SetCursor(x, y, x, y);
    LCD_WriteData_Word(Color);      
}