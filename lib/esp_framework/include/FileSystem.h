// FileSystem.h
#ifdef USE_UFILESYS

#ifndef _FILESYSTEM_h
#define _FILESYSTEM_h

#include "Arduino.h"
#ifdef ESP8266
#include <LittleFS.h>
#include <SPI.h>
#endif // ESP8266
#ifdef ESP32
#if CONFIG_IDF_TARGET_ESP32 and ESP_IDF_VERSION_MAJOR == 3
#include <LITTLEFS.h>
#define LittleFS LITTLEFS
#else
#include <LittleFS.h>
#endif
#include "FFat.h"
#include "FS.h"
#endif // ESP32

#define UFS_TNONE 0
#define UFS_TSDC 1
#define UFS_TFAT 2
#define UFS_TLFS 3

class FileSystem
{
protected:
    static FS *fs;
    static uint8_t type;

public:
    static void init();
    static uint32_t info(uint32_t sel);
    static FS *getFs() { return fs; };
    static bool exists(const char *fname);
    static bool save(const char *fname, const uint8_t *buf, uint32_t len);
    static bool erase(const char *fname, uint32_t len, uint8_t init_value);
    static bool load(const char *fname, uint8_t *buf, uint32_t len);
    static bool del(const char *fname);
    static bool reName(const char *fname1, const char *fname2);
};

#endif
#endif