// Log.h

#ifndef _LOG_h
#define _LOG_h

#include "Config.h"

enum LoggingLevels
{
    LOG_LEVEL_NONE,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_DEBUG_MORE,
    LOG_LEVEL_ALL
};

class Log
{
protected:
    static size_t strchrspn(const char *str1, int character);

public:
#ifdef WEB_LOG_SIZE
    static uint8_t webLogIndex;
    static char webLog[WEB_LOG_SIZE];
    static bool GetLog(uint32_t req_loglevel, uint32_t *index_p, char **entry_pp, size_t *len_p);
#endif

#ifdef USE_SYSLOG
    static IPAddress ip;
    static void Syslog(uint8_t loglevel);
#endif
    static void Record(uint8_t loglevel);
    static void Record(uint8_t loglevel, PGM_P formatP, ...);

    static void Info(PGM_P formatP, ...);
    static void Debug(PGM_P formatP, ...);
    static void Error(PGM_P formatP, ...);
};

#endif
