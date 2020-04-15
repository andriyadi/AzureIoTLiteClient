#ifndef _WIFI_ESP_SECURE_CLIENT_H
#define _WIFI_ESP_SECURE_CLIENT_H

#if defined(RISCV)
#include "WiFiEspClient.h"

class WiFiEspSecureClient: public WiFiEspClient {
public:
    WiFiEspSecureClient(): WiFiEspClient() {

    }

    WiFiEspSecureClient(uint8_t sock): WiFiEspClient(sock) {

    }

    virtual int connect(IPAddress ip, uint16_t port) {
        // Serial.println("Connect HTTPS by IP");
        return WiFiEspClient::connectSSL(ip, port); 
    }

    virtual int connect(const char *host, uint16_t port) {
        // Serial.println("Connect HTTPS by hostname");
        return WiFiEspClient::connectSSL(host, port); 
    }
};
#endif

#endif