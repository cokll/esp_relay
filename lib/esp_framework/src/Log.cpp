#ifdef USE_SYSLOG
#include <WiFiUdp.h>
#endif
#include "Config.h"
#include "Log.h"
#include "Rtc.h"

#ifdef WEB_LOG_SIZE
uint8_t Log::webLogIndex = 1;
char Log::webLog[WEB_LOG_SIZE] = {0};
#ifdef ESP32
#include "AutoMutex.h"
SemaphoreHandle_t logmutex = (SemaphoreHandle_t) nullptr;
#endif
#endif

#ifdef USE_SYSLOG
IPAddress Log::ip;
WiFiUDP PortUdp;
#endif

size_t Log::strchrspn(const char *str1, int character)
{
    size_t ret = 0;
    char *start = (char *)str1;
    char *end = strchr(str1, character);
    if (end)
        ret = end - start;
    return ret;
}

#ifdef WEB_LOG_SIZE
bool Log::GetLog(uint32_t req_loglevel, uint32_t *index_p, char **entry_pp, size_t *len_p)
{
    uint32_t index = *index_p;
    if (!req_loglevel || (index == webLogIndex))
    {
        return false;
    }

#ifdef ESP32
    AutoMutex mutex(&logmutex);
#endif

    if (!index) // Dump all
    {
        index = webLog[0];
    }

    do
    {
        size_t len = 0;
        uint32_t loglevel = 0;
        char *entry_p = webLog;
        do
        {
            uint32_t cur_idx = *entry_p;
            entry_p++;
            size_t tmp = strchrspn(entry_p, '\1');
            tmp++;                // Skip terminating '\1'
            if (cur_idx == index) // Found the requested entry
            {
                loglevel = *entry_p - '0';
                entry_p++; // Skip loglevel
                len = tmp - 1;
                break;
            }
            entry_p += tmp;
        } while (entry_p < webLog + WEB_LOG_SIZE && *entry_p != '\0');
        index++;
        if (index > 255) // Skip 0 as it is not allowed
        {
            index = 1;
        }
        *index_p = index;
        if ((len > 0) && (loglevel <= req_loglevel))
        {
            *entry_pp = entry_p;
            *len_p = len;
            return true;
        }
        delay(0);
    } while (index != webLogIndex);
    return false;
}
#endif

#ifdef USE_SYSLOG
void Log::Syslog(uint8_t loglevel)
{
    if ((2 & globalConfig.debug.type) != 2 || WiFi.status() != WL_CONNECTED || globalConfig.debug.server[0] == '\0' || globalConfig.debug.port == 0 || loglevel > globalConfig.debug.syslog_level)
    {
        return;
    }

    if (!ip)
    {
        WiFi.hostByName(globalConfig.debug.server, ip);
    }
    if (PortUdp.beginPacket(ip, globalConfig.debug.port))
    {
        char syslog_preamble[64]; // Hostname + Id

        snprintf_P(syslog_preamble, sizeof(syslog_preamble), PSTR("%s "), UID);
        memmove(tmpData + strlen(syslog_preamble), tmpData, sizeof(tmpData) - strlen(syslog_preamble));
        tmpData[sizeof(tmpData) - 1] = '\0';
        memcpy(tmpData, syslog_preamble, strlen(syslog_preamble));
        PortUdp_write(tmpData, strlen(tmpData));
        PortUdp.endPacket();
        delay(1); // Add time for UDP handling (#5512)
    }
}
#endif

void Log::Record(uint8_t loglevel)
{
    char mxtime[13]; // "01 13:45:21 "
    snprintf_P(mxtime, sizeof(mxtime), PSTR("%02d %02d:%02d:%02d "), Rtc::rtcTime.day_of_month, Rtc::rtcTime.hour, Rtc::rtcTime.minute, Rtc::rtcTime.second);

    if ((1 & globalConfig.debug.type) == 1 && loglevel <= globalConfig.debug.seriallog_level)
    {
        Serial.printf(PSTR("%s%s\r\n"), mxtime, tmpData);
    }
    if ((8 & globalConfig.debug.type) == 8 && loglevel <= globalConfig.debug.seriallog_level)
    {
        Serial1.printf(PSTR("%s%s\r\n"), mxtime, tmpData);
    }

#ifdef WEB_LOG_SIZE
#ifdef ESP32
    AutoMutex mutex(&logmutex);
#endif
    // Delimited, zero-terminated buffer of log lines.
    // Each entry has this format: [index][loglevel][log data]['\1']
    if (((4 & globalConfig.debug.type) == 4 || loglevel == LOG_LEVEL_ERROR) && loglevel <= globalConfig.debug.weblog_level)
    {
        webLogIndex &= 0xFF;
        if (!webLogIndex)
            webLogIndex++;                                           // Index 0 is not allowed as it is the end of char string
        while (webLogIndex == webLog[0] ||                           // If log already holds the next index, remove it
               strlen(webLog) + strlen(tmpData) + 17 > WEB_LOG_SIZE) // 13 = web_log_index + mxtime + '\1' + '\0'
        {
            char *it = webLog;
            it++;                                              // Skip web_log_index
            it += strchrspn(it, '\1');                         // Skip log line
            it++;                                              // Skip delimiting "\1"
            memmove(webLog, it, WEB_LOG_SIZE - (it - webLog)); // Move buffer forward to remove oldest log line
        }
        snprintf_P(webLog, sizeof(webLog), PSTR("%s%c%c%s%s\1"), webLog, webLogIndex++, '0' + loglevel, mxtime, tmpData);
        webLogIndex &= 0xFF;
        if (!webLogIndex)
            webLogIndex++; // Index 0 is not allowed as it is the end of char string
    }
#endif

#ifdef USE_SYSLOG
    Syslog(loglevel);
#endif
}

void Log::Record(uint8_t loglevel, PGM_P formatP, ...)
{
    va_list arg;
    va_start(arg, formatP);
    vsnprintf_P(tmpData, sizeof(tmpData), formatP, arg);
    va_end(arg);

    Record(loglevel);
}

void Log::Info(PGM_P formatP, ...)
{
    va_list arg;
    va_start(arg, formatP);
    vsnprintf_P(tmpData, sizeof(tmpData), formatP, arg);
    va_end(arg);

    Record(LOG_LEVEL_INFO);
}

void Log::Debug(PGM_P formatP, ...)
{
    va_list arg;
    va_start(arg, formatP);
    vsnprintf_P(tmpData, sizeof(tmpData), formatP, arg);
    va_end(arg);

    Record(LOG_LEVEL_DEBUG);
}

void Log::Error(PGM_P formatP, ...)
{
    va_list arg;
    va_start(arg, formatP);
    vsnprintf_P(tmpData, sizeof(tmpData), formatP, arg);
    va_end(arg);

    Record(LOG_LEVEL_ERROR);
}
