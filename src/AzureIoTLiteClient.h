#ifndef _AZURE_IOTC_CLIENT_H
#define _AZURE_IOTC_CLIENT_H

#include <Arduino.h>
#include "PubSubClient.h"
#include "IotcDefs.h"
#include "StringBuffer.h"

#undef min
#undef max

#include <functional>

enum AzureIoTConnectType_e {
    AZURE_IOTC_CONNECT_SYMM_KEY,
    AZURE_IOTC_CONNECT_X509_CERT,
    AZURE_IOTC_CONNECT_CONNECTION_STRING
};

enum AzureIoTCallbacks_e {
    AzureIoTCallbackConnectionStatus = 0x01,
    AzureIoTCallbackMessageSent,
    AzureIoTCallbackCommand,
    AzureIoTCallbackError,
    AzureIoTCallbackSettingsUpdated
};

enum AzureIoTConnectionState_e {
    AzureIoTConnectionOK,
    AzureIoTConnectionDisconnected,
    AzureIoTConnectionBadCredential,
    AzureIoTConnectionDeviceDisabled,
    AzureIoTConnectionExpiredSASToken,
    AzureIoTConnectionRetryExpired,
    AzureIoTConnectionNoNetwork,
    AzureIoTConnectionCommError
};

struct AzureIoTCallbackInfo_t {
    const char* eventName;
    const char* tag;
    const char* payload;
    unsigned payloadLength;

    void* appContext;

    int statusCode;
    void* callbackResponse;
};

struct AzureIoTConfig_t {
    const char *scopeId;
    const char *deviceId;
    const char *deviceKey;
    AzureIoTConnectType_e connectType;

    const char *connectionString;

    AzureIoTConfig_t() {}
    virtual ~AzureIoTConfig_t() = default;

	AzureIoTConfig_t(const char *scope, const char *did, const char *dky, AzureIoTConnectType_e connType):
    scopeId(scope), deviceId(did), deviceKey(dky), connectType(connType) 
    {}

    AzureIoTConfig_t(const char *connString): connectionString(connString) {
        connectType = AZURE_IOTC_CONNECT_CONNECTION_STRING;
    }
};

class AzureIoTLiteClient {

public:

    enum AzureIoTClientEvent_e {
        AzureIoTClientEventUnknown,
        AzureIoTClientEventNTPSyncing,
        AzureIoTClientEventNTPSyncSuccess,
        AzureIoTClientEventNTPSyncFailed,
        AzureIoTClientEventConnecting,
        AzureIoTClientEventConnected,
        AzureIoTClientEventDisconnected
    };

    typedef std::function<void(const AzureIoTCallbacks_e callbackType, const AzureIoTCallbackInfo_t *callbackInfo)> AzureIoTCallback;
    typedef std::function<void(const AzureIoTClientEvent_e eventType)> AzureIoTClientEventCallback;

    AzureIoTLiteClient(Client& client);
    ~AzureIoTLiteClient();

    bool begin(AzureIoTConfig_t *config);
    bool run();
    bool connect(AzureIoTConfig_t *config = NULL);
    bool disconnect();

    void setCallback(const AzureIoTCallbacks_e callbackType, AzureIoTCallback cb) {
        callbacks_[callbackType] = cb;
    }

    void onClientEvent(AzureIoTClientEventCallback cb) {
        clientEventCallback_ = cb;
    }

    bool sendTelemetry(const char* payload, unsigned length);
    bool sendProperty(const char* payload, unsigned length);
    bool sendEvent(const char* payload, unsigned length);
    bool sendState(const char* payload, unsigned length);
    bool sendTelemetryWithSystemProperties(const char *payload,
                                           unsigned length,
                                           const char *sysPropPayload,
                                           unsigned sysPropPayloadLength);

    bool setLogging(IotcLogLevel level);

protected:
    
    PubSubClient *mqttClient_ = NULL;
    Client& client_;

    AzureIoTConfig_t *config_ = NULL;
    AzureIoTClientEvent_e currentClientEvent_ = AzureIoTClientEventUnknown;

    AzureIOT::StringBuffer currentDeviceId_;
    AzureIOT::StringBuffer currentUsername_;
    AzureIOT::StringBuffer currentPassword_;
    AzureIOT::StringBuffer currentHostname_;

    uint32_t messageId_ = 0;
    unsigned long lastConnCheck_ = 0, lastReconnectAttempt_ = 0;

    AzureIoTCallback callbacks_[8];
    AzureIoTClientEventCallback clientEventCallback_ = NULL;

    bool reconnectMqtt(const char *id, const char *user, const char *pass);
    bool doConnect();
    void changeEventTo(AzureIoTClientEvent_e event);
    void mqttCallback(char *topic, uint8_t *payload, unsigned int payloadLength);

    int queryIoTHubHostname(char *hostName);

    int iotcRequestDeviceSetting();
    int handleDeviceTwinGetState(AzureIOT::StringBuffer &topicName, AzureIOT::StringBuffer &payload);
    int iotcEchoDesired(const char *propertyName, AzureIOT::StringBuffer &message, const char *status, int statusCode);
    int iotcSendTelemetryWithSystemProperties(const char *payload,
                                              unsigned length,
                                              const char *sysPropPayload,
                                              unsigned sysPropPayloadLength);
    int iotcSendProperty(const char *payload, unsigned length);

    int onCloudCommand(const char *method_name, const char *payload, size_t size, char **response, size_t *resp_size);
    void notifyConnectionStatusCallback(AzureIoTConnectionState_e status);
    void notifySettingsUpdatedCallback(AzureIOT::StringBuffer &topicName, const char *propertyName, AzureIOT::StringBuffer &payload);
    void notifyConfirmationCallback(const char *buffer, size_t size);
    void notifyErrorCallback(const char *message);
};


#endif