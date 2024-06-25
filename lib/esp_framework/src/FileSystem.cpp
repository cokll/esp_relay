#ifdef USE_UFILESYS

#include "FileSystem.h"
#include "Log.h"

FS *FileSystem::fs = 0;
uint8_t FileSystem::type = 0;

void FileSystem::init(void)
{
    type = UFS_TLFS;
    fs = 0;

#ifdef ESP8266
    fs = &LittleFS;
    if (!LittleFS.begin())
    {
        if (!LittleFS.format())
        {
            type = 0;
            fs = 0;
            Log::Error(PSTR("FS: FlashFS no Support, Format Error"));
            return;
        }
        else
        {
            if (!LittleFS.begin())
            {
                type = 0;
                fs = 0;
                Log::Error(PSTR("FS: FlashFS no Support"));
                return;
            }
        }
    }
#endif // ESP8266

#ifdef ESP32
    // try lfs first
    fs = &LittleFS;
    if (!LittleFS.begin(true))
    {
        // ffat is second
        fs = &FFat;
        type = UFS_TFAT;
        if (!FFat.begin(true))
        {
            type = 0;
            fs = 0;
            Log::Error(PSTR("FS: FlashFS no Support"));
            return;
        }
    }
#endif // ESP32
    if (type)
    {
        Log::Info(PSTR("FS: FlashFS mounted with %d kB free, %d kB total"), info(1), info(0));
    }
}

uint32_t FileSystem::info(uint32_t sel)
{
    uint64_t result = 0;
#ifdef ESP8266
    FSInfo64 fsinfo;
#endif // ESP8266

    switch (type)
    {
    case UFS_TLFS:
#ifdef ESP8266
        fs->info64(fsinfo);
        if (sel == 0)
        {
            result = fsinfo.totalBytes;
        }
        else
        {
            result = (fsinfo.totalBytes - fsinfo.usedBytes);
        }
#endif // ESP8266
#ifdef ESP32
        if (sel == 0)
        {
            result = LittleFS.totalBytes();
        }
        else
        {
            result = LittleFS.totalBytes() - LittleFS.usedBytes();
        }
#endif // ESP32
        break;

    case UFS_TFAT:
#ifdef ESP32
        if (sel == 0)
        {
            result = FFat.totalBytes();
        }
        else
        {
            result = FFat.freeBytes();
        }
#endif // ESP32
        break;
    }
    return result / 1024;
}

bool FileSystem::exists(const char *fname)
{
    if (!type)
    {
        return false;
    }

    bool yes = fs->exists(fname);
    if (!yes)
    {
        // Log::Info(PSTR("FS: File '%s' not found"), fname + 1); // Skip leading slash
    }
    return yes;
}

bool FileSystem::save(const char *fname, const uint8_t *buf, uint32_t len)
{
    if (!type)
    {
        return false;
    }

    File file = fs->open(fname, "w");
    if (!file)
    {
        Log::Info(PSTR("FS: Save failed"));
        return false;
    }

    file.write(buf, len);
    file.close();
    return true;
}

bool FileSystem::erase(const char *fname, uint32_t len, uint8_t init_value)
{
    if (!type)
    {
        return false;
    }

    File file = fs->open(fname, "w");
    if (!file)
    {
        Log::Info(PSTR("FS: Erase failed"));
        return false;
    }

    for (uint32_t i = 0; i < len; i++)
    {
        file.write(&init_value, 1);
    }
    file.close();
    return true;
}

bool FileSystem::load(const char *fname, uint8_t *buf, uint32_t len)
{
    if (!type)
    {
        return false;
    }
    if (!exists(fname))
    {
        return false;
    }

    File file = fs->open(fname, "r");
    if (!file)
    {
        // Log::Info(PSTR("FS: File '%s' not found"), fname + 1); // Skip leading slash
        return false;
    }

    file.read(buf, len);
    file.close();
    return true;
}

bool FileSystem::del(const char *fname)
{
    if (!type)
    {
        return false;
    }

    if (!fs->remove(fname))
    {
        Log::Info(PSTR("FS: Delete failed"));
        return false;
    }
    return true;
}

bool FileSystem::reName(const char *fname1, const char *fname2)
{
    if (!type)
    {
        return false;
    }

    if (!fs->rename(fname1, fname2))
    {
        Log::Info(PSTR("FS: Rename failed"));
        return false;
    }
    return true;
}
#endif