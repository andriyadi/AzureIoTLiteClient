//
// Created by Andri Yadi on 14/04/20.
//

#if defined(ESP32)

#include "../TimeUtils.h"
#include <cstring>
#include <ctime>
#include "lwip/apps/sntp.h"

static bool sntpInitted = false;
void initializeSntp() {
    if (sntpInitted) {
        return;
    }

    DEBUGLOG("Initializing SNTP...\r\n");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, (char*)"0.id.pool.ntp.org");
    sntp_setservername(1, (char*)"1.id.pool.ntp.org");
    sntp_setservername(2, (char*)"pool.ntp.org");
    sntp_init();

    sntpInitted = true;
}

bool requestTime(int timezone) {

    initializeSntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        DEBUGLOG("Waiting for system time to be set... (%d/%d) [tm_year:%d]\r\n", retry, retry_count, timeinfo.tm_year);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    return  (retry < retry_count);
}

long getEpoch() {

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        DEBUGLOG("Time is not set yet. Connecting to WiFi and getting time over NTP.");
        requestTime(0);
        // update 'now' variable with current time
        time(&now);
    }

//    DEBUGLOG("Epoch: %lu\n\n", now);
    return now;// + (7*3600);
}

void getTimeString(char *timeStr) {
    time_t now;
    struct tm timeinfo;

    now = (time_t)getEpoch();

    char strftime_buf[64];
    // Set timezone to Eastern Standard Time and print local time
    //setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    //setenv("TZ", "Etc/GMT-7", 1);
    setenv("TZ", "UTC", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    //strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%dT%H:%M:%S.000Z", &timeinfo);
    //NET_DEBUG_PRINT("The current date/time: %s", strftime_buf);

    strcpy(timeStr, strftime_buf);
}

#endif //defined(ESP32)