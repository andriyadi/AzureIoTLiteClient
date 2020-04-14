#include "AzureIoTCentralClient.h"

#if defined(RISCV)
#include "wifi/DxWiFi.h"
#include "rtc.h"
#endif

#include <assert.h>
#include "iotc_json.h"
#include "IotcUtils.h"
#include "TimeUtils.h"

#define MUST_CALL_AFTER_CONNECT(mqttClient)                 \
    if (mqttClient == NULL) {                               \
        IOTC_LOG(F("ERROR: Client was not connected."));    \
        return 1;                                           \
    }                                                       \
    else {                                                  \
        if (!mqttClient->connected()) {                     \
            return 1;                                       \
        }                                                   \
    }

#define CHECK_NOT_NULL(x)                   \
    if (x == NULL) {                        \
        IOTC_LOG(F(TO_STRING_(x) "is NULL"));   \
        return 1;                           \
    }

#define CHECK_NOT_NULL_RET_BOOL(x)          \
    if (x == NULL) {                        \
        IOTC_LOG(F(TO_STRING_(x) "is NULL"));   \
        return false;                       \
    }

AzureIoTCentralClient::AzureIoTCentralClient(Client &client) : client_(client) {
}

AzureIoTCentralClient::~AzureIoTCentralClient() {

    if (mqttClient_ != NULL) {
        // delete mqttClient_;
        mqttClient_ = NULL;
    }
}

bool AzureIoTCentralClient::begin(AzureIoTConfig_t *config)
{
    setLogLevel(IOTC_LOGGING_ALL);
    
    config_ = config;

    changeEventTo(AzureIoTClientEventNTPSyncing);
    if (requestTime()) {
        changeEventTo(AzureIoTClientEventNTPSyncSuccess);
    }

    return true;
}

bool AzureIoTCentralClient::connect(AzureIoTConfig_t *config) {
    if (config != NULL) {
        config_ = config;
    }

    if (currentClientEvent_ == AzureIoTClientEventNTPSyncSuccess) {
        return doConnect();
    }

    return false;
}

bool AzureIoTCentralClient::disconnect() {

    if (mqttClient_) {
        if (mqttClient_->connected()) {
            mqttClient_->disconnect();
        }
        delete mqttClient_;
        mqttClient_ = NULL;
    }

    if (client_) {
        client_.stop();
    }

    notifyConnectionStatusCallback(AzureIoTConnectionDisconnected);

    return true;
}

bool AzureIoTCentralClient::run()
{
    if (currentClientEvent_ == AzureIoTClientEventNTPSyncing) {
        if (requestTime()) {
            changeEventTo(AzureIoTClientEventNTPSyncSuccess);
        }
        else {
            // delay ?
            delay(5000);
            return false;
        }
    }

    if (currentClientEvent_ == AzureIoTClientEventNTPSyncSuccess) {
        // Now connect
        return doConnect();
    }

    if (currentClientEvent_ == AzureIoTClientEventDisconnected) {
        if (millis() - lastReconnectAttempt_ > 5000) {
            lastReconnectAttempt_ = millis();
//            IOTC_LOG("MQTT reconnecting from LOOP...");

            // Attempt to reconnect here

//            if (reconnectMqtt()) {
//                lastReconnectAttempt_ = 0;
//            }
            return false;
        }
    }

    if (currentClientEvent_ == AzureIoTClientEventConnected) {
        if (millis() - lastConnCheck_ > 1000) {
            lastConnCheck_ = millis();
            if (!mqttClient_->connected()) {
                changeEventTo(AzureIoTClientEventDisconnected);
                notifyConnectionStatusCallback(AzureIoTConnectionDisconnected);
                return false;

            } else {
//              mqttClient_->loop();
            }
        }

        mqttClient_->loop();
    }

    return true;
}


bool AzureIoTCentralClient::sendTelemetry(const char *payload, unsigned length) {
    return sendTelemetryWithSystemProperties(payload, length, NULL, 0);
}

bool AzureIoTCentralClient::sendProperty(const char *payload, unsigned length) {
    CHECK_NOT_NULL_RET_BOOL(payload);
    int ret = iotcSendProperty(payload, length);
    return (ret == 0);
}

bool AzureIoTCentralClient::sendEvent(const char *payload, unsigned length) {
    return sendTelemetry(payload, length);
}

bool AzureIoTCentralClient::sendState(const char *payload, unsigned length) {
    return sendTelemetry(payload, length);
    return false;
}

bool AzureIoTCentralClient::sendTelemetryWithSystemProperties(const char *payload, unsigned length,
                                                              const char *sysPropPayload,
                                                              unsigned sysPropPayloadLength) {
    CHECK_NOT_NULL_RET_BOOL(payload);
    int ret = iotcSendTelemetryWithSystemProperties(payload, length, sysPropPayload, sysPropPayloadLength);
    return (ret == 0);
}


bool AzureIoTCentralClient::setLogging(IotcLogLevel level) {
    if (level < IOTC_LOGGING_DISABLED || level > IOTC_LOGGING_ALL) {
        IOTC_LOG(F("ERROR: (setLogging) invalid argument. ERROR:0x0001"));
        return false;
    }

    setLogLevel(level);
    return true;
}

bool AzureIoTCentralClient::reconnectMqtt(const char *deviceId, const char *user, const char *pass) {

    if (mqttClient_ == NULL) {
        return false;
    }

    IOTC_LOG("Reconnecting to MQTT...");
//    IOTC_LOG("id: %s, \r\n  user: %s, \r\n  pass: %s", deviceId, user, pass);

    int retry = 0;
    while (retry < 10 && !mqttClient_->connected()) {
        if (mqttClient_->connect(deviceId, user, pass)) {
            IOTC_LOG("MQTT connected");
        }
        else {
            IOTC_LOG("MQTT NOT connected. RC = %d", mqttClient_->state());
            delay(2000);
            retry++;
        }
    }

    if (retry >= 10) {
        notifyConnectionStatusCallback(AzureIoTConnectionBadCredential);
        return false;
    }

    // Add subscriptions
    const unsigned bufferLength = strlen(deviceId) + STRING_BUFFER_64;
    AzureIOT::StringBuffer buffer(bufferLength);

    int errorCode = 0;
    buffer.setLength(snprintf(*buffer, bufferLength, "devices/%s/messages/events/#", deviceId));
    errorCode = mqttClient_->subscribe(*buffer);

    buffer.setLength(snprintf(*buffer, bufferLength, "devices/%s/messages/devicebound/#", deviceId));
    errorCode += mqttClient_->subscribe(*buffer);

    errorCode += mqttClient_->subscribe("$iothub/twin/PATCH/properties/desired/#");  // twin desired property changes

    errorCode += mqttClient_->subscribe("$iothub/twin/res/#");  // twin properties response
    errorCode += mqttClient_->subscribe("$iothub/methods/POST/#");

    if (errorCode < 5) {
        IOTC_LOG("ERROR: mqttClient couldn't subscribe to twin/methods etc. Error code sum => %d", errorCode);
    }

    return mqttClient_->connected();
}

bool AzureIoTCentralClient::doConnect()
{
    if (currentClientEvent_ == AzureIoTClientEventConnected) {
        return true;
    }

    changeEventTo(AzureIoTClientEventConnecting);

    // Query IoT Hub hostname
    AzureIOT::StringBuffer tmpHostname(STRING_BUFFER_128);
    if (queryIoTHubHostname(*tmpHostname) == 1) {
        return false;
    }

    AzureIOT::StringBuffer deviceId;
    AzureIOT::StringBuffer hostName;
    AzureIOT::StringBuffer username;
    AzureIOT::StringBuffer password;

    if (config_->connectType == AZURE_IOTC_CONNECT_CONNECTION_STRING) {
        getUsernameAndPasswordFromConnectionString(config_->connectionString, strlen(config_->connectionString),
                                                    hostName, deviceId,
                                                    username, password);
    }
    else if (config_->connectType == AZURE_IOTC_CONNECT_SYMM_KEY) {
        AzureIOT::StringBuffer connString(STRING_BUFFER_256);
        int rc = snprintf(*connString, STRING_BUFFER_256,
                        "HostName=%s;DeviceId=%s;SharedAccessKey=%s",
                        *tmpHostname, config_->deviceId, config_->deviceKey);
        assert(rc > 0 && rc < STRING_BUFFER_256);
        connString.setLength(rc);

        getUsernameAndPasswordFromConnectionString(*connString, rc, hostName, deviceId, username, password);
    }
    else if (config_->connectType == AZURE_IOTC_CONNECT_X509_CERT) {
        IOTC_LOG(F("ERROR: IOTC_CONNECT_X509_CERT NOT IMPLEMENTED"));
        notifyConnectionStatusCallback(AzureIoTConnectionDeviceDisabled);
        return false;
    }

//    client_.stop();
//    int retry = 0;
//    while (retry < 5 && !client_.connect(*hostName, AZURE_MQTT_SERVER_PORT))
//        retry++;
//
//    if (!client_.connected())
//    {
//        IOTC_LOG("ERROR: Failed to connect to IoTHub endpoint: %s", *hostName);
//        return false;
//    }

    if (mqttClient_ == NULL) {
        mqttClient_ = new PubSubClient(*hostName, AZURE_MQTT_SERVER_PORT, client_);

        // Add callback
        using namespace std::placeholders;
        mqttClient_->setCallback(std::bind(&AzureIoTCentralClient::mqttCallback, this, _1, _2, _3));
    }

    // Do actual MQTT connect
    bool _mqttConncted = reconnectMqtt(*deviceId, *username, *password);
    if (!_mqttConncted) {
        // Clean up
        // delete mqttClient_;
        mqttClient_ = NULL;

        return false;
    }

    iotcRequestDeviceSetting();

    changeEventTo(AzureIoTClientEventConnected);
    notifyConnectionStatusCallback(AzureIoTConnectionOK);

    return true;
}


void AzureIoTCentralClient::mqttCallback(char *topic, uint8_t *payloadBuf, unsigned int payloadLength) {

    if (!topic || strlen(topic) == 0) {
        return;
    }

    unsigned long topic_length = strlen(topic);

    assert(topic != NULL);
    AzureIOT::StringBuffer topicName(topic, topic_length);
    AzureIOT::StringBuffer payload;
    if (payloadLength) {
        payload.initialize((const char*)payloadBuf, payloadLength);
    }

    if (topicName.startsWith("$iothub/twin", strlen("$iothub/twin"))) {
        handleDeviceTwinGetState(topicName, payload);
    }
    else if (topicName.startsWith("$iothub/methods", strlen("$iothub/methods"))) {

        int index = topicName.indexOf("$rid=", 5, 0);
        if (index == -1) {
            IOTC_LOG(F("ERROR: corrupt C2D message topic => %s"), *topicName);
            return;
        }

        const char *topicId = (*topicName + index + 5);
        const char *topicTemplate = "$iothub/methods/POST/";
        const int topicTemplateLength = strlen(topicTemplate);
        index = topicName.indexOf("/", 1, topicTemplateLength + 1);
        if (index == -1) {
            IOTC_LOG(F("ERROR: corrupt C2D message topic (methodName) => %s"), *topicName);
            return;
        }

        AzureIOT::StringBuffer methodName(topic + topicTemplateLength,
                                          index - topicTemplateLength);

        const char *constResponse = "{}";
        char *response = NULL;
        size_t respSize = 0;
        int rc = onCloudCommand(*methodName, (const char *)payloadBuf, payloadLength, &response, &respSize);
        if (respSize == 0) {
            respSize = 2;
        } else {
            // If any response from callback handler
            constResponse = response;
        }

        AzureIOT::StringBuffer respTopic(STRING_BUFFER_128);
        respTopic.setLength(snprintf(*respTopic, STRING_BUFFER_128, "$iothub/methods/res/%d/?$rid=%s", rc, topicId));

        if (!mqttClient_->publish(*respTopic, constResponse, respSize)) {
            IOTC_LOG("ERROR: mqtt_publish has failed during C2D with response "
                     "topic '%s' and response '%s'",
                     *respTopic, constResponse);
        }

        if (response != constResponse) {
            IOTC_FREE(response);
        }
    }
    else {
        IOTC_LOG(F("ERROR: unknown twin topic: %s, msg: %s"), topic, ((payloadLength > 0)? (const char*)payloadBuf: "NULL"));
    }
}

void AzureIoTCentralClient::changeEventTo(AzureIoTClientEvent_e event)
{
    currentClientEvent_ = event;

     if (clientEventCallback_) {
         clientEventCallback_(event);
     }
}

/***
 * Utility methods
 ***/

//long getEpoch() {
//    auto *dateTime = rtc_timer_get_tm();
//    time_t timeSinceEpoch = mktime(dateTime);
//    DEBUGLOG("Epoch: %lu\n\n", timeSinceEpoch);
//
//    return timeSinceEpoch;
//}

int AzureIoTCentralClient::queryIoTHubHostname(char *hostName)
{
    size_t authHeaderSize = 0;
    AzureIOT::StringBuffer authHeader(STRING_BUFFER_256);
    IOTC_LOG("- iotc.dps : getting auth...");
    if (createDPSAuthString(config_->scopeId, config_->deviceId, config_->deviceKey, *authHeader, STRING_BUFFER_256, authHeaderSize))
    {
        Serial.println("ERROR: getDPSAuthString has failed");
        return 1;
    }
    else {
        IOTC_LOG("Authorization: %s", *authHeader);
    }

    // char *authHeaderSent =  "SharedAccessSignature sr=0ne000E75A0%2Fregistrations%2F2i3rmwz5en8&sig=02SILXHFUJjAiLkhKpJTA%2BEltDI3Ws6JXKwqDsQo7Hs%3D&se=1586798959&skn=registration";
    char *authHeaderSent =  *authHeader;

    IOTC_LOG("- iotc.dps : getting operation id...");
    AzureIOT::StringBuffer operationId(STRING_BUFFER_64);
    int retval = 0;

    // if (httpsClient_ == NULL) {
    //     IOTC_LOG("Creating HttpsClient");
    //     httpsClient_ = new HttpsClient(client_, AZURE_IOT_CENTRAL_DPS_ENDPOINT, 443);
    // }
    
    // if (queryOperationId(&client_, config_->scopeId, config_->deviceId, authHeader2, *operationId, NULL) == 0) {
    // // if (queryOperationId(&client_, config_->scopeId, config_->deviceId, authHeader2, "4.430a1a615e0874a3.baec48c5-7f96-4a3e-ae9f-16be7e5ce929", hostName) == 0) {
    //     IOTC_LOG("Hostname: %s", hostName);
    // }

    if ((retval = queryOperationId(&client_, config_->scopeId, config_->deviceId, authHeaderSent, *operationId, NULL)) == 0)
    {
        IOTC_LOG("Operations ID: %s\r\n", *operationId);
        delay(2000);

        IOTC_LOG("- iotc.dps : getting host name...");
        int retry = 0;
        while (retry < 5 && queryOperationId(&client_, config_->scopeId, config_->deviceId, authHeaderSent, *operationId, hostName) == 1) {
            delay(2000);
            retry++;
        }

        IOTC_LOG("IoT Hub Hostname: %s", hostName);
        if (strlen(hostName) == 0) {
            return 1;
        }

        return 0;
    }

    return 1;
}

int AzureIoTCentralClient::iotcRequestDeviceSetting() {
    MUST_CALL_AFTER_CONNECT(mqttClient_);

#define getDeviceSettingsTopic "$iothub/twin/GET/?$rid=0"

    if (!mqttClient_->publish(getDeviceSettingsTopic, "", 0)) {
        IOTC_LOG("ERROR: (iotcRequestDeviceSetting) MQTTClient publish has failed.");
        return 1;
    }

#undef getDeviceSettingsTopic

    return 0;;
}

int AzureIoTCentralClient::onCloudCommand(const char *method_name, const char *payload, size_t size, char **response, size_t *resp_size) {

    assert(response != NULL && resp_size != NULL);
    *response = NULL;
    *resp_size = 0;

    if (callbacks_[::AzureIoTCallbackCommand]) {
        AzureIoTCallbackInfo_t cbInfo;
        cbInfo.eventName = "Command";
        cbInfo.tag = method_name;
        cbInfo.payload = (const char *)payload;
        cbInfo.payloadLength = (unsigned)size;
//        cbInfo.appContext = internal->callbacks[/*IOTCallbacks::*/ ::Command].appContext;
        cbInfo.statusCode = 0;
        cbInfo.callbackResponse = NULL;

        // Call callback
        callbacks_[/*AzureIoTCallback_e::*/ ::AzureIoTCallbackCommand](AzureIoTCallbackCommand, &cbInfo);

        if (cbInfo.callbackResponse != NULL) {
            *response = (char *)cbInfo.callbackResponse;
            *resp_size = strlen((char *)cbInfo.callbackResponse);
        }

        return 200;
    }

    return 500;
}

int AzureIoTCentralClient::handleDeviceTwinGetState(AzureIOT::StringBuffer &topicName,
                                                        AzureIOT::StringBuffer &payload) {

    if (payload.getLength() == 0) {
        return 1;
    }

    jsobject_t desired, outDesired, outReported;
    jsobject_initialize(&desired, *payload, payload.getLength());

    if (jsobject_get_object_by_name(&desired, "desired", &outDesired) != -1 &&
        jsobject_get_object_by_name(&desired, "reported", &outReported) != -1) {

        notifySettingsUpdatedCallback(topicName, "twin", payload);

    } else {
        for (unsigned i = 0, count = jsobject_get_count(&desired); i < count;
             i += 2) {
            char *itemName = jsobject_get_name_at(&desired, i);
            if (itemName != NULL && itemName[0] != '$') {
                notifySettingsUpdatedCallback(topicName, itemName, payload);
            }
            if (itemName) IOTC_FREE(itemName);
        }
    }
    jsobject_free(&outReported);
    jsobject_free(&desired);
    jsobject_free(&outDesired);

    return 0;
}

int AzureIoTCentralClient::iotcEchoDesired(const char *propertyName, AzureIOT::StringBuffer &message, const char *status, int statusCode) {

    jsobject_t rootObject;
    jsobject_initialize(&rootObject, *message, message.getLength());
    jsobject_t propertyNameObject;

    if (jsobject_get_object_by_name(&rootObject, propertyName,
                                    &propertyNameObject) != 0) {
        IOTC_LOG(
                "ERROR: echoDesired has failed due to payload doesn't include the "
                "property => %s",
                *message);
        jsobject_free(&rootObject);
        return 1;
    }

    char *value = jsobject_get_data_by_name(&propertyNameObject, "value");
    double desiredVersion = jsobject_get_number_by_name(&rootObject, "$version");

    const char *echoTemplate =
            "{\"%s\":{\"value\":%s,\"statusCode\":%d,\
\"status\":\"%s\",\"desiredVersion\":%d}}";
    uint32_t buffer_size = strlen(echoTemplate) + strlen(value) +
                           3 /* statusCode */
                           + 32 /* status */ + 23 /* version max */;
    buffer_size = iotc_min(buffer_size, 512);
    AzureIOT::StringBuffer buffer(buffer_size);

    size_t size = snprintf(*buffer, buffer_size, echoTemplate, propertyName,
                           value, statusCode, status, (int)desiredVersion);
    buffer.setLength(size);

    IOTC_FREE(value);

    const char *topicName = "$iothub/twin/PATCH/properties/reported/?$rid=%d";
    AzureIOT::StringBuffer topic(
            strlen(topicName) +
            21);  // + 2 for %d == +23 in case requestId++ overflows
    topic.setLength(snprintf(*topic, topic.getLength(), topicName, messageId_++));

    if (!mqttClient_->publish(*topic, *buffer, size)) {
        IOTC_LOG("ERROR: (echoDesired) MQTTClient publish has failed => %s", *buffer);
    }
    jsobject_free(&propertyNameObject);
    jsobject_free(&rootObject);

    return 0;
}

int AzureIoTCentralClient::iotcSendTelemetryWithSystemProperties(const char *payload, unsigned length,
                                                                 const char *sysPropPayload,
                                                                 unsigned sysPropPayloadLength) {

    MUST_CALL_AFTER_CONNECT(mqttClient_);

    if ((sysPropPayload == NULL && sysPropPayloadLength != 0) ||
        (sysPropPayload != NULL && sysPropPayloadLength == 0)) {
        IOTC_LOG("ERROR: (iotcSendTelemetryWithSystemProperties) sysPropPayload "
                 "doesn't match with sysPropPayloadLength");
        return 1;
    }

    if ((sysPropPayload == NULL && sysPropPayloadLength != 0) ||
        (sysPropPayload != NULL && sysPropPayloadLength == 0)) {
        IOTC_LOG("ERROR: (iotcSendTelemetryWithSystemProperties) sysPropPayload "
                 "doesn't match with sysPropPayloadLength");
        return 1;
    }

    AzureIOT::StringBuffer topic(strlen(config_->deviceId) + strlen("devices/ /messages/events/") + sysPropPayloadLength);

    if (sysPropPayloadLength > 0) {
        topic.setLength(snprintf(*topic, topic.getLength(), "devices/%s/messages/events/%.*s", config_->deviceId, sysPropPayloadLength, sysPropPayload));
    } else {
        topic.setLength(snprintf(*topic, topic.getLength(), "devices/%s/messages/events/", config_->deviceId));
    }

    if (!mqttClient_->publish(*topic, payload, length)) {
        IOTC_LOG("ERROR: (iotcSendTelemetryWithSystemProperties) MQTTClient publish has failed => %s", payload);
        return 1;
    }

    notifyConfirmationCallback(payload, length);
    return 0;
}

int AzureIoTCentralClient::iotcSendProperty(const char *payload, unsigned length) {
    MUST_CALL_AFTER_CONNECT(mqttClient_);

    const char *topicName = "$iothub/twin/PATCH/properties/reported/?$rid=%d";
    AzureIOT::StringBuffer topic(strlen(topicName) + 21);  // + 2 for %d == +23 in case requestId++ overflows
    topic.setLength(snprintf(*topic, topic.getLength(), topicName, messageId_++));

    if (!mqttClient_->publish(*topic, payload, length)) {
        IOTC_LOG("ERROR: (iotc_send_property) MQTTClient publish has failed => %s", payload);
        return 1;
    }

    notifyConfirmationCallback(payload, length);
    return 0;
}

void AzureIoTCentralClient::notifySettingsUpdatedCallback(AzureIOT::StringBuffer &topicName, const char *propertyName, AzureIOT::StringBuffer &payload) {

    const char *response = "completed";
    if (callbacks_[/*AzureIoTCallback_e::*/ ::AzureIoTCallbackSettingsUpdated]) {
        AzureIoTCallbackInfo_t info;
        info.eventName = "SettingsUpdated";
        info.tag = propertyName;
        info.payload = *payload;
        info.payloadLength = payload.getLength();
//        info.appContext = internal->callbacks[/*IOTCallbacks::*/ ::SettingsUpdated].appContext;
        info.statusCode = 200;
        info.callbackResponse = NULL;

        callbacks_[/*IOTCallbacks::*/ ::AzureIoTCallbackSettingsUpdated](AzureIoTCallbackSettingsUpdated, &info);

        if (topicName.startsWith(
                "$iothub/twin/PATCH/properties/desired/",
                strlen("$iothub/twin/PATCH/properties/desired/"))) {

            if (info.callbackResponse) {
                response = (const char *)info.callbackResponse;
            }

            iotcEchoDesired(propertyName, payload, response, info.statusCode);
        }
    }
}

void AzureIoTCentralClient::notifyConfirmationCallback(const char *buffer, size_t size) {
    int result = 0;

    if (callbacks_[/*AzureIoTCallback_e::*/ ::AzureIoTCallbackMessageSent]) {
        AzureIoTCallbackInfo_t info;
        info.eventName = "MessageSent";
        info.tag = NULL;
        info.payload = (const char *)buffer;
        info.payloadLength = (unsigned)size;
//        info.appContext = internal->callbacks[/*AzureIoTCallback_e::*/ ::AzureIoTCallbackMessageSent].appContext;
        info.statusCode = (int)result;
        info.callbackResponse = NULL;

        callbacks_[/*IOTCallbacks::*/ ::AzureIoTCallbackMessageSent](AzureIoTCallbackSettingsUpdated, &info);
    }
}

void AzureIoTCentralClient::notifyErrorCallback(const char *message) {
    if (callbacks_[/*AzureIoTCallback_e::*/ ::AzureIoTCallbackError]) {
        AzureIoTCallbackInfo_t info;
        info.eventName = "Error";
        info.tag = message;
        info.payload = NULL;
        info.payloadLength = 0;
//        info.appContext = internal->callbacks[/*AzureIoTCallback_e::*/ ::AzureIoTCallbackError].appContext;
        info.statusCode = 1;
        info.callbackResponse = NULL;

        callbacks_[/*IOTCallbacks::*/ ::AzureIoTCallbackMessageSent](AzureIoTCallbackError, &info);
    }
}

void AzureIoTCentralClient::notifyConnectionStatusCallback(AzureIoTConnectionState_e status) {
    if (callbacks_[/*AzureIoTCallback_e::*/ ::AzureIoTCallbackConnectionStatus]) {
        AzureIoTCallbackInfo_t info;
        info.eventName = "ConnectionStatus";
        info.tag = NULL;
        info.payload = NULL;
        info.payloadLength = 0;
//        info.appContext = internal->callbacks[/*AzureIoTCallback_e::*/ ::AzureIoTCallbackConnectionStatus].appContext;
        info.statusCode = status;
        info.callbackResponse = NULL;

        callbacks_[/*IOTCallbacks::*/ ::AzureIoTCallbackMessageSent](AzureIoTCallbackConnectionStatus, &info);
    }
}






