#ifndef _Util_h
#define _Util_h

#include "Arduino.h"

class Util
{
public:
    static char *strlowr(char *str);
    static char *strupr(char *str);
    static uint16_t hex2Str(uint8_t *bin, uint16_t bin_size, char *buff, bool needBlank = true);
    static char *dtostrfd(double number, unsigned char prec, char *s);
    static uint32_t SqrtInt(uint32_t num);
    static uint32_t RoundSqrtInt(uint32_t num);
    static bool endWith(char *str, const char *suffix, uint16_t strLen);

    static inline int32_t timeDifference(uint32_t prev, uint32_t next);
    static int32_t timePassedSince(uint32_t timestamp);
    static bool timeReached(uint32_t timer);
    static void setNextTimeInterval(uint32_t &timer, const uint32_t step);
    static int32_t timePassedSinceUsec(uint32_t timestamp);
    static bool timeReachedUsec(uint32_t timer);
    static int split(char *str, const char *delim, char dst[][80]);
};
#endif