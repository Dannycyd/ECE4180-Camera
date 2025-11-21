/*****************************************************************************
* Optimized Main Sketch - DMA Accelerated
******************************************************************************/
#include <SPI.h>
#include "LCD_Driver.h"
#include "GUI_Paint.h"
#include "image.h"

void setup()
{
    // Start serial for performance monitoring
    Serial.begin(115200);
    delay(100);
    
    Serial.println("\n=== LCD Initialization ===");
    
    delay(800);
    Config_Init();
    delay(400);
    LCD_Init();
    delay(200);

    LCD_SetBacklight(100);

    Paint_NewImage(LCD_WIDTH, LCD_HEIGHT, 0, WHITE);
    
    Serial.println("Clearing screen...");
    unsigned long start = millis();
    Paint_Clear(WHITE);
    unsigned long elapsed = millis() - start;
    Serial.printf("Clear time: %lu ms\n", elapsed);
    
    Serial.println("\nSetup complete!");
    Serial.printf("Free RAM: %u bytes\n", ESP.getFreeHeap());
    Serial.println("Expected image draw time: 50-100ms");
    Serial.println("-----------------------------------\n");
}

void loop()
{
    // Draw image with performance monitoring
    Serial.println("Drawing image...");
    unsigned long start = millis();
    
    Paint_DrawImage(gImage_170X320_JL, 0, 0, 170, 320);
    
    unsigned long elapsed = millis() - start;
    Serial.printf("✓ Image displayed in %lu ms\n", elapsed);
    
    if (elapsed < 100) {
        Serial.println("  Status: Excellent! DMA is working.");
    } else if (elapsed < 500) {
        Serial.println("  Status: Good, but could be faster.");
    } else {
        Serial.println("  WARNING: Slow! Check if DMA is enabled.");
    }
    
    delay(3000);
    
    // Clear screen with performance monitoring
    Serial.println("Clearing screen...");
    start = millis();
    
    Paint_Clear(WHITE);
    
    elapsed = millis() - start;
    Serial.printf("✓ Screen cleared in %lu ms\n\n", elapsed);
    
    delay(1000);
}

/*********************************************************************************************************
  END FILE
*********************************************************************************************************/