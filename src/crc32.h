#pragma once

#include "mylib.h"

class CRC32
{
public:
    CRC32();
    CRC32(const void* data, unsigned int size);
    unsigned int Add(const void* data, unsigned int size);
    int CheckListFromFile(STRING fileName);

    unsigned int value;

private:
    void AddFile(STRING fileName);
    static unsigned int crc_table[256];
};
