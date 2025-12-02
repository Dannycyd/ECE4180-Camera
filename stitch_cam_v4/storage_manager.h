/**
 * @file storage_manager.h
 * @brief SD card storage management
 */

#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "config.h"
#include <SD.h>
#include <FS.h>

class StorageManager {
public:
  static void init() {
    DEBUG_PRINTLN("→ Initializing SD Card...");
    
    // Initialize SD card
    if (!SD.begin(Pins::SD_CS)) {
      DEBUG_PRINTLN("  ❌ SD Card initialization failed");
      isAvailable = false;
      return;
    }
    
    // Check card type
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
      DEBUG_PRINTLN("  ❌ No SD card attached");
      isAvailable = false;
      return;
    }
    
    // Print card info
    DEBUG_PRINTF("  ✓ SD Card Type: ");
    switch (cardType) {
      case CARD_MMC:  DEBUG_PRINTLN("MMC"); break;
      case CARD_SD:   DEBUG_PRINTLN("SD"); break;
      case CARD_SDHC: DEBUG_PRINTLN("SDHC"); break;
      default:        DEBUG_PRINTLN("UNKNOWN"); break;
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    DEBUG_PRINTF("  ✓ SD Card Size: %lluMB\n", cardSize);
    
    // Create photos directory if it doesn't exist
    if (!SD.exists(StorageConfig::PHOTO_DIR)) {
      if (SD.mkdir(StorageConfig::PHOTO_DIR)) {
        DEBUG_PRINTF("  ✓ Created directory: %s\n", StorageConfig::PHOTO_DIR);
      } else {
        DEBUG_PRINTF("  ❌ Failed to create directory: %s\n", StorageConfig::PHOTO_DIR);
      }
    }
    
    // Count existing photos
    photoCount = countPhotos();
    DEBUG_PRINTF("  ✓ Found %d existing photos\n", photoCount);
    
    isAvailable = true;
    DEBUG_PRINTLN("✓ SD Card initialized");
  }
  
  /**
   * @brief Save JPEG buffer to SD card
   * @param jpegData Pointer to JPEG data
   * @param length Length of JPEG data
   * @param filename Output filename (will be set)
   * @return true if save successful
   */
  static bool savePhoto(const uint8_t* jpegData, uint32_t length, char* filename) {
    if (!isAvailable || !jpegData || length == 0) {
      return false;
    }
    
    // Generate unique filename
    generateFilename(filename);
    
    // Open file for writing
    File file = SD.open(filename, FILE_WRITE);
    if (!file) {
      DEBUG_PRINTF("  ❌ Failed to open file: %s\n", filename);
      return false;
    }
    
    // Write JPEG data
    size_t written = file.write(jpegData, length);
    file.close();
    
    if (written != length) {
      DEBUG_PRINTF("  ❌ Write error: %d/%d bytes\n", written, length);
      return false;
    }
    
    photoCount++;
    DEBUG_PRINTF("  ✓ Saved: %s (%d bytes)\n", filename, length);
    return true;
  }
  
  /**
   * @brief Delete a photo
   */
  static bool deletePhoto(const char* filename) {
    if (!isAvailable) return false;
    
    if (SD.remove(filename)) {
      photoCount--;
      DEBUG_PRINTF("  ✓ Deleted: %s\n", filename);
      return true;
    }
    
    DEBUG_PRINTF("  ❌ Failed to delete: %s\n", filename);
    return false;
  }
  
  /**
   * @brief Get list of all photos
   * @param fileList Array to store filenames
   * @param maxFiles Maximum number of files
   * @return Number of files found
   */
  static uint32_t getPhotoList(String* fileList, uint32_t maxFiles) {
    if (!isAvailable) return 0;
    
    File root = SD.open(StorageConfig::PHOTO_DIR);
    if (!root || !root.isDirectory()) {
      return 0;
    }
    
    uint32_t count = 0;
    File file = root.openNextFile();
    
    while (file && count < maxFiles) {
      if (!file.isDirectory()) {
        String name = file.name();
        if (name.endsWith(".jpg") || name.endsWith(".JPG")) {
          fileList[count++] = name;
        }
      }
      file.close();
      file = root.openNextFile();
    }
    
    root.close();
    return count;
  }
  
  /**
   * @brief Read photo file
   * @param filename File to read
   * @param buffer Buffer to store data
   * @param maxSize Maximum buffer size
   * @return Number of bytes read
   */
  static uint32_t readPhoto(const char* filename, uint8_t* buffer, uint32_t maxSize) {
    if (!isAvailable) return 0;
    
    File file = SD.open(filename, FILE_READ);
    if (!file) {
      DEBUG_PRINTF("  ❌ Failed to open: %s\n", filename);
      return 0;
    }
    
    uint32_t fileSize = file.size();
    uint32_t readSize = min(fileSize, maxSize);
    
    uint32_t bytesRead = file.read(buffer, readSize);
    file.close();
    
    return bytesRead;
  }
  
  /**
   * @brief Get file size
   */
  static uint32_t getFileSize(const char* filename) {
    if (!isAvailable) return 0;
    
    File file = SD.open(filename, FILE_READ);
    if (!file) return 0;
    
    uint32_t size = file.size();
    file.close();
    return size;
  }
  
  /**
   * @brief Check if SD card is available
   */
  static bool available() {
    return isAvailable;
  }
  
  /**
   * @brief Get total number of photos
   */
  static uint32_t getPhotoCount() {
    return photoCount;
  }
  
  /**
   * @brief Get free space in MB
   */
  static uint64_t getFreeSpaceMB() {
    if (!isAvailable) return 0;
    return SD.totalBytes() / (1024 * 1024);
  }
  
private:
  static bool isAvailable;
  static uint32_t photoCount;
  
  /**
   * @brief Generate unique filename
   */
  static void generateFilename(char* buffer) {
    // Format: /photos/IMG_NNNN.jpg
    snprintf(buffer, StorageConfig::MAX_FILENAME_LEN, 
             "%s/%s%04d.jpg", 
             StorageConfig::PHOTO_DIR,
             StorageConfig::PHOTO_PREFIX,
             photoCount + 1);
  }
  
  /**
   * @brief Count existing photos in directory
   */
  static uint32_t countPhotos() {
    if (!isAvailable) return 0;
    
    File root = SD.open(StorageConfig::PHOTO_DIR);
    if (!root || !root.isDirectory()) {
      return 0;
    }
    
    uint32_t count = 0;
    File file = root.openNextFile();
    
    while (file) {
      if (!file.isDirectory()) {
        String name = file.name();
        if (name.endsWith(".jpg") || name.endsWith(".JPG")) {
          count++;
        }
      }
      file.close();
      file = root.openNextFile();
    }
    
    root.close();
    return count;
  }
};

// Static member initialization
bool StorageManager::isAvailable = false;
uint32_t StorageManager::photoCount = 0;

#endif // STORAGE_MANAGER_H
