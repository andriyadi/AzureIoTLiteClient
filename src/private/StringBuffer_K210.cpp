#include <Arduino.h>

#if defined(RISCV)

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../IotcDefs.h"
#include "../Base64.h"
#include "../StringBuffer.h"

#include "hmac-sha256.h"

namespace AzureIOT
{

bool StringBuffer::hash(const char *key, unsigned key_length)
{
    assert(data != NULL);

    uint8_t outhmac[HMAC_SHA256_DIGEST_SIZE];
    hmac_sha256(outhmac, (const uint8_t *)data, length, (const uint8_t *)key, key_length);

    if (length < HMAC_SHA256_DIGEST_SIZE)
    {
        IOTC_FREE(data);
        data = (char *)IOTC_MALLOC(HMAC_SHA256_DIGEST_SIZE + 1);
    }
    memcpy(data, outhmac, HMAC_SHA256_DIGEST_SIZE);
    setLength(HMAC_SHA256_DIGEST_SIZE);

    return true;
}

} // namespace AzureIOT

#endif