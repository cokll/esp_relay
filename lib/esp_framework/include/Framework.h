
// Framework.h

#ifndef _FRAMEWORK_h
#define _FRAMEWORK_h

#include "Arduino.h"

class Framework
{
    static uint16_t rebootCount;
    static void callback(char *topic, byte *payload, unsigned int length);
    static void connectedCallback();
    static void tickerPerSecondDo();
    static void sleepDelay(uint32_t mseconds);

public:
    static uint8_t sleepTime;
    static uint32_t loopLoadAvg;
    static bool sleepNormal;
    static void one(unsigned long baud);
    static void setup();
    static void loop();
};

#endif