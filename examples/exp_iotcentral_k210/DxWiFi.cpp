#include "DxWiFi.h"


bool DxWiFiEspClass::getTime(long &epochTime, char *ansiString, int timezone) {
    if (status() != WL_CONNECTED) {
        Serial.printf("Not connected yet! Status: %d\r\n", status());
        return false;
    }

    bool isEnabled = false;
    int setTz = 0;
    DxEspDrv::querySNTPConfig(isEnabled, setTz);
    if (!isEnabled) {
        delay(100);
        if (!DxEspDrv::enabledSNTP(timezone)) {
            Serial.println("Failed enabling SNTP");
            return false;
        }
        // Wait?
        delay(200);
    }    

    bool ret = false;
    for(int i = 0; i < 10; i++) {
        ret = DxEspDrv::querySNTPTime(epochTime, ansiString);   
        if (ret) {
            break;
        }
        Serial.println("Waiting for SNTP response...");
        delay(500);
    }

    return ret;
}

DxWiFiEspClass DxWiFi;

