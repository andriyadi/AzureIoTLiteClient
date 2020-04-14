#ifndef _IOTC_UTILS_H
#define _IOTC_UTILS_H

#include <assert.h>
#include <limits.h>
#include <stddef.h>  // size_t etc.

#include "StringBuffer.h"
#include "IotcDefs.h"

void setLogLevel(IotcLogLevel l);
IotcLogLevel getLogLevel();

//#ifdef ARDUINO
#include <Arduino.h>
#define IOTC_LOG(...)                               \
    if (getLogLevel() > IOTC_LOGGING_DISABLED) {    \
        Serial.printf("  - ");                      \
        Serial.printf(__VA_ARGS__);                 \
        Serial.printf("\r\n");                      \
    }
#undef F
#define F(x) x
//#endif

long getEpoch();

void setSASExpires(unsigned e);

int getUsernameAndPasswordFromConnectionString(
    const char *connectionString, size_t connectionStringLength,
    AzureIOT::StringBuffer &hostName, AzureIOT::StringBuffer &deviceId,
    AzureIOT::StringBuffer &username, AzureIOT::StringBuffer &password);

int createDPSAuthString(const char *scopeId, const char *deviceId, const char *key, char *buffer, int bufferSize, size_t &outLength);
//int queryOperationId(HttpsClient *client, const char *scopeId, const char *deviceId, char *authHeader, char *operationId);

int queryOperationId(Client *client, const char *scopeId, const char *deviceId, char *authHeader, char *operationId, char* hostName);

#endif