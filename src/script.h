#pragma once

#include <stddef.h>
#include "mylib.h"
#include "myerror.h"
#include "basetype.h"
#include "list.h"

class STREAM;

struct SCRIPTSTACK {
    int m_intValue;
    STRING m_stringValue;
    int m_elementCount;
    unsigned char m_flags;

    SCRIPTSTACK();
    SCRIPTSTACK(const SCRIPTSTACK& source);
    SCRIPTSTACK(const STRING& stringValue, int elementCount);
    SCRIPTSTACK(int integerValue, int elementCount);
    SCRIPTSTACK(void* object, int elementCount);
    SCRIPTSTACK(const STRING& stringValue, unsigned char flags, int elementCount);
    SCRIPTSTACK(int integerValue, unsigned char flags, int elementCount);
    ~SCRIPTSTACK();
    const SCRIPTSTACK& operator = (const SCRIPTSTACK& source);
    int Int() const;
    const STRING& String();
    void Read(STREAM* stream);
    void Write(STREAM* stream) const;
    void BinarOperator(int operation, const SCRIPTSTACK& rhs);
};

struct SCRIPTVAR {
    unsigned char m_type;
    unsigned char m_functionFlags;
    int m_valueOrOffset;
    union {
        int m_parameterStackBase;
        int m_variableStackBase;
        int m_labelOffset;
    };
    int m_elementOrParameterCount;
    int m_sourceFileIndex;
    int m_sourceLine;

    SCRIPTVAR();
};

struct SCRIPT_SOURCE {
    struct LINE {
        unsigned short m_line;
        unsigned short m_bytecodeDelta;
    };

    STRING m_fileName;
    unsigned int m_begin;
    unsigned int m_end;
    unsigned int m_lineCount;
    LINE* m_lines;

    SCRIPT_SOURCE();
    ~SCRIPT_SOURCE();
    void Release();
    void Begin(const STRING& fileName, unsigned int bytecodePosition);
    void End(unsigned int bytecodePosition);
    void AddLine(unsigned short line, unsigned int bytecodePosition);
};

template<> LIST<SCRIPTSTACK>::LIST();
template<> LIST<SCRIPTSTACK>::~LIST();
template<> LIST<NAMED_LIST_STRUCT<SCRIPTVAR> >::LIST();
template<> LIST<NAMED_LIST_STRUCT<SCRIPTVAR> >::~LIST();
template<> LIST<NAMED_LIST_STRUCT<STRING> >::LIST();
template<> LIST<NAMED_LIST_STRUCT<STRING> >::~LIST();
template<> STRING NAMED_LIST<SCRIPTVAR>::Name(int index) const;

struct SCRIPT {

    SCRIPT();
    ~SCRIPT();

    LIST<SCRIPTSTACK> m_stack;
    NAMED_LIST<SCRIPTVAR> m_functionList;
    NAMED_LIST<STRING> m_defineList;
    SCRIPTVAR* m_currentFunction;
    SCRIPT_SOURCE m_sourceFiles[256];
    STRING m_scriptFileName;
    void* m_allocation38;
    unsigned char* m_bytecode;
    int m_bytecodeCursor;
    char* m_sourceCursor;
    char* m_sourceEnd;
    void* m_sourceAllocation;
    int m_currentSourceFile;
    unsigned int m_sourceFileCount;
    int m_sourceLine;
    int m_conditionalDepth;
    int m_fallbackFunction;
    int m_parseMode;
    int m_variableMap[256];
    int m_eventMap[256];
    unsigned int m_instructionTraceAddresses[2];
    int m_errorLatch;
    int m_parserMode;

    void PushStr(const STRING& value);
    void PushStr(const STRING& value, const STRING& tag);
    void PushInt(int value);
    void PushInt(int value, const STRING& tag);
    void PushObject(void* object, const STRING& tag);
    int PopInt();
    const STRING& PopStr();
    void* PopObject();
    int IsLastStackString() const;
    void LoadVar(STREAM* stream);
    void SaveVar(STREAM* stream) const;
    int DeletePointerToObject(void* objectToClear);

    void Release();
    int LoadLGC(const STRING& fileName);
    int Load(const STRING& fileName);
    int Save();
    void Error(TYPE_ERROR type, const STRING& subject, int errorArgument);
    void RuntimeError(unsigned int bytecodePosition, const STRING& subject, int errorArgument);
    void RuntimeError(unsigned int bytecodePosition, int stackIndex, const STRING& subject, int errorArgument);
    STRING GetVariableStr(const STRING& expression);
    void SetVariableStr(const STRING& name, const STRING& source);
    int GetVariableInt(const STRING& expression, int* cachedIndex = 0);
    void SetVariableInt(const STRING& name, int integerValue);
    int GetVariableActionInt(unsigned int actionIndex);
    void SetVariableActionInt(unsigned int actionIndex, int integerValue);
    int GetNFunction(const STRING& name) const;
    int CallScriptEventFunction(int eventSlot, const char* signature, int argument1, int argument2, int argument3);
    int CallFunction(const STRING& name, const char* signature, int argument1, int argument2, int argument3);
    int CallFunction(int function, const char* signature, int argument1, int argument2, int argument3);

private:
    enum TYPE_VAR {
        TYPE_VAR_RUNTIME_LOCAL = 0,
        TYPE_VAR_STATIC_LOCAL = 1,
        TYPE_VAR_GLOBAL = 2,
        TYPE_VAR_STATIC_GLOBAL = 3,
        TYPE_VAR_PARAMETER = 4
    };

    void AddDataCommand(unsigned char opcodeByte);
    void AddDataCommand(unsigned char command, int value);
    int skipempty2();
    void skipempty();
    void GetLine(STRING* lineText);
    int GetName(STRING* name);
    int Word(char* word);
    int WordEnd(char* word);
    int GetInt();
    STRING GetString();
    int PutString(char* text);
    void IntVar(TYPE_VAR type);
    void StringVar(TYPE_VAR type);
    void mnog();
    void vyrag(int parserMode);
    void oper(int* breakPatchList);
    int func();
    void SetNoElement(int elementOrParameterCount);
    unsigned int FindSourceLine(unsigned int bytecodePosition, int* line) const;
    void SetOperation(int expressionStart, int operation);
    void slag();
    void cmpslag();
    void logicslag();
    void vyragAnd();
    void vyragXor();
    void vyragOr();
    void vyragCmpAnd();
    void vyrag_oper();
};
