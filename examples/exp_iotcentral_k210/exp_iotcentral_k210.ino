#include <Arduino.h>
#include "DxWiFi.h"
#include "AzureIoTLiteClient.h"
#include "WiFiEspSecureClient.h"
#include "rtc.h"

#define TEST_SAS 0

#if TEST_SAS
#include "IotcUtils.h"
#endif

const char* ssidName = "<CHANGE_THIS>";     // your network SSID (name of wifi network)
const char* ssidPass = "<CHANGE_THIS>"; // your network password

AzureIoTConfig_t iotconfig {
    "<CHANGE_THIS>",
    "<CHANGE_THIS>",
    "<CHANGE_THIS>",
    AZURE_IOTC_CONNECT_SYMM_KEY
};

WiFiEspSecureClient wifiClient;
AzureIoTLiteClient iotclient(wifiClient);

static bool connectWiFi() {
    Serial.println("Connecting WIFI...");
    
    // Restart ESP module
    DxWiFi.restartWiFiModule();

    Serial.println("WiFi shield init");
    // initialize serial for ESP module, begin with 115200 default
    Serial1.begin(115200);
    DxWiFi.init(&Serial1, true);

    //Reset UART to 921600
    // DxEspDrv::resetUartBaudRate();
    // Serial1.end();
    // delay(100);
    // Serial1.begin(921600);
    // DxWiFi.init(&Serial1, false);

    // check for the presence of the shield
    if (DxWiFi.status() == WL_NO_SHIELD)
    {
        Serial.println("WiFi shield not present");
        // don't continue
        return false;
    }

    // Attempt to connect to WiFi network
    int wifiStatus = WL_IDLE_STATUS;
    while (wifiStatus != WL_CONNECTED)
    {
        Serial.printf("Attempting to connect to WPA SSID: %s\r\n", ssidName);
        // Connect to WPA/WPA2 network
        wifiStatus = DxWiFi.begin(ssidName, ssidPass);
    }

    Serial.print("Connected to the network. IP: "); Serial.println(DxWiFi.localIP());

    // long epoch; char ansStr[40];
    // if (DxWiFi.getTime(epoch, ansStr, 7)) {
    //     Serial.printf("Got time %ld -> %s\r\n", epoch, ansStr);
    // }
    // else {
    //     Serial.println("Failed getting SNTP time");
    // }

    return true;
}

void executeCommand(const char *cmdName, char *payload) {
    if (strcmp(cmdName, "turnONLED") == 0) {
        LOG_VERBOSE("Executing setLED -> value: %s", payload);
        if (strcmp(payload, "true") == 0) {
            digitalWrite(2, HIGH);
        }
        else {
            digitalWrite(2, LOW);
        }
    }
}

void onEvent(const AzureIoTCallbacks_e cbType, const AzureIoTCallbackInfo_t *callbackInfo) {
    // ConnectionStatus
    if (strcmp(callbackInfo->eventName, "ConnectionStatus") == 0) {
        LOG_VERBOSE("Is connected ? %s (%d)", callbackInfo->statusCode == AzureIoTConnectionOK ? "YES" : "NO",  callbackInfo->statusCode);
        // isConnected = callbackInfo->statusCode == AzureIoTConnectionOK;
        return;
    }

    // payload buffer doesn't have a null ending.
    // add null ending in another buffer before print
    AzureIOT::StringBuffer buffer;
    if (callbackInfo->payloadLength > 0) {
        buffer.initialize(callbackInfo->payload, callbackInfo->payloadLength);
    }

    LOG_VERBOSE("[%s] event was received. Payload => %s\n", callbackInfo->eventName, buffer.getLength() ? *buffer : "EMPTY");

    if (strcmp(callbackInfo->eventName, "Command") == 0) {
        LOG_VERBOSE("Command name was => %s\r\n", callbackInfo->tag);
        executeCommand(callbackInfo->tag, *buffer);
    }
}

void setup()
{
    delay(2000);
    rtc_init();

    Serial.begin(115200);
    // while (!Serial)
    // {
    //     ; // wait for serial port to connect. Needed for native USB port only
    // }
    Serial.println("It begins!");

#if TEST_SAS
    size_t authHeaderSize = 0;
    AzureIOT::StringBuffer authHeader(STRING_BUFFER_256);
    DEBUGLOG("- iotc.dps : getting auth...\r\n");
    if (createDPSAuthString(iotconfig.scopeId, iotconfig.deviceId, iotconfig.deviceKey, *authHeader, STRING_BUFFER_256, authHeaderSize))
    {
        Serial.println("ERROR: getDPSAuthString has failed");
    }
    else {
        DEBUGLOG("Authorization (%ld): %s\r\n", authHeaderSize, *authHeader);
    }
#else
    
    if (!connectWiFi()) {
        while(1)
            ;
    }

    // Add callbacks
    iotclient.setCallback(AzureIoTCallbackMessageSent, onEvent);
    iotclient.setCallback(AzureIoTCallbackCommand, onEvent);
    iotclient.setCallback(AzureIoTCallbackSettingsUpdated, onEvent);

    iotclient.begin(&iotconfig);

#endif
}

unsigned long lastTick = 0, loopId = 0;

void loop() {
#if !TEST_SAS
    if (!iotclient.run()) {
        return; //No point to continue
    }

    unsigned long ms = millis();
    if (ms - lastTick > 10000) {  // send telemetry every 10 seconds
        char msg[64] = {0};
        int pos = 0;
        bool errorCode = 0;

        lastTick = ms;

        if (loopId++ % 2 == 0) {
            // Send telemetry
            long tempVal = random(35, 40);
            pos = snprintf(msg, sizeof(msg) - 1, "{\"temperature\": %.2f}", tempVal*1.0f);
            msg[pos] = 0;
            errorCode = iotclient.sendTelemetry(msg, pos);

            // Send property
            pos = snprintf(msg, sizeof(msg) - 1, "{\"temperatureAlert\": %d}", (tempVal >= 38));
            msg[pos] = 0;
            errorCode = iotclient.sendProperty(msg, pos);

        } else {
            // Send property
            pos = snprintf(msg, sizeof(msg) - 1, "{\"wornMask\": %d}", (random(0, 100) % 2 == 0));
            msg[pos] = 0;
            errorCode = iotclient.sendProperty(msg, pos);
        }

        if (!errorCode) {
            LOG_ERROR("Sending message has failed with error code %d", errorCode);
        }
    }
#endif
}