#pragma once
#include <cstdio>

class STREAM;
struct VECTOR2;
struct HKEY__;
struct HWND__;
struct _FILETIME;

int Random(int maximum);
float Random(float scale);
int Random2(int maximum);
float Random2(float scale);
VECTOR2 Random2XY(float minimum, float maximum);

namespace StringGlobals
{
    extern char EmptyTextCharacter;
    char* EmptySentinel();
    const char* EmptyText();
}

class STRING {
public:
    STRING() noexcept : value(EMPTY) {}
    STRING(const char* source);
    STRING(const STRING& source);
    ~STRING();

    union {
        char* m_value;
        char* value;
    };

    const STRING& operator=(const STRING& source);
    const STRING& operator=(const char* source);
    const STRING& operator+=(const STRING& suffix);
    const STRING& operator+=(const char* suffix);
    int operator==(const STRING& right) const;
    int operator==(const char* right) const;
    int operator!=(const char* right) const;
    const STRING operator+(const STRING& right) const;
    const STRING operator+(const char* right) const;
    void Read(std::FILE* file);
    void Read(STREAM* stream);
    int LoadFile(const STRING& fileName);
    int SaveFile(const STRING& fileName) const;
    int WriteToClipboard(HWND__* window) const;
    int ReadFromClipboard(HWND__* window);
    void ToWideChar(unsigned short* output,int outputChars) const;
    const STRING SplitRegPath(HKEY__** root) const;
    int Replace(const char* oldText,const char* newText);
    int Replace(const STRING& oldText,const STRING& newText);
    void RemoveEndChars(const char* chars);
    void RemoveBeginChars(const char* chars);
    int CountChar(int needle) const;
    int Length() const;
    const char* CharPtr() const;
    const STRING Before(const char* delimiter) const;
    const STRING BeforeChar(const char* delimiters) const;
    int HaveFirst(const char* prefix) const;
    const STRING After(const char* delimiter) const;
    const STRING BeforeLast(const char* delimiter) const;
    const STRING BeforeLast(const STRING& delimiter) const;
    STRING ToLower() const;
    STRING ToUpper() const;
    const STRING AfterLast(const char* delimiter) const;
    const STRING Add(int delta) const;
    const STRING SubString(unsigned int begin, unsigned int end) const;

private:
    static char EMPTY[1];
    friend char* StringGlobals::EmptySentinel();
    void Copy(const char* source,int length);
    STRING(const char* source,int length);

public:
    STRING ToBase64(int addCode) const;

int IsEmpty() const;
    int IsNumber() const;
    int Int() const;
    float Float() const;
    void Write(STREAM* stream) const;
    void Write(std::FILE* file) const;
    const STRING Before(const STRING& delimiter) const;
    const STRING After(const STRING& delimiter) const;
    char operator[](int index) const;

private:
    friend const STRING operator+(const char* left,const STRING& right);
    STRING(const char* left,const char* right);

};

char* StringEmptySentinel();
const STRING operator+(const char* left, const STRING& right);
const STRING Int2Str(int value);
const STRING Printf(const char* format, ...);
const STRING GetDateString();
const STRING GetTimeString();
std::FILE* FOpen(const STRING& fileName, const STRING& mode);
int FFileLength(const STRING& fileName);
void* FFileMapping(const STRING& fileName);
int FGetFileTime(STRING fileName, _FILETIME* creationTime, _FILETIME* accessTime, _FILETIME* writeTime);
int FSetFileTime(STRING fileName, _FILETIME* creationTime, _FILETIME* accessTime, _FILETIME* writeTime);
const STRING FCurrentDirectory();
STRING FFirstFile(int* findHandle, const STRING& fileMask, _FILETIME* lastWriteTime);
STRING FNextFile(int* findHandle, _FILETIME* lastWriteTime);

extern STRING WindowsUserPath;

extern int zmTemp;
