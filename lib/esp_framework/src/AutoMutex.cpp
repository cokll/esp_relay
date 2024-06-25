#ifdef ESP32
#include "AutoMutex.h"

AutoMutex::AutoMutex(SemaphoreHandle_t *mutex, const char *name, int maxWait, bool take)
{
    if (mutex)
    {
        if (!(*mutex))
        {
            AutoMutex::init(mutex);
        }
        this->mutex = *mutex;
        this->maxWait = maxWait;
        this->name = name;
        if (take)
        {
            this->taken = xSemaphoreTakeRecursive(this->mutex, this->maxWait);
            if (!this->taken)
            {
                Serial.printf("\r\nMutexfail %s\r\n", this->name);
            }
        }
    }
    else
    {
        this->mutex = (SemaphoreHandle_t) nullptr;
    }
}

AutoMutex::~AutoMutex()
{
    if (this->mutex)
    {
        if (this->taken)
        {
            xSemaphoreGiveRecursive(this->mutex);
            this->taken = false;
        }
    }
}

void AutoMutex::init(SemaphoreHandle_t *ptr)
{
    SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();
    (*ptr) = mutex;
    // needed, else for ESP8266 as we will initialis more than once in logging
    //  (*ptr) = (void *) 1;
}

void AutoMutex::give()
{
    if (this->mutex)
    {
        if (this->taken)
        {
            xSemaphoreGiveRecursive(this->mutex);
            this->taken = false;
        }
    }
}

void AutoMutex::take()
{
    if (this->mutex)
    {
        if (!this->taken)
        {
            this->taken = xSemaphoreTakeRecursive(this->mutex, this->maxWait);
            if (!this->taken)
            {
                Serial.printf("\r\nMutexfail %s\r\n", this->name);
            }
        }
    }
}
#endif