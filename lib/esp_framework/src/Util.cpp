#include "Util.h"

char *Util::strlowr(char *str)
{
    char *orign = str;
    for (; *str != '\0'; str++)
        *str = tolower(*str);
    return orign;
}

char *Util::strupr(char *str)
{
    char *orign = str;
    for (; *str != '\0'; str++)
        *str = toupper(*str);
    return orign;
}

uint16_t Util::hex2Str(uint8_t *bin, uint16_t bin_size, char *buff, bool needBlank)
{
    const char *set = "0123456789ABCDEF";
    char *nptr = buff;
    if (NULL == buff)
    {
        return -1;
    }
    uint16_t len = needBlank ? (bin_size * 2 + bin_size) : (bin_size * 2 + 1);
    while (bin_size--)
    {
        *nptr++ = set[(*bin) >> 4];
        *nptr++ = set[(*bin++) & 0xF];
        if (needBlank && bin_size > 0)
        {
            *nptr++ = ' ';
        }
    }
    *nptr = '\0';
    return len;
}

char *Util::dtostrfd(double number, unsigned char prec, char *s)
{
    if ((isnan(number)) || (isinf(number)))
    { // Fix for JSON output (https://stackoverflow.com/questions/1423081/json-left-out-infinity-and-nan-json-status-in-ecmascript)
        strcpy(s, PSTR("null"));
        return s;
    }
    else
    {
        dtostrf(number, 1, prec, s);
        while (prec > 0)
        {
            if (s[strlen(s) - 1] == '0')
            {
                s[strlen(s) - 1] = 0;
            }
            else if (s[strlen(s) - 1] == '.')
            {
                s[strlen(s) - 1] = 0;
                break;
            }
            else
            {
                break;
            }
        }
        return s;
    }
}

uint32_t Util::SqrtInt(uint32_t num)
{
    if (num <= 1)
    {
        return num;
    }

    uint32_t x = num / 2;
    uint32_t y;
    do
    {
        y = (x + num / x) / 2;
        if (y >= x)
        {
            return x;
        }
        x = y;
    } while (true);
}

uint32_t Util::RoundSqrtInt(uint32_t num)
{
    uint32_t s = SqrtInt(4 * num);
    if (s & 1)
    {
        s++;
    }
    return s / 2;
}

bool Util::endWith(char *str, const char *suffix, uint16_t strLen)
{
    if (strLen == 0)
    {
        strLen = strlen(str);
    }
    size_t suffixLen = strlen(suffix);
    return suffixLen <= strLen && strncmp(str + strLen - suffixLen, suffix, suffixLen) == 0 ? true : false;
}

inline int32_t Util::timeDifference(uint32_t prev, uint32_t next)
{
    return ((int32_t)(next - prev));
}

int32_t Util::timePassedSince(uint32_t timestamp)
{
    // Compute the number of milliSeconds passed since timestamp given.
    // Note: value can be negative if the timestamp has not yet been reached.
    return timeDifference(timestamp, millis());
}

bool Util::timeReached(uint32_t timer)
{
    // Check if a certain timeout has been reached.
    const long passed = timePassedSince(timer);
    return (passed >= 0);
}

void Util::setNextTimeInterval(uint32_t &timer, const uint32_t step)
{
    timer += step;
    const long passed = timePassedSince(timer);
    if (passed < 0)
    {
        return;
    } // Event has not yet happened, which is fine.
    if (static_cast<unsigned long>(passed) > step)
    {
        // No need to keep running behind, start again.
        timer = millis() + step;
        return;
    }
    // Try to get in sync again.
    timer = millis() + (step - passed);
}

int32_t Util::timePassedSinceUsec(uint32_t timestamp)
{
    return timeDifference(timestamp, micros());
}

bool Util::timeReachedUsec(uint32_t timer)
{
    // Check if a certain timeout has been reached.
    const long passed = timePassedSinceUsec(timer);
    return (passed >= 0);
}

int Util::split(char *str, const char *delim, char dst[][80])
{
    char *s = strdup(str);
    char *token;
    int n = 0;
    for (token = strsep(&s, delim); token != NULL; token = strsep(&s, delim))
    {
        strcpy(dst[n++], token);
    }
    return n;
}