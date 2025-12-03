/*****************************************************************************
* | File        :   LCD_Driver.h
* | Author      :   Waveshare team
* | Function    :   LCD driver
* | Info        :
*----------------
* | This version:   V2.0 (ESP32 port)
* | Date        :   2018-12-18 (Original)
* | Info        :   Modified for ESP32 with DMA-accelerated display operations
*
* ESP32 PORT ADDITIONS:
* ---------------------
* - Hardware DMA-accelerated clear operations (LCD_Clear, LCD_ClearWindow)
* - Memory-optimized buffer filling using DMA chunks
* - Direct integration with DEV_SPI_Write_DMA() for bulk transfers
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documnetation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to  whom the Software is
* furished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
******************************************************************************/
#ifndef __LCD_DRIVER_H
#define __LCD_DRIVER_H

#include "DEV_Config.h"

#define LCD_WIDTH   240
#define LCD_HEIGHT  320

#define HORIZONTAL 0
#define VERTICAL   1

void LCD_WriteData_Byte(UBYTE da); 
void LCD_WriteData_Word(UWORD da);
void LCD_WriteReg(UBYTE da);

void LCD_SetCursor(UWORD x1, UWORD y1, UWORD x2, UWORD y2);
void LCD_SetUWORD(UWORD x, UWORD y, UWORD Color);

void LCD_Init(void);
void LCD_SetBacklight(UWORD Value);
void LCD_Clear(UWORD Color);
void LCD_ClearWindow(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD color);

#endif