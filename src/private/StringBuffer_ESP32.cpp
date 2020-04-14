//
// Created by Andri Yadi on 14/04/20.
//

#if defined(ESP32)
#include "../StringBuffer.h"
#include "assert.h"
#include "mbedtls/asn1.h"
#include "mbedtls/bignum.h"
#include "mbedtls/pk.h"

#include "../IotcDefs.h"

#define SHA256_DIGEST_SIZE 32

namespace AzureIOT
{
    bool StringBuffer::hash(const char *key, unsigned key_length)
    {
        assert(data != NULL);

        uint8_t outhmac[SHA256_DIGEST_SIZE];

//	int ret = mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
//	                       (const unsigned char *) key, key_length, sign);
        int ret = mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                                  (const unsigned char *) key, key_length,
                                  (const unsigned char *) data, length, outhmac);
        if (ret != 0) {
            ESP_LOGE("HASH", "mbedtls_md failed: 0x%x", ret);
            return false;
        }

        if (length < SHA256_DIGEST_SIZE)
        {
            IOTC_FREE(data);
            data = (char *)IOTC_MALLOC(SHA256_DIGEST_SIZE + 1);
        }
        memcpy(data, outhmac, SHA256_DIGEST_SIZE);
        setLength(SHA256_DIGEST_SIZE);

        return true;
    }

} // namespace AzureIOT
#endif