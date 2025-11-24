/**************************************************************************
 * ESP32-C6 Camera LCD - Full Debug Version
 **************************************************************************/

#include <Wire.h>
#include <SPI.h>
#include <ArduCAM.h>
#include "memorysaver.h"

#include "DEV_Config.h"
#include "LCD_Driver.h"
#include "GUI_Paint.h"

// Camera pins (your current setup)
static const int PIN_CAM_CS   = 18;
static const int PIN_CAM_SCK  = 21;
static const int PIN_CAM_MISO = 20;
static const int PIN_CAM_MOSI = 19;

// I2C pins
static const int PIN_SDA = 22;
static const int PIN_SCL = 23;

static const uint16_t SRC_W = 320;
static const uint16_t SRC_H = 240;
static const uint32_t FRAME_BYTES = SRC_W * SRC_H * 2;

uint8_t frameBuf[FRAME_BYTES];

ArduCAM myCAM(OV2640, PIN_CAM_CS);

void initCamera();
void captureFrameToBuffer();
void streamFrameToLcd(const uint8_t *src);

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n=== ESP32-C6 Camera LCD DEBUG ===\n");

    // I2C init
    Serial.println("1. Init I2C...");
    Wire.begin(PIN_SDA, PIN_SCL);
    delay(100);
    Serial.println("   OK");

    // Camera SPI init
    Serial.println("2. Init Camera SPI...");
    Serial.print("   Pins: CS=");
    Serial.print(PIN_CAM_CS);
    Serial.print(", SCK=");
    Serial.print(PIN_CAM_SCK);
    Serial.print(", MISO=");
    Serial.print(PIN_CAM_MISO);
    Serial.print(", MOSI=");
    Serial.println(PIN_CAM_MOSI);
    
    SPI.begin(PIN_CAM_SCK, PIN_CAM_MISO, PIN_CAM_MOSI, PIN_CAM_CS);
    pinMode(PIN_CAM_CS, OUTPUT);
    digitalWrite(PIN_CAM_CS, HIGH);
    delay(100);
    Serial.println("   OK");

    // ArduCAM SPI test
    Serial.println("3. Test ArduCAM SPI...");
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    delay(1);
    uint8_t test = myCAM.read_reg(ARDUCHIP_TEST1);
    SPI.endTransaction();
    
    Serial.print("   Expected 0x55, got 0x");
    Serial.println(test, HEX);
    
    if (test != 0x55) {
        Serial.println("   ERROR: SPI FAILED!");
        while (1) delay(1000);
    }
    Serial.println("   OK");

    // I2C camera check
    Serial.println("4. Check OV2640 I2C...");
    Wire.beginTransmission(0x30);
    if (Wire.endTransmission() != 0) {
        Serial.println("   ERROR: Camera not found!");
        while (1) delay(1000);
    }
    Serial.println("   OK");

    // Camera init
    Serial.println("5. Init Camera...");
    initCamera();

    // LCD init
    Serial.println("6. Init LCD...");
    Config_Init();
    LCD_Init();
    LCD_SetBacklight(100);
    
    // Test LCD with colors
    Serial.println("   Testing LCD colors...");
    LCD_Clear(RED);
    delay(300);
    LCD_Clear(GREEN);
    delay(300);
    LCD_Clear(BLUE);
    delay(300);
    LCD_Clear(BLACK);
    Serial.println("   OK");

    Serial.println("\n=== STARTING CAPTURE LOOP ===\n");
    delay(1000);
}

void loop() {
    Serial.println("--- New Frame ---");
    
    uint32_t t0 = millis();
    captureFrameToBuffer();
    uint32_t t1 = millis();

    streamFrameToLcd(frameBuf);
    uint32_t t2 = millis();

    Serial.print("Capture: ");
    Serial.print(t1 - t0);
    Serial.print("ms, LCD: ");
    Serial.print(t2 - t1);
    Serial.print("ms, FPS: ");
    Serial.println(1000.0 / (t2 - t0), 1);
    
    // Debug: Show what we're actually sending to LCD
    Serial.print("Sending to LCD - First pixel bytes: ");
    Serial.print(frameBuf[0], HEX);
    Serial.print(" ");
    Serial.print(frameBuf[1], HEX);
    Serial.print(", RGB565 value: 0x");
    uint16_t rgb = (frameBuf[0] << 8) | frameBuf[1];
    Serial.println(rgb, HEX);
    Serial.println();

    delay(100);
}

void initCamera() {
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    
    myCAM.set_format(BMP);
    Serial.println("   Format: BMP");
    
    myCAM.InitCAM();
    Serial.println("   InitCAM done");
    
    myCAM.OV2640_set_JPEG_size(OV2640_320x240);
    Serial.println("   Size: 320x240");
    
    SPI.endTransaction();
    delay(500);
    
    // Test capture
    Serial.println("   Test capture...");
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    myCAM.flush_fifo();
    myCAM.clear_fifo_flag();
    myCAM.start_capture();
    SPI.endTransaction();
    
    uint32_t start = millis();
    while (true) {
        SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
        bool done = myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK);
        SPI.endTransaction();
        
        if (done) break;
        
        if (millis() - start > 5000) {
            Serial.println("   ERROR: Test capture timeout!");
            return;
        }
        delay(10);
    }
    
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    uint32_t fifo_len = myCAM.read_fifo_length();
    SPI.endTransaction();
    
    Serial.print("   Test FIFO: ");
    Serial.print(fifo_len);
    Serial.print(" bytes (need ");
    Serial.print(FRAME_BYTES);
    Serial.println(")");
    
    if (fifo_len >= FRAME_BYTES) {
        Serial.println("   SUCCESS!");
        
        // Read first few bytes to verify
        SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
        myCAM.CS_LOW();
        myCAM.set_fifo_burst();
        
        Serial.print("   First 16 bytes: ");
        for (int i = 0; i < 16; i++) {
            uint8_t b = SPI.transfer(0x00);
            if (b < 0x10) Serial.print("0");
            Serial.print(b, HEX);
            Serial.print(" ");
        }
        Serial.println();
        
        myCAM.CS_HIGH();
        SPI.endTransaction();
    } else {
        Serial.println("   ERROR: FIFO too small!");
    }
    
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    myCAM.clear_fifo_flag();
    SPI.endTransaction();
}

void captureFrameToBuffer() {
    // Trigger capture
    SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    myCAM.flush_fifo();
    myCAM.clear_fifo_flag();
    myCAM.start_capture();
    SPI.endTransaction();
    
    // Wait for capture
    uint32_t start = millis();
    while (true) {
        SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
        bool done = myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK);
        SPI.endTransaction();
        
        if (done) {
            Serial.print("Capture time: ");
            Serial.print(millis() - start);
            Serial.println("ms");
            break;
        }
        
        if (millis() - start > 5000) {
            Serial.println("ERROR: Capture TIMEOUT!");
            return;
        }
        delay(5);
    }
    
    // Read FIFO
    SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    uint32_t fifo_len = myCAM.read_fifo_length();
    
    Serial.print("FIFO length: ");
    Serial.print(fifo_len);
    
    if (fifo_len < FRAME_BYTES) {
        Serial.println(" (TOO SMALL!)");
        SPI.endTransaction();
        myCAM.clear_fifo_flag();
        return;
    }
    Serial.println(" (OK)");
    SPI.endTransaction();
    
    // Read FIFO byte-by-byte
    uint32_t read_start = millis();
    
    SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));
    myCAM.CS_LOW();
    myCAM.set_fifo_burst();
    
    // Read frame data byte-by-byte
    for (uint32_t i = 0; i < FRAME_BYTES; i++) {
        frameBuf[i] = SPI.transfer(0x00);
    }
    
    // Discard extra bytes
    if (fifo_len > FRAME_BYTES) {
        for (uint32_t i = FRAME_BYTES; i < fifo_len; i++) {
            SPI.transfer(0x00);
        }
    }
    
    myCAM.CS_HIGH();
    SPI.endTransaction();
    
    uint32_t read_time = millis() - read_start;
    Serial.print("FIFO read: ");
    Serial.print(read_time);
    Serial.println("ms");
    
    // Show sample pixels
    Serial.print("Sample pixels - First: 0x");
    if (frameBuf[0] < 0x10) Serial.print("0");
    Serial.print(frameBuf[0], HEX);
    if (frameBuf[1] < 0x10) Serial.print("0");
    Serial.print(frameBuf[1], HEX);
    
    Serial.print(", Mid: 0x");
    if (frameBuf[FRAME_BYTES/2] < 0x10) Serial.print("0");
    Serial.print(frameBuf[FRAME_BYTES/2], HEX);
    if (frameBuf[FRAME_BYTES/2+1] < 0x10) Serial.print("0");
    Serial.print(frameBuf[FRAME_BYTES/2+1], HEX);
    
    Serial.print(", Last: 0x");
    if (frameBuf[FRAME_BYTES-2] < 0x10) Serial.print("0");
    Serial.print(frameBuf[FRAME_BYTES-2], HEX);
    if (frameBuf[FRAME_BYTES-1] < 0x10) Serial.print("0");
    Serial.println(frameBuf[FRAME_BYTES-1], HEX);
    
    SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    myCAM.clear_fifo_flag();
    SPI.endTransaction();
}

void streamFrameToLcd(const uint8_t *src) {
    uint8_t *dmaBuf = getDMABuffer();
    
    // Camera is 320×240, LCD is 240×320
    // Send all 320 rows of camera data (each row is 240 pixels)
    
    Serial.println("Setting LCD window to 240x320...");
    LCD_SetCursor(0, 0, 239, 319);  // Full LCD height
    
    DEV_Digital_Write(DEV_CS_PIN, 0);
    DEV_Digital_Write(DEV_DC_PIN, 1);
    
    uint32_t sent = 0;
    
    // Camera buffer layout: 320 pixels × 240 rows
    // We need to send: 240 pixels × 320 rows to fill LCD
    // So we transpose: camera's 320 columns become LCD's 320 rows
    
    for (uint16_t lcdRow = 0; lcdRow < 320; lcdRow++) {
        // Each LCD row comes from one camera column
        // Camera column = lcdRow (0-319)
        uint16_t camCol = lcdRow;
        
        // Read this column from all 240 camera rows
        uint16_t dmaIdx = 0;
        for (uint16_t camRow = 0; camRow < 240; camRow++) {
            // Calculate position in camera buffer
            uint32_t srcIdx = (camRow * 320 + camCol) * 2;  // Each pixel is 2 bytes
            
            // Copy 2 bytes (one RGB565 pixel)
            dmaBuf[dmaIdx++] = src[srcIdx];
            dmaBuf[dmaIdx++] = src[srcIdx + 1];
            
            // Send when buffer is full
            if (dmaIdx >= DMA_BUFFER_SIZE - 2) {
                DEV_SPI_Write_DMA(dmaBuf, dmaIdx);
                sent += dmaIdx;
                dmaIdx = 0;
            }
        }
        
        // Send remaining pixels in this row
        if (dmaIdx > 0) {
            DEV_SPI_Write_DMA(dmaBuf, dmaIdx);
            sent += dmaIdx;
        }
    }
    
    DEV_Digital_Write(DEV_CS_PIN, 1);
    
    Serial.print("LCD: Sent ");
    Serial.print(sent);
    Serial.println(" bytes (240x320, transposed)");
}