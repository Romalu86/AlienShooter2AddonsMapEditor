#pragma once
#include "stream.h"
#include "mylib.h"

class STRING_STREAM : public STREAM
{
public:
    STRING_STREAM();
    ~STRING_STREAM() override;
    int Read(void* buffer, unsigned int count) override;
    int Write(const void* buffer, unsigned int count) override;
    void Clear();
    void ResetPosition();
    STRING string;
    unsigned int position;
};
