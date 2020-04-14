//
// Created by Andri Yadi on 14/04/20.
//

#if defined(RISCV)

#include "../TimeUtils.h"

#if !USE_WIFI_BUILTIN_SNTP
#include <NTP.h>
static NTP *ntp_ = NULL;
static WiFiEspUDP *wifiUdp_ = NULL;
#endif // End of !USE_WIFI_BUILTIN_SNTP

bool requestTime() {
#if !USE_WIFI_BUILTIN_SNTP
    if (ntp_ == NULL)
    {
        wifiUdp_ = new WiFiEspUDP();
        ntp_ = new NTP(*wifiUdp_);
        ntp_->updateInterval(2000);

        // ntp_->ruleDST("CEST", Last, Sun, Mar, 2, 120);

        ntp_->begin(false);
        return false;
    }
    else {
        return ntp_->update();
    }

#else

    long epoch; char ansStr[40];
    if (DxWiFi.getTime(epoch, ansStr, 7)) {
        DEBUGLOG("Got time %ld -> %s\r\n", epoch, ansStr);

        processTime(epoch);
        return true;
    }
    else {
        DEBUGLOG("ERROR: Failed getting SNTP time\r\n");

        //TODO: Should continue?
        return false;
    }
#endif

    return false;
}

void processTime(long epoch) {
#if !USE_WIFI_BUILTIN_SNTP
    if (ntp_ == NULL)
    {
        return;
    }

    DEBUGLOG("Current time: %s - %s\r\n", ntp_->formattedTime("%d. %B %Y"), ntp_->formattedTime("%A %T"));

    rtc_timer_set(ntp_->year(), ntp_->month(), ntp_->day(), ntp_->hours(), ntp_->month(), ntp_->seconds());

    auto *dateTime = rtc_timer_get_tm();
    time_t timeSinceEpoch = mktime(dateTime);
    unsigned long expiresSecond = timeSinceEpoch + 7200;
    DEBUGLOG("Epoch: %lu - %lu\n\n", timeSinceEpoch, expiresSecond);

    // Delete NTP
    delete ntp_;
    ntp_ = NULL;

    // delete wifiUdp_;
    wifiUdp_ = NULL;

#else
    struct tm epoch_tm;
    time_t epoch_time = (time_t)epoch;
    memcpy(&epoch_tm, localtime(&epoch_time), sizeof (struct tm));

    rtc_timer_set_tm(&epoch_tm);

    // For testing
    auto *dateTime = rtc_timer_get_tm();
    time_t timeSinceEpoch = mktime(dateTime);
    unsigned long expiresSecond = timeSinceEpoch + 7200;
    DEBUGLOG("Epoch: %lu -> %lu\n\n", timeSinceEpoch, expiresSecond);
#endif
}

long getEpoch() {
    auto *dateTime = rtc_timer_get_tm();
    time_t timeSinceEpoch = mktime(dateTime);
    DEBUGLOG("Epoch: %lu\n\n", timeSinceEpoch);

    return timeSinceEpoch;
}

#endif // End of defined(RISCV)


