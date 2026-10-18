#include <windows.h>
#include "myerror.h"
#include "mylib.h"
#include "basetype.h"

float STRING::Float() const
{
    float result;
    std::sscanf(value, "%f", &result);
    return result;
}
#include "stream.h"
#include <cstdarg>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <new>
#include <io.h>

void VECTOR2::Read(STREAM* stream)
{
    stream->Read(&x, 4u);
    stream->Read(&y, 4u);
}

void VECTOR2::Save(STREAM* stream) const
{
    stream->Write(&x, 4u);
    stream->Write(&y, 4u);
}

void VECTOR::Save(STREAM* stream) const
{
    stream->Write(&x, 4u);
    stream->Write(&y, 4u);
    stream->Write(&z, 4u);
}

void GAMMA::Save(STREAM* stream) const
{
    stream->Write(this, 8u);
}

void VECTOR::Read(STREAM* stream)
{
    stream->Read(&x, 4u);
    stream->Read(&y, 4u);
    stream->Read(&z, 4u);
}

void GAMMA::Read(STREAM* stream)
{
    stream->Read(this, 8u);
}

STRING STRING::ToUpper() const
{
    STRING converted(m_value);
    _strupr(converted.m_value);
    return STRING(converted);
}

void STRING::ToWideChar(unsigned short* output, int outputChars) const
{
    MultiByteToWideChar(CP_ACP, 0, m_value, -1,
        reinterpret_cast<wchar_t*>(output), outputChars);
}

int STRING::SaveFile(const STRING& fileName) const
{
    FSTREAM output(fileName,STRING("wb"));
    if (!output.IsOpen()) return 0;
    const int length=Length();
    return length-output.Write(value,static_cast<unsigned int>(length));
}

int STRING::WriteToClipboard(HWND__* window) const
{
    const int length=Length();
    int result=0;
    if (!OpenClipboard(window)) return 1;
    EmptyClipboard();
    if (length) {
        HGLOBAL data=GlobalAlloc(0x2042u,static_cast<unsigned int>(length)+1u);
        if (data) {
            void* target=GlobalLock(data);
            std::memcpy(target,value,static_cast<unsigned int>(length)+1u);
            GlobalUnlock(data);
            if (!SetClipboardData(CF_TEXT,data)) result=1;
        } else result=1;
    }
    CloseClipboard();
    return result;
}

int STRING::ReadFromClipboard(HWND__* window)
{
    int result=0;
    if (!OpenClipboard(window)) return 1;
    HGLOBAL data=GetClipboardData(CF_TEXT);
    if (data) {
        const char* text=static_cast<const char*>(GlobalLock(data));
        if (text) {
            if (value!=StringEmptySentinel()) ::operator delete(value);
            Copy(text,static_cast<int>(GlobalSize(data)));
        } else result=1;
        GlobalUnlock(data);
    } else result=1;
    CloseClipboard();
    return result;
}

int STRING::Int() const
{
    if (m_value[1] != 'x')
        return std::atoi(m_value);
    int result;
    if (std::sscanf(m_value, "%i", &result) != 1)
        return 0;
    return result;
}

int STRING::IsNumber() const
{
    return std::isdigit(static_cast<signed char>(m_value[0])) != 0 ||
        (m_value[0] == '-' && std::isdigit(static_cast<signed char>(m_value[1])) != 0);
}

const STRING STRING::Add(int delta) const
{
    if (!m_value[0])
        return STRING("");

    const std::uint32_t indexBits = static_cast<std::uint32_t>(std::strlen(m_value))-1u;
    std::int32_t digitEnd;
    std::memcpy(&digitEnd, &indexBits, sizeof(digitEnd));
    STRING working(*this);
    if (!delta)
        return STRING(working);

    do
    {
        --digitEnd;
    }
    while (digitEnd >= 0 && !std::isdigit(static_cast<signed char>(m_value[digitEnd])));
    if (digitEnd < 0)
        return STRING("");

    int digitBegin = digitEnd;
    do
    {
        --digitBegin;
    }
    while (digitBegin >= 0 && std::isdigit(static_cast<signed char>(m_value[digitBegin])));
    if (digitBegin < 0 || m_value[digitBegin] != '-')
        ++digitBegin;

    const int width = digitEnd-digitBegin+1;
    const std::uint32_t numberBits = static_cast<std::uint32_t>(std::atoi(m_value+digitBegin))+
        static_cast<std::uint32_t>(delta);
    std::int32_t number;
    std::memcpy(&number, &numberBits, sizeof(number));
    char numberText[128];
    std::sprintf(numberText, "%0*i", width, number);
    std::strncpy(working.m_value+digitBegin, numberText, static_cast<unsigned int>(width));
    return STRING(working);
}

static unsigned int StringCapacity(unsigned int length)
{
    return (length & ~15u) + 16u;
}

int STRING::LoadFile(const STRING& fileName)
{
    FSTREAM stream(fileName, STRING("rb"));
    if (!stream.IsOpen())
    {
        *this = StringGlobals::EmptyText();
        return 0;
    }

    const unsigned int length = static_cast<unsigned int>(stream.Length());
    if (m_value != StringEmptySentinel())
        ::operator delete(m_value);
    m_value = static_cast<char*>(::operator new(StringCapacity(length), std::nothrow));
    if (!m_value)
    {
        m_value = StringEmptySentinel();
        return 0;
    }

    (void)stream.Read(m_value, length);
    m_value[length] = 0;
    std::int32_t signedLength;
    std::memcpy(&signedLength, &length, sizeof(signedLength));
    return signedLength;
}

const STRING& STRING::operator=(const STRING& source)
{

    if(this==&source) return *this;

    const char* const sourceText=source.value;
    const unsigned int sourceLength=(unsigned int)strlen(sourceText);
    char* const oldText=value;

    if(oldText==StringEmptySentinel()) {
        if(sourceLength==0u) return *this;
        value=static_cast<char*>(::operator new(StringCapacity(sourceLength)));
    } else if(sourceLength==0u) {
        ::operator delete(oldText);
        value=StringEmptySentinel();
        return *this;
    } else {
        const unsigned int oldLength=(unsigned int)strlen(oldText);
        if(StringCapacity(oldLength)!=StringCapacity(sourceLength)) {
            ::operator delete(oldText);
            value=static_cast<char*>(::operator new(StringCapacity(sourceLength)));
        }
    }

    strncpy(value,source.value,sourceLength+1u);
    return *this;
}

const STRING& STRING::operator=(const char* source)
{
    const unsigned int sourceLength=(unsigned int)strlen(source);
    char* const oldText=value;

    if(oldText==StringEmptySentinel()) {
        if(sourceLength==0u) return *this;
        value=static_cast<char*>(::operator new(StringCapacity(sourceLength)));
    } else if(sourceLength==0u) {
        ::operator delete(oldText);
        value=StringEmptySentinel();
        return *this;
    } else {
        const unsigned int oldLength=(unsigned int)strlen(oldText);
        if(StringCapacity(oldLength)!=StringCapacity(sourceLength)) {
            ::operator delete(oldText);
            value=static_cast<char*>(::operator new(StringCapacity(sourceLength)));
        }
    }

    strncpy(value,source,sourceLength+1u);
    return *this;
}

const STRING& STRING::operator+=(const STRING& suffix)
{
    const char* sourceText=suffix.value;
    if(sourceText[0]==0) return *this;

    char* const oldText=value;
    const unsigned int oldLength=(unsigned int)strlen(oldText);
    const unsigned int sourceLength=(unsigned int)strlen(sourceText);
    const unsigned int combinedLength=oldLength+sourceLength;

    if(oldText==StringEmptySentinel() ||
       StringCapacity(combinedLength)!=StringCapacity(oldLength)) {
        char* const replacement=static_cast<char*>(::operator new(StringCapacity(combinedLength)));
        value=replacement;
        strncpy(replacement,oldText,oldLength);
        if(oldText!=StringEmptySentinel()) ::operator delete(oldText);
    }

    char* const appendPosition=value+oldLength;
    sourceText=suffix.value;
    strncpy(appendPosition,sourceText,sourceLength+1u);
    return *this;
}

const STRING& STRING::operator+=(const char* suffix)
{
    if(suffix[0]==0) return *this;

    char* const oldText=value;
    const unsigned int oldLength=(unsigned int)strlen(oldText);
    const unsigned int sourceLength=(unsigned int)strlen(suffix);
    const unsigned int combinedLength=oldLength+sourceLength;

    if(oldText==StringEmptySentinel() ||
       StringCapacity(combinedLength)!=StringCapacity(oldLength)) {
        char* const replacement=static_cast<char*>(::operator new(StringCapacity(combinedLength)));
        value=replacement;
        strncpy(replacement,oldText,oldLength);
        if(oldText!=StringEmptySentinel()) ::operator delete(oldText);
    }

    strncpy(value+oldLength,suffix,sourceLength+1u);
    return *this;
}

void STRING::Read(std::FILE* file)
{
    *this = StringGlobals::EmptyText();
    char buffer[256];
    unsigned int bufferUsed = 0u;
    for (;;)
    {
        if (bufferUsed == 255u)
        {
            buffer[255] = 0;
            bufferUsed = 0u;
            *this+=buffer;
        }

        int character = std::fgetc(file);
        if (character<0 || character == '\n') character = 0;
        if (character != '\r') buffer[bufferUsed++] = static_cast<char>(character);
        if (character <= 0) break;
    }
    *this+=buffer;
}

void STRING::Read(STREAM* stream)
{
    unsigned int characterBits = 0u;
    *this = StringGlobals::EmptyText();
    char buffer[256];
    unsigned int bufferUsed = 0u;
    for (;;)
    {
        if (bufferUsed == 255u)
        {
            buffer[255] = 0;
            bufferUsed = 0u;
            *this+=buffer;
        }

        const unsigned int readFailed = static_cast<unsigned int>(stream->Read(&characterBits, 1u));
        if (readFailed || characterBits == '\n') characterBits = 0u;
        const int character = static_cast<int>(characterBits);
        if (character != '\r') buffer[bufferUsed++] = static_cast<char>(character);
        if (character <= 0) break;
    }
    *this+=buffer;
}

int STRING::Replace(const char* oldText, const char* newText)
{
    const unsigned int oldLength = static_cast<unsigned int>(strlen(oldText));
    const unsigned int newLength = static_cast<unsigned int>(strlen(newText));
    char* const match = strstr(value, oldText);
    if (!match) return 0;

    if (static_cast<std::int32_t>(oldLength) >= static_cast<std::int32_t>(newLength))
    {
        strncpy(match, newText, newLength);
        const unsigned int suffixLength = static_cast<unsigned int>(strlen(match))-oldLength+1u;
        memmove(match+newLength, match+oldLength, suffixLength);
        return 1;
    }

    char* const oldValue = value;
    const unsigned int prefixLength = static_cast<unsigned int>(match-oldValue);
    const unsigned int currentLength = static_cast<unsigned int>(strlen(oldValue));
    const unsigned int finalLength = currentLength+newLength-oldLength;
    value = static_cast<char*>(::operator new(finalLength+1u));
    strncpy(value, oldValue, prefixLength);
    strncpy(value+prefixLength, newText, newLength);
    const unsigned int suffixLength = static_cast<unsigned int>(strlen(match))-oldLength+1u;
    strncpy(value+prefixLength+newLength, match+oldLength, suffixLength);
    if (oldValue != StringEmptySentinel()) ::operator delete(oldValue);
    return 1;
}

int STRING::Replace(const STRING& oldText, const STRING& newText)
{
    return Replace(oldText.CharPtr(), newText.CharPtr());
}

void STRING::RemoveEndChars(const char* chars)
{
    char* const first = value;
    if (first[0] == 0) return;

    char* last = first + strlen(first) - 1u;
    for (;;)
    {
        if (strchr(chars, static_cast<int>(static_cast<signed char>(*last))) == 0)
            return;
        *last = 0;
        if (last == value) return;
        --last;
    }
}

void STRING::RemoveBeginChars(const char* chars)
{
    const unsigned int removeCount = static_cast<unsigned int>(strspn(value, chars));
    if (removeCount != 0u)
    {
        char* const text = value;
        char* const begin = text+removeCount;
        const unsigned int remaining = static_cast<unsigned int>(strlen(begin))+1u;
        memmove(text, begin, remaining);
    }
}

int STRING::CountChar(int needle) const
{
    const signed char* cursor = reinterpret_cast<const signed char*>(value);
    int count = 0;
    while (*cursor != 0)
    {
        if (static_cast<int>(*cursor) == needle) ++count;
        ++cursor;
    }
    return count;
}

const STRING STRING::BeforeChar(const char* delimiters) const
{
    const int length = static_cast<int>(std::strcspn(value, delimiters));
    return STRING(value, length);
}

int STRING::HaveFirst(const char* prefix) const
{
    return std::strncmp(value, prefix, std::strlen(prefix)) == 0;
}

const STRING STRING::Before(const char* delimiter) const
{
    const char* const found = std::strstr(value, delimiter);
    if (found)
        return STRING(value, static_cast<int>(found-value));
    return STRING(*this);
}

const STRING STRING::After(const char* delimiter) const
{
    const char* const found = strstr(value, delimiter);
    const char* const begin = found
        ? found + strlen(delimiter)
        : StringGlobals::EmptyText();
    return STRING(begin);
}

const STRING STRING::BeforeLast(const char* delimiter) const
{
    const char* last = 0;
    const char* found = std::strstr(value, delimiter);
    while (found)
    {
        last = found;
        found = std::strstr(found+1, delimiter);
    }
    if (last)
        return STRING(value, static_cast<int>(last-value));
    return STRING(*this);
}

const STRING STRING::AfterLast(const char* delimiter) const
{
    const char* last = 0;
    const char* found = strstr(value, delimiter);
    while (found)
    {
        last = found;
        found = strstr(found+1, delimiter);
    }
    const char* const begin = last
        ? last + strlen(delimiter)
        : StringGlobals::EmptyText();
    return STRING(begin);
}

char StringGlobals::EmptyTextCharacter = '\0';

char STRING::EMPTY[1] = {0};

char* StringGlobals::EmptySentinel()
{
    return STRING::EMPTY;
}

char* StringEmptySentinel()
{
    return StringGlobals::EmptySentinel();
}

STRING::STRING(const char* source)
{
    if (source && source[0])
        Copy(source, static_cast<int>(std::strlen(source)));
    else
        value = StringEmptySentinel();
}

STRING::STRING(const STRING& source)
{
    const char* const sourceText = source.value;
    if (sourceText[0])
        Copy(sourceText, static_cast<int>(std::strlen(sourceText)));
    else
        value = StringEmptySentinel();
}

STRING::STRING(const char* source, int length)
{
    if (length)
        Copy(source, length);
    else
        value = StringEmptySentinel();
}

STRING::STRING(const char* left, const char* right)
{
    const unsigned int leftLength = static_cast<unsigned int>(strlen(left));
    const unsigned int rightLength = static_cast<unsigned int>(strlen(right));
    value = static_cast<char*>(::operator new(StringCapacity(leftLength+rightLength)));
    memcpy(value, left, leftLength);
    memcpy(value+leftLength, right, rightLength+1u);
}

STRING::~STRING()
{
    char* const text = value;
    if (text != StringEmptySentinel())
        ::operator delete(text);
}

void STRING::Copy(const char* source, int length)
{
    value = static_cast<char*>(::operator new(StringCapacity(static_cast<unsigned int>(length))));
    memcpy(value, source, static_cast<unsigned int>(length));
    value[length] = 0;
}

const STRING STRING::SubString(unsigned int begin, unsigned int end) const
{
    const unsigned int length = end-begin+1u;
    return STRING(value+begin, static_cast<int>(length));
}

const char* StringGlobals::EmptyText() { return &EmptyTextCharacter; }

int STRING::Length() const { return static_cast<int>(std::strlen(value)); }
const char* STRING::CharPtr() const { return value; }
int STRING::IsEmpty() const { return value[0] == 0; }
int STRING::operator==(const STRING& right) const { return std::strcmp(value, right.value) == 0; }
int STRING::operator==(const char* right) const { return std::strcmp(value, right) == 0; }
int STRING::operator!=(const char* right) const { return std::strcmp(value, right) != 0; }

char STRING::operator[](int index) const { return value[index]; }
const STRING STRING::operator+(const STRING& right) const { return STRING(value, right.value); }
const STRING STRING::operator+(const char* right) const { return STRING(value, right); }
const STRING operator+(const char* left, const STRING& right) { return STRING(left, right.value); }
const STRING STRING::Before(const STRING& delimiter) const { return Before(delimiter.value); }
const STRING STRING::After(const STRING& delimiter) const { return After(delimiter.value); }
const STRING STRING::BeforeLast(const STRING& delimiter) const { return BeforeLast(delimiter.value); }

std::FILE* FOpen(const STRING& fileName, const STRING& mode)
{
    return fileName.value[0] ? std::fopen(fileName.value, mode.value) : nullptr;
}
int FFileLength(const STRING& fileName)
{
    FSTREAM file(fileName, STRING("rb"));
    if (!file.IsOpen()) return 0;
    return file.Length();
}
const STRING Printf(const char* format, ...)
{
    char buffer[4096] = {};
    va_list args;
    va_start(args, format);
    std::vsprintf(buffer, format, args);
    va_end(args);
    return STRING(buffer);
}
const STRING Int2Str(int value)
{
    char buffer[128];
    return STRING(_itoa(value, buffer, 10));
}
const STRING GetDateString()
{
    char buffer[256];
    __time32_t now;
    (void)_time32(&now);
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", _localtime32(&now));
    return STRING(buffer);
}
const STRING GetTimeString()
{
    char buffer[256];
    __time32_t now;
    (void)_time32(&now);
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", _localtime32(&now));
    return STRING(buffer);
}

void* FFileMapping(const STRING& fileName)
{
    const HANDLE file = ::CreateFileA(fileName.value, GENERIC_READ,
        FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (!file)
    {
        if (::Error)
        {
            const unsigned int errorCode = ::GetLastError();
            ::Error->Log("!!!ERROR!!! FFileMapping(%s):Can't CreateFile %u",
                fileName.value, errorCode);
        }
        return 0;
    }

    const HANDLE mapping = ::CreateFileMappingA(file, 0, PAGE_READONLY, 0, 0, 0);
    ::CloseHandle(file);
    if (!mapping)
    {
        if (::Error)
        {
            const unsigned int errorCode = ::GetLastError();
            ::Error->Log("!!!ERROR!!! FFileMapping(%s):Can't CreateFileMapping %u",
                fileName.value, errorCode);
        }
        return 0;
    }

    void* const view = ::MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    ::CloseHandle(mapping);
    if (!view && ::Error)
    {
        const unsigned int errorCode = ::GetLastError();
        ::Error->Log("!!!ERROR!!! FFileMapping(%s):Can't MapViewOfFile %u",
            fileName.value, errorCode);
    }
    return view;
}

int FGetFileTime(STRING fileName, _FILETIME* creationTime,
    _FILETIME* accessTime, _FILETIME* writeTime)
{
    const HANDLE file = ::CreateFileA(fileName.value, GENERIC_READ,
        FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (file == INVALID_HANDLE_VALUE)
        return 1;
    ::GetFileTime(file, creationTime, accessTime, writeTime);
    ::CloseHandle(file);
    return 0;
}

int FSetFileTime(STRING fileName, _FILETIME* creationTime,
    _FILETIME* accessTime, _FILETIME* writeTime)
{
    const HANDLE file = ::CreateFileA(fileName.value, GENERIC_WRITE,
        FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (file == INVALID_HANDLE_VALUE)
        return 1;
    ::SetFileTime(file, creationTime, accessTime, writeTime);
    ::CloseHandle(file);
    return 0;
}

const STRING FCurrentDirectory()
{
    char path[4096];
    return ::GetCurrentDirectoryA(sizeof(path), path) ? STRING(path) : STRING("");
}

void STRING::Write(STREAM* stream) const
{
    (void)stream->Write(value, static_cast<unsigned int>(std::strlen(value))+1u);
}

void STRING::Write(std::FILE* file) const
{
    (void)std::fwrite(value, std::strlen(value)+1u, 1u, file);
}

STRING STRING::ToBase64(int addCode) const
{
    const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const unsigned int length = static_cast<unsigned int>(Length());
    STRING result;
    unsigned int outputLength = 0;
    for (unsigned int offset = 0; offset < length; offset += 57u)
    {
        const unsigned int count = offset+57u < length ? 57u : length-offset;
        unsigned char input[57];
        std::memcpy(input, value+offset, count);
        std::memset(input+count, 0, 57u-count);
        for (unsigned int i = 0; i < count; ++i)
            input[i] = static_cast<unsigned char>(input[i]+static_cast<unsigned char>(addCode));

        char encoded[77];
        unsigned int used = 0;
        for (unsigned int i = 0; i < count; i += 3u)
        {
            unsigned int group = (static_cast<unsigned int>(input[i])<<16) |
                                 (static_cast<unsigned int>(input[i+1])<<8) | input[i+2];
            for (unsigned int j = 0; j < 4u; ++j)
            {
                encoded[used+3u-j] = alphabet[group&63u];
                group >>= 6;
            }
            used += 4u;
        }
        outputLength += used;
        encoded[used] = 0;
        result += encoded;
    }
    for (unsigned int padding = (3u-length%3u)%3u; padding != 0u; --padding)
        result.value[outputLength-padding] = '=';
    return STRING(result);
}

STRING FFirstFile(int* findHandle, const STRING& fileMask, _FILETIME* lastWriteTime)
{
    WIN32_FIND_DATAA data;
    HANDLE handle = FindFirstFileA(fileMask.CharPtr(), &data);
    *findHandle = BitsToSigned32(PtrToU32(handle));
    if (handle == INVALID_HANDLE_VALUE) return STRING(StringGlobals::EmptyText());
    if (lastWriteTime) *lastWriteTime = data.ftLastWriteTime;
    return STRING(data.cFileName);
}

STRING FNextFile(int* findHandle, _FILETIME* lastWriteTime)
{
    WIN32_FIND_DATAA data;
    HANDLE handle = U32ToPtr(static_cast<unsigned int>(*findHandle));
    if (!FindNextFileA(handle, &data))
    {
        FindClose(handle);
        return STRING(StringGlobals::EmptyText());
    }
    if (lastWriteTime) *lastWriteTime = data.ftLastWriteTime;
    return STRING(data.cFileName);
}

STRING WindowsUserPath;

int zmTemp = 0;
