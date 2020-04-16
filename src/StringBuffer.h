#ifndef AZURE_IOTC_LITE_STRING_BUFFER_H
#define AZURE_IOTC_LITE_STRING_BUFFER_H

#include <stdint.h>
#include <stddef.h>

#define STRING_BUFFER_16 16
#define STRING_BUFFER_32 32
#define STRING_BUFFER_64 64
#define STRING_BUFFER_128 128
#define STRING_BUFFER_256 256
#define STRING_BUFFER_512 512
#define STRING_BUFFER_1024 1024
#define STRING_BUFFER_4096 4096

namespace AzureIOT
{

class StringBuffer
{
    char *data;
    const char *immutable;
    unsigned int length;

public:
    StringBuffer(): data(NULL), immutable(NULL), length(0) {}

    StringBuffer(StringBuffer &buffer);

    StringBuffer(const char *str, unsigned int lengthStr, bool isCopy = true);

    StringBuffer(unsigned lengthStr);

    void initialize(const char *str, unsigned lengthStr);
    void alloc(unsigned lengthStr);
    void set(unsigned index, char c);
    void clear();
    ~StringBuffer();

    char *operator*() { return data; }
    unsigned getLength() { return length; }
    void setLength(unsigned int l);
    bool startsWith(const char *str, size_t len);
    int32_t indexOf(const char *look_for, size_t look_for_length,
                    int32_t start_index = 0);

#if defined(__MBED__) || defined(ARDUINO) || defined(RISCV)
    bool hash(const char *key, unsigned key_length);
#endif

    bool urlDecode();
    bool urlEncode();

    bool base64Decode();
    bool base64Encode();
};

} // namespace AzureIOT

#endif // AZURE_IOTC_LITE_STRING_BUFFER_H