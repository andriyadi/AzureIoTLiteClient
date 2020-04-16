//
// Created by Andri Yadi on 14/04/20.
//

#ifndef _TIMEUTILS_H
#define _TIMEUTILS_H

#include <cstdlib>
#include <cstdio>

#define LOGGING                     1
#define USE_WIFI_BUILTIN_SNTP       1

#if ARDUINO
#include "Arduino.h"
#define ser_printf Serial.printf
#else
#define ser_printf printf
#endif

#if LOGGING
#define DEBUGLOG(...) ser_printf(__VA_ARGS__)
#else
#define DEBUGLOG(...)
#endif

bool requestTime(int timezone);
void processTime(long epoch = 0);
void getTimeString(char *timeStr);

#endif //_TIMEUTILS_H
