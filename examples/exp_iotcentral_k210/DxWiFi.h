#ifndef _DX_WIFI_H
#define _DX_WIFI_H

#include "WiFiEsp.h"
#include "utility/EspDrv.h"

typedef enum
{
    TAG_OK,
    TAG_ERROR,
    TAG_FAIL,
    TAG_SENDOK,
    TAG_CONNECT
} TagsEnum;

class DxEspDrv : public EspDrv {

public:
    static void wifiDriverInit(Stream *espSerial, bool reset = true) {
        LOGDEBUG(F("> My wifiDriverInit"));

        EspDrv::espSerial = espSerial;

        bool initOK = false;
        
        for(int i=0; i<5; i++)
        {
            if (sendCmd(F("AT")) == TAG_OK)
            {
                initOK=true;
                break;
            }
            delay(1000);
        }

        if (!initOK)
        {
            LOGERROR(F("Cannot initialize ESP module"));
            delay(5000);
            return;
        }

        if (reset) {
            EspDrv::reset();

            // check firmware version
            getFwVersion();

            // prints a warning message if the firmware is not 1.X or 2.X
            if ((fwVersion[0] != '1' and fwVersion[0] != '2') or
                fwVersion[1] != '.')
            {
                LOGWARN1(F("Warning: Unsupported firmware"), fwVersion);
                delay(4000);
            }
            else
            {
                LOGINFO1(F("Initilization successful -"), fwVersion);
            }
        }
    }

    static bool resetUartBaudRate()
    {
        LOGINFO(F("Resetting BAUD Rate"));
        if (EspDrv::sendCmd(F("AT")) != TAG_OK)
        {
            return false;
        }

        if (EspDrv::sendCmd(F("AT+UART_CUR=921600,8,1,0,0")) != TAG_OK)
        {
            return false;
        }

        return true;
    }
};

class DxWiFiEspClass: public WiFiEspClass {

public:
    DxWiFiEspClass() {}

    static void restartWiFiModule(uint8_t enPin = 8) {
        pinMode(enPin, OUTPUT);
        digitalWrite(enPin, LOW);
        delay(200);
        digitalWrite(enPin, HIGH);
        delay(2000);
    }

    static void init(Stream* espSerial, bool reset = true) {
        LOGINFO(F("Initializing ESP module"));
	    DxEspDrv::wifiDriverInit(espSerial, reset);
    }

    bool getTime(long &epochTime, char *ansiString, int timezone=7);
};

extern DxWiFiEspClass DxWiFi;

#endif