/**************************************************************************
 * ESP32-S3 Camera LCD - JPEG MODE with SD Card
 * 
 * SD Card and Camera share the same SPI bus
 * Different CS pins for each device
 **************************************************************************/

#include <Wire.h>
#include <SPI.h>
#include <SD.h>

// Include ArduCAM first
#include <ArduCAM.h>
#include "memorysaver.h"

// CRITICAL FIX: Undefine the problematic swap macro from ArduCAM
#ifdef swap
    #undef swap
#endif

// Now safely include TJpg_Decoder and other C++ headers
#include <TJpg_Decoder.h>

#include "DEV_Config.h"
#include "LCD_Driver.h"
#include "GUI_Paint.h"

// Shared SPI bus pins
static const int PIN_SPI_SCK  = 10;
static const int PIN_SPI_MISO = 11;
static const int PIN_SPI_MOSI = 12;

// SD Card pins (shares SPI bus)
static const int PIN_SD_CS    = 14;

// Camera pins (shares SPI bus)
static const int PIN_CAM_CS   = 13;

// I2C pins
static const int PIN_SDA = 9;
static const int PIN_SCL = 8;

// JPEG buffer
#define MAX_JPEG_SIZE 32768
uint8_t jpegBuf[MAX_JPEG_SIZE];
uint32_t jpegLen = 0;

// RGB565 frame buffer
static const uint16_t FRAME_W = 320;
static const uint16_t FRAME_H = 240;
static const uint32_t FRAME_BYTES = FRAME_W * FRAME_H * 2;
uint8_t frameBuf[FRAME_BYTES];

ArduCAM myCAM(OV2640, PIN_CAM_CS);

// SD card status
bool sdCardAvailable = false;
uint32_t photoCount = 0;

void initCamera();
void initSDCard();
bool captureJpegToBuffer();
bool decodeJpegToRGB565();
void streamFrameToLcd_Optimized(const uint8_t *src);
bool saveJpegToSD();

// TJpgDec callback
bool tjpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    for (uint16_t row = 0; row < h; row++) {
        if ((y + row) >= FRAME_H) break;
        
        for (uint16_t col = 0; col < w; col++) {
            if ((x + col) >= FRAME_W) break;
            
            uint32_t dstIdx = ((y + row) * FRAME_W + (x + col)) * 2;
            uint16_t pixel = bitmap[row * w + col];
            
            frameBuf[dstIdx] = pixel >> 8;
            frameBuf[dstIdx + 1] = pixel & 0xFF;
        }
    }
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=== ESP32-S3 Camera LCD + SD Card - JPEG MODE ===");
    Serial.println("Swap macro conflict: FIXED\n");

    // Initialize CS pins (HIGH = inactive)
    pinMode(PIN_SD_CS, OUTPUT);
    pinMode(PIN_CAM_CS, OUTPUT);
    digitalWrite(PIN_SD_CS, HIGH);
    digitalWrite(PIN_CAM_CS, HIGH);
    
    // I2C init
    Serial.println("1. Init I2C...");
    Wire.begin(PIN_SDA, PIN_SCL);
    delay(100);
    Serial.println("   OK");

    // Shared SPI init
    Serial.println("2. Init Shared SPI Bus...");
    Serial.printf("   SCK:  %d\n", PIN_SPI_SCK);
    Serial.printf("   MISO: %d\n", PIN_SPI_MISO);
    Serial.printf("   MOSI: %d\n", PIN_SPI_MOSI);
    SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
    delay(100);
    Serial.println("   OK");

    // SD Card init (try first, before camera)
    Serial.println("3. Init SD Card...");
    Serial.printf("   CS: %d\n", PIN_SD_CS);
    initSDCard();

    // ArduCAM test
    Serial.println("4. Test ArduCAM...");
    Serial.printf("   CS: %d\n", PIN_CAM_CS);
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    delay(1);
    uint8_t test = myCAM.read_reg(ARDUCHIP_TEST1);
    SPI.endTransaction();
    
    if (test != 0x55) {
        Serial.printf("   ERROR: Expected 0x55, got 0x%02X\n", test);
        Serial.println("   Check camera wiring and pin configuration!");
        while (1) delay(1000);
    }
    Serial.println("   OK");

    // I2C camera check
    Serial.println("5. Check OV2640...");
    Wire.beginTransmission(0x30);
    if (Wire.endTransmission() != 0) {
        Serial.println("   ERROR: Camera not found on I2C!");
        while (1) delay(1000);
    }
    Serial.println("   OK");

    // Camera init
    Serial.println("6. Init Camera (JPEG)...");
    initCamera();

    // LCD init
    Serial.println("7. Init LCD...");
    Config_Init();
    LCD_Init();
    LCD_SetBacklight(100);
    LCD_Clear(BLACK);
    Serial.println("   OK");

    // JPEG decoder init
    Serial.println("8. Init JPEG Decoder...");
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(false);
    TJpgDec.setCallback(tjpgOutput);
    Serial.println("   OK");

    Serial.println("\n=== STARTING CAPTURE ===\n");
    if (sdCardAvailable) {
        Serial.println("SD Card: READY - Photos will be saved");
    } else {
        Serial.println("SD Card: NOT AVAILABLE - Display only");
    }
    Serial.println();
    delay(1000);
}

void loop() {
    static uint32_t frameCount = 0;
    static uint32_t lastPrintTime = 0;
    static float avgFps = 0;
    
    uint32_t t0 = millis();
    
    // Capture JPEG
    if (!captureJpegToBuffer()) {
        Serial.println("Capture failed!");
        delay(1000);
        return;
    }
    uint32_t t1 = millis();
    
    // Decode JPEG
    if (!decodeJpegToRGB565()) {
        Serial.println("Decode failed!");
        delay(1000);
        return;
    }
    uint32_t t2 = millis();
    
    // Display on LCD
    streamFrameToLcd_Optimized(frameBuf);
    uint32_t t3 = millis();
    
    // Save to SD card every 30 frames (optional)
    if (sdCardAvailable && (frameCount % 30 == 0)) {
        uint32_t tSD0 = millis();
        if (saveJpegToSD()) {
            uint32_t tSD1 = millis();
            Serial.printf("   Saved photo_%04d.jpg (%d bytes, %dms)\n", 
                         photoCount, jpegLen, tSD1 - tSD0);
        }
    }
    
    // Calculate FPS
    float fps = 1000.0 / (t3 - t0);
    avgFps = (avgFps * frameCount + fps) / (frameCount + 1);
    frameCount++;
    
    // Print stats every second
    if (millis() - lastPrintTime > 1000) {
        lastPrintTime = millis();
        Serial.printf("Frame:%d | Capture:%dms | JPEG:%db | Decode:%dms | LCD:%dms | FPS:%.1f (avg:%.1f)\n",
                      frameCount,
                      t1 - t0,
                      jpegLen,
                      t2 - t1,
                      t3 - t2,
                      fps,
                      avgFps);
    }
}

void initSDCard() {
    // Make sure camera CS is HIGH before initializing SD
    digitalWrite(PIN_CAM_CS, HIGH);
    
    // Try to initialize SD card
    if (!SD.begin(PIN_SD_CS)) {
        Serial.println("   WARNING: SD Card initialization failed!");
        Serial.println("   Possible reasons:");
        Serial.println("   - No SD card inserted");
        Serial.println("   - Card not formatted (use FAT32)");
        Serial.println("   - Wrong wiring");
        Serial.println("   - Bad SD card");
        Serial.println("   Continuing without SD card...");
        sdCardAvailable = false;
        return;
    }
    
    // Check card type
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("   WARNING: No SD card attached");
        sdCardAvailable = false;
        return;
    }
    
    // Print card info
    Serial.print("   Card Type: ");
    switch(cardType) {
        case CARD_MMC:  Serial.println("MMC"); break;
        case CARD_SD:   Serial.println("SDSC"); break;
        case CARD_SDHC: Serial.println("SDHC"); break;
        default:        Serial.println("UNKNOWN"); break;
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("   Card Size: %llu MB\n", cardSize);
    
    uint64_t totalBytes = SD.totalBytes() / (1024 * 1024);
    uint64_t usedBytes = SD.usedBytes() / (1024 * 1024);
    Serial.printf("   Total: %llu MB\n", totalBytes);
    Serial.printf("   Used:  %llu MB\n", usedBytes);
    Serial.printf("   Free:  %llu MB\n", totalBytes - usedBytes);
    
    // Create photos directory if it doesn't exist
    if (!SD.exists("/photos")) {
        Serial.println("   Creating /photos directory...");
        if (SD.mkdir("/photos")) {
            Serial.println("   Directory created");
        } else {
            Serial.println("   Failed to create directory");
        }
    }
    
    sdCardAvailable = true;
    Serial.println("   SD Card: READY");
}

void initCamera() {
    // Make sure SD card CS is HIGH before using camera
    digitalWrite(PIN_SD_CS, HIGH);
    
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    
    myCAM.set_format(JPEG);
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
    uint32_t len = myCAM.read_fifo_length();
    myCAM.clear_fifo_flag();
    SPI.endTransaction();
    
    Serial.printf("   Test capture: %d bytes\n", len);
    if (len > 0 && len < MAX_JPEG_SIZE) {
        Serial.println("   Camera OK!");
    } else {
        Serial.println("   WARNING: Unexpected size");
    }
}

bool captureJpegToBuffer() {
    // Make sure SD card CS is HIGH
    digitalWrite(PIN_SD_CS, HIGH);
    
    SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    myCAM.flush_fifo();
    myCAM.clear_fifo_flag();
    myCAM.start_capture();
    SPI.endTransaction();
    
    uint32_t start = millis();
    while (millis() - start < 5000) {
        SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
        bool done = myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK);
        SPI.endTransaction();
        if (done) break;
        delay(5);
    }
    
    SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    jpegLen = myCAM.read_fifo_length();
    SPI.endTransaction();
    
    if (jpegLen == 0 || jpegLen > MAX_JPEG_SIZE) {
        return false;
    }
    
    SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
    myCAM.CS_LOW();
    myCAM.set_fifo_burst();
    
    for (uint32_t i = 0; i < jpegLen; i++) {
        jpegBuf[i] = SPI.transfer(0x00);
    }
    
    myCAM.CS_HIGH();
    SPI.endTransaction();
    
    SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    myCAM.clear_fifo_flag();
    SPI.endTransaction();
    
    return (jpegBuf[0] == 0xFF && jpegBuf[1] == 0xD8);
}

bool saveJpegToSD() {
    if (!sdCardAvailable) {
        return false;
    }
    
    // Make sure camera CS is HIGH
    digitalWrite(PIN_CAM_CS, HIGH);
    
    // Create filename
    char filename[32];
    sprintf(filename, "/photos/photo_%04d.jpg", photoCount);
    
    // Open file for writing
    File file = SD.open(filename, FILE_WRITE);
    if (!file) {
        Serial.printf("   ERROR: Failed to open %s for writing\n", filename);
        return false;
    }
    
    // Write JPEG data
    size_t written = file.write(jpegBuf, jpegLen);
    file.close();
    
    if (written != jpegLen) {
        Serial.printf("   ERROR: Only wrote %d/%d bytes\n", written, jpegLen);
        return false;
    }
    
    photoCount++;
    return true;
}

bool decodeJpegToRGB565() {
    memset(frameBuf, 0, FRAME_BYTES);
    
    uint16_t w, h;
    if (TJpgDec.getJpgSize(&w, &h, jpegBuf, jpegLen) != JDR_OK) {
        return false;
    }
    
    return (TJpgDec.drawJpg(0, 0, jpegBuf, jpegLen) == JDR_OK);
}

void streamFrameToLcd_Optimized(const uint8_t *src) {
    uint8_t *dmaBuf = getDMABuffer();
    const uint16_t LCD_W = 240;
    const uint16_t LCD_H = 320;
    
    LCD_SetCursor(0, 0, LCD_W - 1, LCD_H - 1);
    DEV_SPI_Write_Bulk_Start();
    
    for (uint16_t lcdRow = 0; lcdRow < LCD_H; lcdRow++) {
        uint16_t bufIdx = 0;
        
        for (uint16_t lcdCol = 0; lcdCol < LCD_W; lcdCol++) {
            uint16_t srcRow = LCD_W - 1 - lcdCol;
            uint16_t srcCol = lcdRow;
            uint32_t srcIdx = (srcRow * FRAME_W + srcCol) * 2;
            
            dmaBuf[bufIdx++] = src[srcIdx];
            dmaBuf[bufIdx++] = src[srcIdx + 1];
            
            if (bufIdx >= DMA_BUFFER_SIZE) {
                DEV_SPI_Write_Bulk_Data(dmaBuf, bufIdx);
                bufIdx = 0;
            }
        }
        
        if (bufIdx > 0) {
            DEV_SPI_Write_Bulk_Data(dmaBuf, bufIdx);
        }
    }
    
    DEV_SPI_Write_Bulk_End();
}
