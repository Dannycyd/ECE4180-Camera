//=============================================================================
// FILE: LCD_Driver.h (Complete - 240×320 Version)
//=============================================================================
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