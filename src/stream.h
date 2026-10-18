#include <cstdio>
#pragma once

class STRING;

class STREAM
{
public:
    virtual ~STREAM() noexcept(false) {}
    virtual int Read(void* buffer,unsigned int count)=0;
    virtual int Write(const void* buffer,unsigned int count)=0;
};

class FSTREAM : public STREAM
{
public:
    FSTREAM(const char* fileName, const char* mode);
    FSTREAM(const STRING& fileName, const STRING& mode);
    virtual ~FSTREAM();
    virtual int Read(void* buffer,unsigned int count);
    virtual int Write(const void* buffer,unsigned int count);
    long Position() const;
    int IsOpen() const;
    int IsEnd() const;
    int Length() const;

    std::FILE* file;
};
