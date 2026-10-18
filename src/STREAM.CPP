#include "stream.h"
#include "basetype.h"
#include "mylib.h"
#include <io.h>

FSTREAM::FSTREAM(const char* fileName, const char* mode)
    : file(std::fopen(fileName, mode))
{
}

FSTREAM::FSTREAM(const STRING& fileName, const STRING& mode)
    : file(FOpen(fileName, mode))
{
}

int FSTREAM::IsOpen() const
{
    return file != 0;
}

int FSTREAM::IsEnd() const
{
    return std::feof(file) ? 0x10 : 0;
}

int FSTREAM::Length() const
{
    return _filelength(_fileno(file));
}

FSTREAM::~FSTREAM()
{
    if (file)
        std::fclose(file);
}

int FSTREAM::Read(void* buffer, unsigned int count)
{
    const unsigned int transferred = static_cast<unsigned int>(
        std::fread(buffer, 1u, count, file));
    return BitsToSigned32(count-transferred);
}

int FSTREAM::Write(const void* buffer, unsigned int count)
{
    const unsigned int transferred = static_cast<unsigned int>(
        std::fwrite(buffer, 1u, count, file));
    return BitsToSigned32(count-transferred);
}

long FSTREAM::Position() const
{
    return std::ftell(file);
}
