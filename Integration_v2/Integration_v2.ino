/**************************************************************************
 * ESP32-C6 Camera LCD - MEMORY OPTIMIZED High Performance
 * 
 * Optimizations for LIMITED RAM:
 * Single frame buffer (saves 150KB)
 * Pre-rotated capture (rotation is "free")
 * 16KB DMA buffer (reduced from 32KB)
 * Zero-copy transfer
 * Hardware DMA
 * 
 * Expected: 12-18 FPS with ~180KB RAM usage
 * 
 * Memory breakdown:
 * - Frame buffer: 153KB (240×320×2)
 * - DMA buffer: 16KB
 * - Code + stack: ~20KB
 * Total: ~190KB (fits in 327KB limit!)
 **************************************************************************/

#include <Wire.h>
#include <SPI.h>
#include <ArduCAM.h>
#include "memorysaver.h"

#include "DEV_Config.h"
#include "LCD_Driver.h"
#include "GUI_Paint.h"

// Camera pins
static const int PIN_CAM_CS   = 13;
static const int PIN_CAM_SCK  = 10;
static const int PIN_CAM_MISO = 11;
static const int PIN_CAM_MOSI = 12;

// I2C pins
static const int PIN_SDA = 9;
static const int PIN_SCL = 8;

/*
// ESP32-C6 Definitions
// Camera pins
static const int PIN_CAM_CS   = 18;
static const int PIN_CAM_SCK  = 21;
static const int PIN_CAM_MISO = 20;
static const int PIN_CAM_MOSI = 19;

// I2C pins
static const int PIN_SDA = 22;
static const int PIN_SCL = 23;

*/

// Display dimensions
static const uint16_t LCD_W = 240;
static const uint16_t LCD_H = 320;
static const uint16_t CAM_W = 320;
static const uint16_t CAM_H = 240;
static const uint32_t FRAME_BYTES = LCD_W * LCD_H * 2;

// SINGLE frame buffer (saves 150KB!)
uint8_t frameBuffer[FRAME_BYTES] __attribute__((aligned(4)));

ArduCAM myCAM(OV2640, PIN_CAM_CS);

// Performance tracking
uint32_t frameCount = 0;
uint32_t lastStatsTime = 0;

void initCamera();
void captureFramePreRotated(uint8_t* buffer);
void streamFrameToLcd_Fast(const uint8_t *src);

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n╔═══════════════════════════════════════════════╗");
    Serial.println("║  ESP32-C6 Camera LCD - Memory Optimized       ║");
    Serial.println("║  Target: 12-18 FPS with low RAM usage         ║");
    Serial.println("╚═══════════════════════════════════════════════╝\n");

    Serial.printf("CPU Frequency: %d MHz\n", getCpuFrequencyMhz());
    Serial.printf("Frame Buffer: %d KB\n", FRAME_BYTES / 1024);
    Serial.printf("DMA Buffer: %d KB\n", DMA_BUFFER_SIZE / 1024);
    Serial.printf("Free Heap: %d KB\n\n", ESP.getFreeHeap() / 1024);

    // I2C init
    Serial.println("→ Init I2C...");
    Wire.begin(PIN_SDA, PIN_SCL);
    delay(100);

    // Camera SPI init
    Serial.println("→ Init Camera SPI...");
    SPI.begin(PIN_CAM_SCK, PIN_CAM_MISO, PIN_CAM_MOSI, PIN_CAM_CS);
    pinMode(PIN_CAM_CS, OUTPUT);
    digitalWrite(PIN_CAM_CS, HIGH);
    delay(100);

    // ArduCAM test
    Serial.println("→ Test ArduCAM...");
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    delay(1);
    uint8_t test = myCAM.read_reg(ARDUCHIP_TEST1);
    SPI.endTransaction();
    
    if (test != 0x55) {
        Serial.println("✗ ERROR: Camera SPI failed!");
        while (1) delay(1000);
    }
    Serial.println("✓ Camera SPI OK");

    // Camera I2C check
    Serial.println("→ Check OV2640...");
    Wire.beginTransmission(0x30);
    if (Wire.endTransmission() != 0) {
        Serial.println("✗ ERROR: Camera not found!");
        while (1) delay(1000);
    }
    Serial.println("✓ Camera found");

    // Camera init
    Serial.println("→ Init Camera...");
    initCamera();
    Serial.println("✓ Camera initialized");

    // LCD init
    Serial.println("→ Init LCD...");
    Config_Init();
    LCD_Init();
    LCD_SetBacklight(100);
    
    LCD_Clear(BLACK);
    Serial.println("✓ Display OK");

    Serial.println("\n╔═══════════════════════════════════════════════╗");
    Serial.println("║  STARTING OPTIMIZED LOOP                      ║");
    Serial.println("╚═══════════════════════════════════════════════╝\n");
    
    lastStatsTime = millis();
}

void loop() {
    uint32_t t0 = micros();
    
    // Capture with pre-rotation
    captureFramePreRotated(frameBuffer);
    uint32_t t1 = micros();
    
    // Display using fast DMA
    streamFrameToLcd_Fast(frameBuffer);
    uint32_t t2 = micros();
    
    frameCount++;
    
    // Print stats every second
    if (millis() - lastStatsTime >= 1000) {
        uint32_t captureTime = t1 - t0;
        uint32_t displayTime = t2 - t1;
        uint32_t totalTime = t2 - t0;
        float fps = 1000000.0 / totalTime;
        
        Serial.println("┌─────────────────────────────────────────────┐");
        Serial.printf("│ Capture: %6d µs (%5.1f ms)               │\n", 
                      captureTime, captureTime / 1000.0);
        Serial.printf("│ Display: %6d µs (%5.1f ms)               │\n", 
                      displayTime, displayTime / 1000.0);
        Serial.printf("│ Total:   %6d µs (%5.1f ms)               │\n", 
                      totalTime, totalTime / 1000.0);
        Serial.println("├─────────────────────────────────────────────┤");
        Serial.printf("│ FPS:     %5.2f                              │\n", fps);
        Serial.printf("│ Frames:  %6u                             │\n", frameCount);
        Serial.printf("│ Heap:    %6d KB                          │\n", 
                      ESP.getFreeHeap() / 1024);
        Serial.println("└─────────────────────────────────────────────┘\n");
        
        lastStatsTime = millis();
    }
}

void initCamera() {
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    
    myCAM.set_format(BMP);
    myCAM.InitCAM();
    myCAM.OV2640_set_JPEG_size(OV2640_320x240);
    
    SPI.endTransaction();
    delay(500);
    
    // Test capture
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    myCAM.flush_fifo();
    myCAM.clear_fifo_flag();
    myCAM.start_capture();
    SPI.endTransaction();
    
    uint32_t start = millis();
    while (millis() - start < 5000) {
        SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
        bool done = myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK);
        SPI.endTransaction();
        if (done) break;
        delay(10);
    }
    
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    myCAM.clear_fifo_flag();
    SPI.endTransaction();
}

//=============================================================================
// Pre-rotation during capture - rotation is "FREE"
// This is the KEY optimization for performance without extra RAM
//=============================================================================
void captureFramePreRotated(uint8_t* buffer) {
    // Trigger capture
    SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
    myCAM.flush_fifo();
    myCAM.clear_fifo_flag();
    myCAM.start_capture();
    SPI.endTransaction();
    
    // Wait for capture
    uint32_t start = micros();
    while (micros() - start < 5000000) {
        SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
        bool done = myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK);
        SPI.endTransaction();
        if (done) break;
        delayMicroseconds(100);
    }
    
    // Check FIFO
    SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
    uint32_t fifo_len = myCAM.read_fifo_length();
    SPI.endTransaction();
    
    if (fifo_len < CAM_W * CAM_H * 2) {
        Serial.println("ERROR: FIFO too small");
        return;
    }
    
    // Read and rotate in one pass
    // Camera: 320×240 landscape
    // LCD: 240×320 portrait
    // Rotation: 90° clockwise
    
    SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
    myCAM.CS_LOW();
    myCAM.set_fifo_burst();
    
    for (uint16_t camRow = 0; camRow < CAM_H; camRow++) {
        for (uint16_t camCol = 0; camCol < CAM_W; camCol++) {
            // Read pixel
            uint8_t b0 = SPI.transfer(0x00);
            uint8_t b1 = SPI.transfer(0x00);
            
            // Calculate rotated position (90° CW)
            uint16_t lcdX = camRow;
            uint16_t lcdY = CAM_W - 1 - camCol;
            
            // Store in pre-rotated buffer
            uint32_t dstIdx = (lcdY * LCD_W + lcdX) * 2;
            buffer[dstIdx] = b0;
            buffer[dstIdx + 1] = b1;
        }
    }
    
    // Discard extra bytes
    uint32_t expected = CAM_W * CAM_H * 2;
    if (fifo_len > expected) {
        for (uint32_t i = expected; i < fifo_len; i++) {
            SPI.transfer(0x00);
        }
    }
    
    myCAM.CS_HIGH();
    SPI.endTransaction();
    
    SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
    myCAM.clear_fifo_flag();
    SPI.endTransaction();
}

//=============================================================================
// Fast LCD transfer - buffer is already pre-rotated!
// Just dump to LCD using hardware DMA
//=============================================================================
void streamFrameToLcd_Fast(const uint8_t *src) {
    // Set LCD window
    LCD_SetCursor(0, 0, LCD_W - 1, LCD_H - 1);
    
    // Start bulk transfer (CS stays low)
    DEV_SPI_Write_Bulk_Start();
    
    // Send entire frame in chunks using zero-copy
    uint32_t remaining = FRAME_BYTES;
    uint32_t srcIdx = 0;
    
    while (remaining > 0) {
        uint32_t chunkSize = (remaining > DMA_BUFFER_SIZE) ? DMA_BUFFER_SIZE : remaining;
        
        // Zero-copy: direct pointer to source buffer
        DEV_SPI_Write_Bulk_Data(&src[srcIdx], chunkSize);
        
        srcIdx += chunkSize;
        remaining -= chunkSize;
    }
    
    DEV_SPI_Write_Bulk_End();
}
