#ifndef _AUTOMUTEX_H
#define _AUTOMUTEX_H
#ifdef ESP32
#include "Arduino.h"
#include "freertos/semphr.h"

/*********************************************************************************************\
 * ESP32 AutoMutex
\*********************************************************************************************/

//////////////////////////////////////////
// automutex.
// create a mute in your driver with:
// void *mutex = nullptr;
//
// then protect any function with
// AutoMutex m(&mutex, "somename");
// - mutex is automatically initialised if not already intialised.
// - it will be automagically released when the function is over.
// - the same thread can take multiple times (recursive).
// - advanced options m.give() and m.take() allow you fine control within a function.
// - if take=false at creat, it will not be initially taken.
// - name is used in serial log of mutex deadlock.
// - maxWait in ticks is how long it will wait before failing in a deadlock scenario (and then emitting on serial)
class AutoMutex
{
    SemaphoreHandle_t mutex;
    bool taken;
    int maxWait;
    const char *name;

public:
    AutoMutex(SemaphoreHandle_t *mutex, const char *name = "", int maxWait = 40, bool take = true);
    ~AutoMutex();
    void give();
    void take();
    static void init(SemaphoreHandle_t *ptr);
};

#endif
#endif