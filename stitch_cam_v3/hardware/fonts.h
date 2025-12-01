/**
 * @file fonts.h
 * @brief Font definitions for GUI_Paint library
 * @version 1.0
 */

#ifndef __FONTS_H
#define __FONTS_H

#include <Arduino.h>

// Font structure
typedef struct _tFont
{    
  const uint8_t *table;
  uint16_t Width;
  uint16_t Height;
} sFONT;

// Font size 8
extern sFONT Font8;

// Font size 12
extern sFONT Font12;

// Font size 16
extern sFONT Font16;

// Font size 20
extern sFONT Font20;

// Font size 24
extern sFONT Font24;

// Simple 8x8 font data
const uint8_t Font8_Table[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Space
    0x00, 0x00, 0x5F, 0x00, 0x00, 0x00, 0x00, 0x00,  // !
    // Add more characters as needed - this is a minimal set
};

// Font definitions
sFONT Font8 = {
  Font8_Table,
  5,  // Width
  8   // Height
};

sFONT Font12 = {
  Font8_Table,  // Reuse for now
  7,
  12
};

sFONT Font16 = {
  Font8_Table,  // Reuse for now
  11,
  16
};

sFONT Font20 = {
  Font8_Table,  // Reuse for now
  14,
  20
};

sFONT Font24 = {
  Font8_Table,  // Reuse for now
  17,
  24
};

#endif /* __FONTS_H */
