#include "myerror.h"
#include "mylib.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <io.h>
#include <windows.h>
#include <dbghelp.h>

MYERROR* Error = nullptr;
extern unsigned long CurrentTime;

namespace {
STRING logFileName;
STRING dumpDirectory;
bool exceptionFilterInstalled = false;

LONG WINAPI WriteCrashDump(EXCEPTION_POINTERS* exception);

const char* const errorCategories[15]={
    "0x%X Couldn't lock %s",
    "0x%X Couldn't copy %s",
    "%i There was not enough memory for %s",
    "0x%X Couldn't create the %s",
    "0x%X Invalid %s",
    "0x%X Load %s",
    "0x%X Save %s",
    "0x%X Couldn't open '%s'",
    "0x%X Couldn't set the %s",
    "0x%X Couldn't get the %s",
    "%i %s",
    "0x%X Section can't found (%s)",
    "0x%X Unable initialize %s",
    "%i Missing %s",
    "%i Unknownn %s"
};
}

MYERROR::MYERROR(FILE* stream) : hwnd(0), file(stream)
{
}

MYERROR::MYERROR(const char* logDirectory, int timestampedFile, int deleteLogOnExit)
    : hwnd(0), deleteOnExit(deleteLogOnExit)
{
    if (!exceptionFilterInstalled)
        SetUnhandledExceptionFilter(&WriteCrashDump);
    exceptionFilterInstalled = true;
    std::strcpy(directory, logDirectory);
    dumpDirectory = directory;
    {
        const STRING time = GetTimeString();
        const STRING date = GetDateString();
        logFileName = (("\\error" + date) + " " + time) + ".log";
    }
    logFileName.Replace(":", "h");
    logFileName.Replace(":", "m");
    logFileName = STRING(directory) + logFileName;
    if (!timestampedFile)
        logFileName = STRING(directory) + "\\error.log";
    file = FOpen(logFileName, STRING(timestampedFile ? "wt" : "at"));
    if (!file)
    {
        logFileName = logFileName.AfterLast("\\");
        file = FOpen(logFileName, STRING(timestampedFile ? "wt" : "at"));
    }
    char programPath[4096];
    GetModuleFileNameA(0, programPath, sizeof(programPath));
    const STRING bannerTime = GetTimeString();
    const STRING bannerDate = GetDateString();
    Log("----< %s %s >----< %s (%s %s) >----", bannerDate.CharPtr(),
        bannerTime.CharPtr(), programPath, "Feb 19 2013", "15:03:45");
}

MYERROR::~MYERROR()
{
    if (file)
        std::fclose(file);
    file = 0;
    if (deleteOnExit)
    {
        (void)_unlink(logFileName.CharPtr());
        return;
    }
    if (logFileName.After("Logs\\") != "error.log" &&
        logFileName.AfterLast("\\") != "error.log")
    {
        const STRING destination = STRING(directory) + "\\error.log";
        (void)_unlink(destination.CharPtr());
        if (!MoveFileExA(logFileName.CharPtr(), destination.CharPtr(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            (void)_unlink("error.log");
            (void)MoveFileExA(logFileName.CharPtr(), "error.log",
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        }
    }
}

namespace {
LONG WINAPI WriteCrashDump(EXCEPTION_POINTERS* exception)
{
    HMODULE debugHelp = 0;
    char modulePath[MAX_PATH];
    if (GetModuleFileNameA(0, modulePath, sizeof(modulePath)))
    {
        char* const separator = std::strrchr(modulePath, '\\');
        if (separator)
        {
            std::strcpy(separator + 1, "dbghelp.dll");
            debugHelp = LoadLibraryA(modulePath);
        }
    }
    if (!debugHelp)
        debugHelp = LoadLibraryA("dbghelp.dll");
    if (!debugHelp)
    {
        if (::Error)
            ::Error->Log("dbghelp.dll not found");
        return EXCEPTION_CONTINUE_SEARCH;
    }
    typedef BOOL (WINAPI *WriteMiniDump)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
        PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION,
        PMINIDUMP_CALLBACK_INFORMATION);
    WriteMiniDump writeDump = reinterpret_cast<WriteMiniDump>(
        GetProcAddress(debugHelp, "MiniDumpWriteDump"));
    if (!writeDump)
    {
        if (::Error)
            ::Error->Log("dbghelp.dll too old");
        return EXCEPTION_CONTINUE_SEARCH;
    }
    char executablePath[4096];
    STRING executable;
    if (GetModuleFileNameA(0, executablePath, sizeof(executablePath)))
        executable = executablePath;
    STRING dumpName;
    if (dumpDirectory.IsEmpty())
        dumpName = executable.BeforeLast(".") + "_dump.dmp";
    else
        dumpName = dumpDirectory + "\\" + executable.AfterLast("\\").BeforeLast(".") + "_dump.dmp";
    const HANDLE dumpFile = CreateFileA(dumpName.CharPtr(), GENERIC_WRITE,
        FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    LONG result = EXCEPTION_CONTINUE_SEARCH;
    if (dumpFile == INVALID_HANDLE_VALUE)
    {
        if (::Error)
            ::Error->Log("Can't create dump file '%s' (error %d)", dumpName.CharPtr(), GetLastError());
    }
    else
    {
        MINIDUMP_EXCEPTION_INFORMATION information;
        information.ThreadId = GetCurrentThreadId();
        information.ExceptionPointers = exception;
        information.ClientPointers = FALSE;
        if (writeDump(GetCurrentProcess(), GetCurrentProcessId(), dumpFile,
                      MiniDumpNormal, &information, 0, 0))
        {
            if (::Error)
                ::Error->Log("Saved dump file to '%s'", dumpName.CharPtr());
            result = EXCEPTION_EXECUTE_HANDLER;
        }
        else if (::Error)
            ::Error->Log("Failed to save dump file to '%s' (error %d)", dumpName.CharPtr(), GetLastError());
        CloseHandle(dumpFile);
    }
    if (::Error)
        ::Error->FilterExcept(exception->ExceptionRecord);
    return result;
}
}

void MYERROR::LogTmp(STRING text, ...)
{
    if (!file)
        return;
    char buffer[4096];
    va_list args;
    va_start(args, text);
    std::vsprintf(buffer, text.CharPtr(), args);
    va_end(args);
    const long position = std::ftell(file);
    std::fputs(buffer, file);
    std::fputs("\n", file);
    std::fflush(file);
    std::fseek(file, position, SEEK_SET);
}

int MYERROR::Window(STRING text, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, text);
    std::vsprintf(buffer, text.CharPtr(), args);
    va_end(args);
    if (file)
    {
        std::fputs(buffer, file);
        std::fputs("\n", file);
        std::fflush(file);
    }
    if (!hwnd)
        hwnd = GetForegroundWindow();
    return MessageBoxA(hwnd, buffer, "Error", MB_OK);
}

void MYERROR::WindowExit(STRING text, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, text);
    std::vsprintf(buffer, text.CharPtr(), args);
    va_end(args);
    Window(STRING(buffer));
    std::exit(1);
}

void MYERROR::Log(STRING text, ...)
{
    if (file == 0)
    {
        return;
    }
    char buffer[4096];
    va_list args;
    va_start(args, text);
    std::vsprintf(buffer, text.m_value, args);
    va_end(args);
    std::fputs(buffer, file);
    std::fputs("\n", file);
    std::fflush(file);
}

void MYERROR::LogExit(STRING text, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, text);
    std::vsprintf(buffer, text.m_value, args);
    va_end(args);

    Log(buffer);
    if (file != 0)
        std::fclose(file);
    file = 0;
    std::exit(1);
}

void MYERROR::Error(const STRING& objectFormat, TYPE_ERROR type,
                            const STRING& subject, unsigned long errorCode, ...)
{
    char buffer[4096];
    {
        const unsigned long currentTime = ::CurrentTime;
        STRING timeText = GetTimeString();
        std::sprintf(buffer, "!!!ERROR %s/%u!!!", timeText.m_value,
                     static_cast<unsigned int>(currentTime));
    }

    va_list args;
    va_start(args, errorCode);
    std::vsprintf(buffer+std::strlen(buffer), objectFormat.m_value, args);
    va_end(args);
    std::strcat(buffer, ": ");
    if (static_cast<unsigned int>(type) <= E_UNKNOWN)
        std::strcat(buffer, errorCategories[static_cast<unsigned int>(type)]);

    Log(buffer, errorCode, subject.m_value);
}

void MYERROR::Log(const char* format, ...)
{
    FILE* const stream = file;
    if (stream != 0)
    {
        char text[4096];
        va_list args;
        va_start(args, format);
        std::vsprintf(text, format, args);
        va_end(args);
        std::fputs(text, file);
        std::fputs("\n", file);
        std::fflush(file);
    }
}

long MYERROR::FilterExcept(_EXCEPTION_RECORD* record)
{
    switch (record->ExceptionCode)
    {
    case EXCEPTION_ACCESS_VIOLATION:
        {
            const bool writeAccess = record->ExceptionInformation[0] != 0;
            const ULONG_PTR targetAddress = record->ExceptionInformation[1];
            void* const instructionAddress = record->ExceptionAddress;
            const STRING time = GetTimeString();
            if (writeAccess)
                Log("!!!ERROR EXCEPTION %s 0x%X!!!: Access violation write to 0x%X", time.CharPtr(), instructionAddress, targetAddress);
            else
                Log("!!!ERROR EXCEPTION %s 0x%X!!!: Access violation read from 0x%X", time.CharPtr(), instructionAddress, targetAddress);
        }
        break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        Log("!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_ARRAY_BOUNDS_EXCEEDED", record->ExceptionAddress);
        break;
    case EXCEPTION_BREAKPOINT:
        Log("!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_BREAKPOINT", record->ExceptionAddress);
        break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        Log("!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_DATATYPE_MISALIGNMENT", record->ExceptionAddress);
        break;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        Log("!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_FLT_DENORMAL_OPERAND", record->ExceptionAddress);
        break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        Log("!!!ERROR EXCEPTION 0x%X!!!:FLT divide by zero", record->ExceptionAddress);
        break;
    case EXCEPTION_FLT_INEXACT_RESULT:
        Log("!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_FLT_INEXACT_RESULT", record->ExceptionAddress);
        break;
    case EXCEPTION_FLT_INVALID_OPERATION:
        Log("!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_FLT_INVALID_OPERATION", record->ExceptionAddress);
        break;
    case EXCEPTION_FLT_OVERFLOW:
        Log("!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_FLT_OVERFLOW", record->ExceptionAddress);
        break;
    case EXCEPTION_FLT_STACK_CHECK:
        Log("!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_FLT_STACK_CHECK", record->ExceptionAddress);
        break;
    case EXCEPTION_FLT_UNDERFLOW:
        Log("!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_FLT_UNDERFLOW", record->ExceptionAddress);
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        Log("!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_ILLEGAL_INSTRUCTION", record->ExceptionAddress);
        break;
    case EXCEPTION_IN_PAGE_ERROR:
        Log("!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_IN_PAGE_ERROR", record->ExceptionAddress);
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        Log("!!!ERROR EXCEPTION 0x%X!!!:INT divide by zero", record->ExceptionAddress);
        break;
    case EXCEPTION_INT_OVERFLOW:
        Log("!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_INT_OVERFLOW", record->ExceptionAddress);
        break;
    case EXCEPTION_INVALID_DISPOSITION:
        Log("!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_INVALID_DISPOSITION", record->ExceptionAddress);
        break;
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        Log("!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_NONCONTINUABLE_EXCEPTION", record->ExceptionAddress);
        break;
    case EXCEPTION_PRIV_INSTRUCTION:
        Log("!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_PRIV_INSTRUCTION", record->ExceptionAddress);
        break;
    case EXCEPTION_SINGLE_STEP:
        Log("!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_SINGLE_STEP", record->ExceptionAddress);
        break;
    case EXCEPTION_STACK_OVERFLOW:
        Log("!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_STACK_OVERFLOW", record->ExceptionAddress);
        break;
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
