#include "IotcUtils.h"

IotcLogLevel gLogLevel = IOTC_LOGGING_DISABLED;

void setLogLevel(IotcLogLevel l) { gLogLevel = l; }
IotcLogLevel getLogLevel() { return gLogLevel; }

static unsigned SAS_EXPIRES = 21600;
void setSASExpires(unsigned e) { SAS_EXPIRES = e; }

int getUsernameAndPasswordFromConnectionString(
    const char *connectionString, size_t connectionStringLength,
    AzureIOT::StringBuffer &hostName, AzureIOT::StringBuffer &deviceId,
    AzureIOT::StringBuffer &username, AzureIOT::StringBuffer &password)
{

    // TODO: improve this so we don't depend on a particular order in connection
    // string
    AzureIOT::StringBuffer connStr(connectionString, connectionStringLength);

    int32_t hostIndex = connStr.indexOf(HOSTNAME_STRING, HOSTNAME_LENGTH);
    size_t length = connStr.getLength();
    if (hostIndex != 0)
    {
        IOTC_LOG("ERROR: connectionString doesn't start with HostName=  RESULT:%d", hostIndex);
        return 1;
    }

    int32_t deviceIndex = connStr.indexOf(DEVICEID_STRING, DEVICEID_LENGTH);
    if (deviceIndex == -1)
    {
        IOTC_LOG("ERROR: ;DeviceId= not found in the connectionString");
        return 1;
    }

    int32_t keyIndex = connStr.indexOf(KEY_STRING, KEY_LENGTH);
    if (keyIndex == -1)
    {
        IOTC_LOG("ERROR: ;SharedAccessKey= not found in the connectionString");
        return 1;
    }

    hostName.initialize(connectionString + HOSTNAME_LENGTH,
                        deviceIndex - HOSTNAME_LENGTH);
    deviceId.initialize(connectionString + (deviceIndex + DEVICEID_LENGTH),
                        keyIndex - (deviceIndex + DEVICEID_LENGTH));

    AzureIOT::StringBuffer keyBuffer(length - (keyIndex + KEY_LENGTH));
    memcpy(*keyBuffer, connectionString + (keyIndex + KEY_LENGTH),
           keyBuffer.getLength());

    AzureIOT::StringBuffer hostURLEncoded(hostName);
    hostURLEncoded.urlEncode();

    unsigned long expiresSecond = getEpoch() + SAS_EXPIRES;

    AzureIOT::StringBuffer stringToSign(hostURLEncoded.getLength() +
                                        STRING_BUFFER_128);
    AzureIOT::StringBuffer deviceIdEncoded(deviceId);
    deviceIdEncoded.urlEncode();
    // size_t keyLength = snprintf(*stringToSign, stringToSign.getLength(), "%s%s%s\n%lu000",
    //                    *hostURLEncoded, "%2Fdevices%2F", *deviceIdEncoded, expires);
    size_t keyLength = snprintf(*stringToSign, stringToSign.getLength(), "%s%s%s\n%lu",
                       *hostURLEncoded, "%2Fdevices%2F", *deviceIdEncoded, expiresSecond);
    stringToSign.setLength(keyLength);

    keyBuffer.base64Decode();
    stringToSign.hash(*keyBuffer, keyBuffer.getLength());
    if (!stringToSign.base64Encode() || !stringToSign.urlEncode())
    {
        IOTC_LOG("ERROR: stringToSign base64Encode / urlEncode has failed.");
        return 1;
    }

    AzureIOT::StringBuffer passwordBuffer(STRING_BUFFER_512);
    size_t passLength = snprintf(
        *passwordBuffer, STRING_BUFFER_512,
        // "SharedAccessSignature sr=%s%s%s&sig=%s&se=%lu000", *hostURLEncoded,
        "SharedAccessSignature sr=%s%s%s&sig=%s&se=%lu", *hostURLEncoded,
        "%2Fdevices%2F", *deviceIdEncoded, *stringToSign, expiresSecond);

    assert(passLength && passLength < STRING_BUFFER_512);
    passwordBuffer.setLength(passLength);

    password.initialize(*passwordBuffer, passwordBuffer.getLength());

    const char *usernameTemplate = "%s/%s/api-version=2016-11-14";
    AzureIOT::StringBuffer usernameBuffer(
        (strlen(usernameTemplate) - 3 /* %s twice */) + hostName.getLength() +
        deviceId.getLength());

    size_t expLength = snprintf(*usernameBuffer, usernameBuffer.getLength(),
                                usernameTemplate, *hostName, *deviceId);
    assert(expLength <= usernameBuffer.getLength());

    username.initialize(*usernameBuffer, usernameBuffer.getLength());

    IOTC_LOG("\r\n"
                "hostname: %s\r\n"
                "deviceId: %s\r\n"
                "username: %s\r\n"
                "password: %s\r\n",
             *hostName, *deviceId, *username, *password);

    return 0;
}

int createDPSAuthString(const char *scopeId, const char *deviceId, const char *key, char *buffer, int bufferSize, size_t &outLength)
{
    // auto *dateTime = rtc_timer_get_tm();
    // time_t timeSinceEpoch = mktime(dateTime);
    // unsigned long expiresSecond = timeSinceEpoch + (24*2600);
    // DEBUGLOG("Epoch: %lu -> %lu\n\n", timeSinceEpoch, expiresSecond);
    // assert(expiresSecond > (24*2600));

//    unsigned long expiresSecond = 1586798959;
    unsigned long expiresSecond = getEpoch() + SAS_EXPIRES;
    IOTC_LOG("Expires: %lu", expiresSecond);

    AzureIOT::StringBuffer deviceIdEncoded(deviceId, strlen(deviceId));
    deviceIdEncoded.urlEncode();

    AzureIOT::StringBuffer stringToSign(STRING_BUFFER_256);
    size_t size = snprintf(*stringToSign, STRING_BUFFER_256, "%s%%2Fregistrations%%2F%s", scopeId, *deviceIdEncoded);
    assert(size < STRING_BUFFER_256);
    stringToSign.setLength(size);

    // DEBUGLOG("sts: %s\r\n", *stringToSign);

    AzureIOT::StringBuffer sr(stringToSign);
    // size = snprintf(*stringToSign, STRING_BUFFER_256, "%s\n%lu000", *sr, expiresSecond);
    size = snprintf(*stringToSign, STRING_BUFFER_256, "%s\n%lu", *sr, expiresSecond);
    assert(size < STRING_BUFFER_256);
    stringToSign.setLength(size);

    // DEBUGLOG("sr: %s\r\n", *stringToSign);

    size = 0;
    AzureIOT::StringBuffer keyDecoded(key, strlen(key));
    keyDecoded.base64Decode();
    // DEBUGLOG("key: %s\r\n", *keyDecoded);

    // DEBUGLOG("SHA256 Start\r\n");
    stringToSign.hash(*keyDecoded, keyDecoded.getLength());
    if (!stringToSign.base64Encode() || !stringToSign.urlEncode())
    {
        IOTC_LOG("ERROR: stringToSign base64Encode / urlEncode has failed.");
        return 1;
    }

    Serial.printf("SHA256 result: %s\r\n", *stringToSign);

    // outLength = snprintf(buffer, bufferSize, "authorization: SharedAccessSignature sr=%s&sig=%s&se=%lu000&skn=registration", sr.c_str(), auth.c_str(), expiresSecond);
    // outLength = snprintf(buffer, bufferSize, "SharedAccessSignature sr=%s&sig=%s&se=%lu000&skn=registration", sr.c_str(), auth.c_str(), expiresSecond);
    outLength = snprintf(buffer, bufferSize, "SharedAccessSignature sr=%s&sig=%s&se=%lu&skn=registration", *sr, *stringToSign, expiresSecond);
    assert(outLength > 0 && outLength < bufferSize);
    buffer[outLength] = 0;

    return 0;
}

int queryOperationId(Client *client, const char *scopeId, const char *deviceId, char *authHeader, char *operationId, char *hostName)
{
    int exitCode = 0;

    int retry = 0;
    while (retry < 5 && !client->connect(IOTC_DEFAULT_ENDPOINT, 443))
        retry++;

    if (!client->connected())
    {
        IOTC_LOG("ERROR: DPS endpoint %s call has failed.", hostName == NULL ? "PUT" : "GET");
        return 1;
    }

    IOTC_LOG("Connected to %s", IOTC_DEFAULT_ENDPOINT);
    // IOTC_LOG("SAS: %s", authHeader);

    AzureIOT::StringBuffer tmpBuffer(STRING_BUFFER_1024);

    AzureIOT::StringBuffer deviceIdEncoded(deviceId, strlen(deviceId));
    deviceIdEncoded.urlEncode();

    size_t txDataSize = 0;

    if (hostName == NULL)
    {
        int payloadSize = strlen("{\"registrationId\":\"%s\"}") + strlen(deviceId) - 2;
        assert(payloadSize != 0);
        // tmpBuffer[payloadSize] = '\0';
        // payloadSize += 1;
        // DEBUGLOG("Payload: %s\r\n", tmpBuffer);

        txDataSize = snprintf(*tmpBuffer, STRING_BUFFER_1024,
                              "\
PUT /%s/registrations/%s/register?api-version=%s HTTP/1.1\r\n\
Host: %s\r\n\
Content-Type: application/json; charset=utf-8\r\n\
%s\r\n\
Accept: */*\r\n\
Content-Length: %d\r\n\
Authorization: %s\r\n\
Connection: close\r\n\
\r\n\
{\"registrationId\":\"%s\"}\r\n\r\n",
                              scopeId, *deviceIdEncoded, "2019-03-31", IOTC_DEFAULT_ENDPOINT,
                              AZURE_IOT_CENTRAL_CLIENT_SIGNATURE, payloadSize, authHeader,
                              deviceId);
    }
    else
    {
        txDataSize = snprintf(*tmpBuffer, STRING_BUFFER_1024,
                              "\
GET /%s/registrations/%s/operations/%s?api-version=2018-11-01 HTTP/1.1\r\n\
Host: %s\r\n\
Content-Type: application/json; charset=utf-8\r\n\
%s\r\n\
Accept: */*\r\n\
Authorization: %s\r\n\
Connection: close\r\n\
\r\n\r\n",
                              scopeId, deviceId, operationId, IOTC_DEFAULT_ENDPOINT,
                              AZURE_IOT_CENTRAL_CLIENT_SIGNATURE, authHeader);
    }

    assert(txDataSize != 0 && txDataSize < STRING_BUFFER_1024);
    tmpBuffer.setLength(txDataSize);

    // IOTC_LOG("Request (%ld):\r\n%s", txDataSize, tmpBuffer);

    // client->println(*tmpBuffer); //-> somehow want to work using `write`

    uint8_t writebuf[256];
    int remain = txDataSize;
    uint8_t *buf = (uint8_t *)*tmpBuffer;
    while (remain)
    {
        int toCpy = remain > sizeof(writebuf) ? sizeof(writebuf) : remain;
        memcpy(writebuf, buf, toCpy);
        buf += toCpy;
        remain -= toCpy;
        client->write(writebuf, toCpy);
    }

    int index = 0;
    while (!client->available())
    {
        // while (!client->peek()) {
        delay(10);
        if (index++ > IOTC_SERVER_RESPONSE_TIMEOUT * 100)
        {
            // 20 secs..
            client->stop();
            IOTC_LOG("ERROR: DPS (%s) request has failed. (Server didn't answer within 20 secs.)", (hostName == NULL ? "PUT" : "GET"));
            return 1;
        }
        else
        {
            //DEBUGLOG(".");
        }
    }

    // IOTC_LOG("");
    // IOTC_LOG("Response should be available");

    index = 0;
    bool enableSaving = false;
    while (client->available() && index < STRING_BUFFER_1024 - 1)
    {
        char ch = (char)client->read();
//        Serial.print(ch);
        if (ch == '{' && !enableSaving)
        {
            // DEBUGLOG(">> Saving start\r\n");
            enableSaving = true; // don't use memory for headers
        }

        if (enableSaving)
        {
            (*tmpBuffer)[index++] = ch;
        }
    }

    // while (client->available() && index < TEMP_BUFFER_SIZE - 1) {
    // // while (client->peek() && index < TEMP_BUFFER_SIZE - 1) {
    //     int read = client->read((uint8_t*)tmpBuffer, 512);
    //     DEBUGLOG("R: %d", read);
    //     if (read == -1) {
    //         break;
    //     }
    //     index += min(read, 512);
    //     *tmpBuffer += index;
    // }

    tmpBuffer.setLength(index);

    // IOTC_LOG("\r\nResponse (%d):\r\n%s", index, tmpBuffer);

    // Parse operationId or assignedHub
    const char *lookFor = hostName == NULL ? R"({"operationId":")" : R"("assignedHub":")";
    index = tmpBuffer.indexOf(lookFor, strlen(lookFor), 0);
    if (index == -1)
    {
    error_exit:
        IOTC_LOG("ERROR: DPS (%s) request has failed.\r\n%s", (hostName == NULL ? "PUT" : "GET"), *tmpBuffer);
        exitCode = 1;
        goto exit_operationId;
    }
    else
    {
        index += strlen(lookFor);
        int index2 = tmpBuffer.indexOf("\"", 1, index + 1);
        if (index2 == -1)
        {
            goto error_exit;
        }

        tmpBuffer.setLength(index2);
        strcpy(hostName == NULL ? operationId : hostName, (*tmpBuffer) + index);
    }

exit_operationId:
    client->stop();
    return exitCode;
}

/*
int AzureIoTCentralClient::queryOperationId(HttpsClient *_client, const char *scopeId, const char *deviceId, char *authHeader, char *operationId)
{
// #if defined(RISCV)
//     if (((WiFiEspClient *)client)->connectSSL(AZURE_IOT_CENTRAL_DPS_ENDPOINT, 443))
// #endif

    _client->stop();

    _client->beginRequest();

    // {
        
        DEBUGLOG("Connected to %s\r\n", AZURE_IOT_CENTRAL_DPS_ENDPOINT);
        DEBUGLOG("SAS: %s\r\n", authHeader);

        char tmpBuffer[TEMP_BUFFER_SIZE] = {0};
        // String deviceIdEncoded = urlEncode(deviceId);
        size_t size = snprintf(tmpBuffer, TEMP_BUFFER_SIZE, "/%s/registrations/%s/register?api-version=2019-03-31", scopeId, deviceId);
        assert(size != 0);
        tmpBuffer[size] = 0;

        DEBUGLOG("Path: %s\r\n", tmpBuffer);

        int connRes = _client->put(tmpBuffer);
        if (connRes != HTTP_SUCCESS) {
            // httpClient_->stop();
            // httpClient_ = NULL;
            // wifiClient_ = NULL;
            Serial.println("Error: HTTP failed to PUT");
            return 1;
        }

        // _client->sendHeader("Host", AZURE_IOT_CENTRAL_DPS_ENDPOINT);
        _client->sendHeader("Content-Type", "application/json; charset=utf-8");
        _client->sendHeader("User-Agent", "iot-central-client/1.0");
        _client->sendHeader("Accept", "");
        _client->sendHeader("authorization", authHeader);
        // _client->sendHeader("Connection", "close");

        int payloadSize = snprintf(tmpBuffer, TEMP_BUFFER_SIZE, "{\"registrationId\":\"%s\"}", deviceId);
        assert(payloadSize != 0);
        tmpBuffer[payloadSize] = '\0';
        // payloadSize += 1;
        // String regMessage = tmpBuffer;
        DEBUGLOG("Payload: %s\r\n", tmpBuffer);

        // size = snprintf(tmpBuffer, TEMP_BUFFER_SIZE, "Content-Length: %d", regMessage.length());
        // assert(size != 0);
        // tmpBuffer[size] = 0;
        // client->println(tmpBuffer);
        _client->sendHeader("Content-Length", payloadSize);

        _client->beginBody();
        delay(50);
        _client->write((const uint8_t*)tmpBuffer, payloadSize);

        // uint8_t writebuf[1024*2];
        // int remain = regMessage.length();
        // uint8_t *buf = (uint8_t*)tmpBuffer;
        // while (remain)
        // {
        //     int toCpy = remain > sizeof(writebuf) ? sizeof(writebuf) : remain;
        //     memcpy(writebuf, buf, toCpy);
        //     buf += toCpy;
        //     remain -= toCpy;
        //     client->write(writebuf, toCpy);
        // }


        delay(500); // give 2 secs to server to process
        DEBUGLOG("Waiting....\r\n");

        // memset(tmpBuffer, 0, TEMP_BUFFER_SIZE);
        // int index = 0;
        // while (client->available() && index < TEMP_BUFFER_SIZE - 1)
        // {
        //     tmpBuffer[index++] = client->read();
        // }
        // tmpBuffer[index] = 0;
        // const char *operationIdString = "{\"operationId\":\"";
        // index = indexOf(tmpBuffer, TEMP_BUFFER_SIZE, operationIdString, strlen(operationIdString), 0);
        // if (index == -1)
        // {
        // error_exit:
        //     Serial.println("ERROR: Error from DPS endpoint");
        //     Serial.println(tmpBuffer);
        //     return 1;
        // }
        // else
        // {
        //     index += strlen(operationIdString);
        //     int index2 = indexOf(tmpBuffer, TEMP_BUFFER_SIZE, "\"", 1, index + 1);
        //     if (index2 == -1)
        //         goto error_exit;
        //     tmpBuffer[index2] = 0;
        //     strcpy(operationId, tmpBuffer + index);
        //     // Serial.print("OperationId:");
        //     // Serial.println(operationId);
        //     client->stop();
        // }

        if (_client->connected()) {  
            // If connected to the server,     
            // read the status code and body of the response
            int statusCode = _client->responseStatusCode();
            DEBUGLOG("Status code: %d\n", statusCode);
            if (!(statusCode == 200 || statusCode == 202)) {
                DEBUGLOG("Wrong status code\n");
                // _client->stop();
                // return 1;
            }

            String response = _client->responseBody(); 
            DEBUGLOG("Response: %s\n", response.c_str());            
        }
        else {
            DEBUGLOG("Lost connection\n");
            _client->stop();
            // httpClient_ = NULL;
            // wifiClient_ = NULL;
            return 1;
        }

        _client->stop();  

    // }
    // else
    // {
    //     Serial.println("ERROR: Couldn't connect AzureIOT DPS endpoint.");
    //     return 1;
    // }

    return 0;
}
*/