#pragma once
#include <cstdio>

class STRING;

enum TYPE_ERROR {
    E_LOCK=0, E_COPY=1, E_MEMORY=2, E_CREATE=3, E_INVALID=4,
    E_LOAD=5, E_SAVE=6, E_OPEN=7, E_SET=8, E_GET=9,
    E_ERROR=10, E_SECTION=11, E_INIT=12, E_MISSING=13, E_UNKNOWN=14
};

struct HWND__;
struct _EXCEPTION_RECORD;

class MYERROR
{
public:
    explicit MYERROR(FILE* stream);
    MYERROR(const char* directory, int timestampedFile, int deleteOnExit);
    virtual ~MYERROR();

    int  Window(STRING text,...);
    void WindowExit(STRING text,...);
    long FilterExcept(_EXCEPTION_RECORD* record);
    void LogTmp(STRING text,...);
    void Log(STRING text,...);
    void Log(const char* text,...);
    void LogExit(STRING text,...);
    void Error(const STRING& objectFormat,TYPE_ERROR type,
                       const STRING& subject,unsigned long errorCode,...);

    HWND__* hwnd;
    FILE* file;
    char directory[1024];
    int deleteOnExit;
};

extern MYERROR* Error;
