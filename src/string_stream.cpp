#include "string_stream.h"
#include "basetype.h"
#include <cstdio>

STRING_STREAM::STRING_STREAM() : position(0) {}
STRING_STREAM::~STRING_STREAM() = default;

void STRING_STREAM::Clear()
{
    string = "";
    position = 0;
}

void STRING_STREAM::ResetPosition()
{
    position = 0;
}

int STRING_STREAM::Read(void* buffer, unsigned int count)
{
    unsigned char* output = static_cast<unsigned char*>(buffer);
    const unsigned int length = static_cast<unsigned int>(string.Length());
    unsigned int read = 0;
    for (; read < count && position < length; ++read, ++output, position += 3u)
    {
        unsigned int byte;
        std::sscanf(string.CharPtr() + position, "%x", &byte);
        *output = static_cast<unsigned char>(byte);
    }
    return BitsToSigned32(count - read);
}

int STRING_STREAM::Write(const void* buffer, unsigned int count)
{
    const unsigned char* input = static_cast<const unsigned char*>(buffer);
    for (unsigned int i = 0; i < count; ++i)
        string += Printf("%02X ", static_cast<unsigned int>(input[i]));
    return 0;
}
