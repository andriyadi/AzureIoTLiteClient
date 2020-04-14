# AzureIoTLiteClient
 Azure IoT Hub / Central lite client library for ESP32, using MQTT


## Sample code

This is simple sample code to connect to Azure IoT Central and publish some telemetry and property.

```cpp
#include <Arduino.h>
#include "AzureIoTCentralClient.h"
#include <WiFiClientSecure.h>

const char* ssidName = "<CHANGE_THIS>";      // your network SSID (name of wifi network)
const char* ssidPass = "<CHANGE_THIS>";      // your network password

AzureIoTConfig_t iotconfig {
        "<CHANGE_THIS>",                    // Azure IoT Central - scopeId
        "<CHANGE_THIS>",                    // deviceId
        "<CHANGE_THIS>",                    // deviceKey
        AZURE_IOTC_CONNECT_SYMM_KEY         // Connection method
};

WiFiClientSecure wifiClient;
AzureIoTCentralClient iotclient(wifiClient);
bool isConnected = false;

static bool connectWiFi() {
    Serial.printf("Connecting WIFI to SSID: %s\r\n", ssidName);

    WiFi.begin(ssidName, ssidPass);

    // Attempt to connect to Wifi network:
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        // wait 1 second for re-trying
        delay(1000);
    }

    Serial.printf("Connected to %s\r\n", ssidName);
    Serial.print("IP: "); Serial.println(WiFi.localIP());

    return true;
}

void onAzureIoTEvent(const AzureIoTCallbacks_e cbType, const AzureIoTCallbackInfo_t *callbackInfo) {
    // ConnectionStatus
    if (strcmp(callbackInfo->eventName, "ConnectionStatus") == 0) {
        LOG_VERBOSE("Is connected ? %s (%d)", callbackInfo->statusCode == AzureIoTConnectionOK ? "YES" : "NO",  callbackInfo->statusCode);
        isConnected = callbackInfo->statusCode == AzureIoTConnectionOK;
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
    }
}

void setup() {
    Serial.begin(115200);
    
    if (!connectWiFi()) {
        while(1)
            ;
        // No point to continue
    }

    // Add callbacks
    iotclient.setCallback(AzureIoTCallbackMessageSent, onAzureIoTEvent);
    iotclient.setCallback(AzureIoTCallbackCommand, onAzureIoTEvent);
    iotclient.setCallback(AzureIoTCallbackSettingsUpdated, onAzureIoTEvent);
    
    // Must call this
    iotclient.begin(&iotconfig);
    
    // Do connect
    iotclient.connect();
}

unsigned long lastTick = 0;

void loop() {

    if (!iotclient.run()) {
        return; //No point to continue
    }

    unsigned long now = millis();

    // Send telemetry every 10 seconds
    if (ms - lastTick > 10000) { 
 
        char msg[64] = {0};
        int pos = 0;
        bool errorCode = 0;

        lastTick = now;

        // Send telemetry, randomized for now
        long tempVal = random(35, 40);
        pos = snprintf(msg, sizeof(msg) - 1, "{\"temperature\": %.2f}", tempVal*1.0f);
        msg[pos] = 0;
        errorCode = iotclient.sendTelemetry(msg, pos);

        // Send property
        pos = snprintf(msg, sizeof(msg) - 1, "{\"temperatureAlert\": %d}", (tempVal >= 38));
        msg[pos] = 0;
        errorCode = iotclient.sendProperty(msg, pos);

        if (!errorCode) {
            LOG_ERROR("Sending message has failed with error code %d", errorCode);
        }
    }
}
```
