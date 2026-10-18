#include "resource_ini.h"
#include "resread.h"
#include "basetype.h"
#include <cstring>
#include <new>

namespace {

int ResourceNameHash(const char* text)
{
    unsigned int hash = 0u;
    unsigned int position = 1u;
    const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text);
    while (*cursor)
    {
        const int remainder = BitsToSigned32(hash) % 999;
        hash += static_cast<unsigned int>(*cursor++) *
                (position++ + static_cast<unsigned int>(remainder));
    }
    return BitsToSigned32(hash) % 100000;
}

void CopyValue(INIVAR& destination, const INIVAR& source)
{
    std::memcpy(&destination, &source, sizeof(INIVAR));
}

void CopyValue(CONSTVAR& destination, const CONSTVAR& source)
{
    destination.text = source.text;
    std::memcpy(&destination.number, &source.number, sizeof(int));
}

}

#define DEFINE_RESOURCE_LIST(T) \
template<> NAMED_LIST_STRUCT<T>::NAMED_LIST_STRUCT() : m_name(), m_value() {} \
template<> NAMED_LIST_STRUCT<T>::~NAMED_LIST_STRUCT() {} \
template<> NAMED_LIST_STRUCT<T>& NAMED_LIST_STRUCT<T>::operator=(const NAMED_LIST_STRUCT<T>& source) \
{ \
    std::memcpy(&m_hash, &source.m_hash, sizeof(m_hash)); \
    m_name = source.m_name; \
    CopyValue(m_value, source.m_value); \
    return *this; \
} \
template<> LIST<NAMED_LIST_STRUCT<T>>::LIST() : m_count(0), m_capacity(0), m_data(0) {} \
template<> LIST<NAMED_LIST_STRUCT<T>>::~LIST() \
{ \
    delete[] m_data; \
    m_data = 0; \
    m_count = 0; \
} \
template<> void LIST<NAMED_LIST_STRUCT<T>>::Release() \
{ \
    m_count = m_capacity = 0; \
    delete[] m_data; \
    m_data = 0; \
} \
template<> int LIST<NAMED_LIST_STRUCT<T>>::No() const { return m_count; } \
template<> void LIST<NAMED_LIST_STRUCT<T>>::Expand(int capacity) \
{ \
    if (capacity <= m_capacity) return; \
    NAMED_LIST_STRUCT<T>* previous = m_data; \
    m_data = new(std::nothrow) NAMED_LIST_STRUCT<T>[capacity]; \
    if (!m_data) ::Error->LogExit(STRING("!!!ERROR!!!::LIST: Not enough memory %i"), capacity); \
    if (previous) \
    { \
        for (int index = 0; index < m_capacity; ++index) m_data[index] = previous[index]; \
        delete[] previous; \
    } \
    m_capacity = capacity; \
} \
template<> void LIST<NAMED_LIST_STRUCT<T>>::ExpandForInsert() \
{ \
    if (m_count >= m_capacity) Expand(m_capacity * 2 + 4); \
} \
template<> int NAMED_LIST<T>::GetHash(const char* name) { return ResourceNameHash(name); } \
template<> int NAMED_LIST<T>::Location(const STRING& name) const \
{ \
    const int hash = GetHash(name.CharPtr()); \
    for (int index = this->m_count; --index >= 0; ) \
        if (this->m_data[index].m_hash == hash && \
            std::strcmp(this->m_data[index].m_name.CharPtr(), name.CharPtr()) == 0) return index; \
    return -1; \
} \
template<> STRING NAMED_LIST<T>::Name(int index) const \
{ \
    return index >= 0 && index < this->m_count ? this->m_data[index].m_name : STRING(""); \
}

DEFINE_RESOURCE_LIST(INIVAR)
DEFINE_RESOURCE_LIST(CONSTVAR)
#undef DEFINE_RESOURCE_LIST

template<> void NAMED_LIST<INIVAR>::Insert(STRING name, INIVAR value)
{
    ExpandForInsert();
    NAMED_LIST_STRUCT<INIVAR>& entry = m_data[m_count];
    entry.m_name = name;
    entry.m_hash = GetHash(name.CharPtr());
    CopyValue(entry.m_value, value);
    ++m_count;
}

template<> void NAMED_LIST<CONSTVAR>::Insert(STRING name, CONSTVAR value)
{
    ExpandForInsert();
    NAMED_LIST_STRUCT<CONSTVAR>& entry = m_data[m_count];
    entry.m_name = name;
    entry.m_hash = GetHash(name.CharPtr());
    ++m_count;
    CopyValue(entry.m_value, value);
}

int RESOURCE::LoadIni(const STRING& fileName)
{
    FSTREAM input(fileName, STRING("rt"));
    STRING line, token;
    int last = -1;
    int lineNumber = 0;
    unsigned int section = 0u;
    NAMED_LIST<INIVAR> variables;
    NAMED_LIST<CONSTVAR> constants;
    if (!m_file)
    {
        ::Error->Window(STRING("Not open resouce file"));
        return 2;
    }
    if (!input.IsOpen())
    {
        ::Error->Window(STRING("Can't open file '%s'"), fileName.CharPtr());
        return 1;
    }

    unsigned int accumulator;
    while (!input.IsEnd())
    {
        line.Read(&input);
        ++lineNumber;
        line = line.Before(";");
        line.RemoveEndChars(" \n\r\t");
        line.RemoveBeginChars(" \n\r\t");
        if (line.IsEmpty()) continue;
        if (line.HaveFirst("["))
        {
            token = line.After("=");
            section = 0u;
            if (!token.IsEmpty()) std::memcpy(&section, token.CharPtr(), sizeof(section));
            last = -1;
            variables.Release();
            constants.Release();
            continue;
        }
        if (!section) continue;

        int count = line.After("[").Int();
        line = line.Before("[");
        line.RemoveEndChars(" \n\r\t");
        if (!count) count = 1;
        struct Declaration { const char* space; const char* tab; int type; };
        static const Declaration declarations[] = {
            {"CHAR ", "CHAR\t", 0}, {"STRING ", "STRING\t", 6},
            {"FLOAT ", "FLOAT\t", 7}, {"BYTE ", "BYTE\t", 1},
            {"DWORD ", "DWORD\t", 4}, {"WORD ", "WORD\t", 2},
            {"FILE ", "FILE\t", 5}
        };
        bool declared = false;
        for (const Declaration& declaration : declarations)
        {
            if (line.Replace(declaration.space, "") || line.Replace(declaration.tab, ""))
            {
                line.RemoveBeginChars(" \n\r\t");
                variables.Insert(line, INIVAR(declaration.type, count));
                declared = true;
                break;
            }
        }
        if (declared) continue;
        if (line.Replace("const ", "") || line.Replace("const\t", ""))
        {
            line.RemoveBeginChars(" \n\r\t");
            token = line.BeforeChar(" \n\r\t");
            line.Replace(token, STRING(""));
            line.RemoveBeginChars(" \n\r\t");
            constants.Insert(token, CONSTVAR(line));
            continue;
        }

        token = line.BeforeChar(" =\t");
        if (token.IsEmpty()) continue;
        const int index = variables.Location(token);
        if (index < 0)
        {
            ::Error->Window(STRING("INI:Unknown word '%s'"), token.CharPtr());
            continue;
        }
        if (index != last + 1)
        {
            ::Error->Window(STRING("INI:Enough word '%s' in %i line"),
                            variables.Name(last + 1).CharPtr(), lineNumber);
            last = index;
            continue;
        }
        last = index;
        line.Replace(token, STRING(""));
        line.RemoveBeginChars(" =\t");
        if (index == 0) PreAppend(section, 0);
        switch (variables[index].type)
        {
        case 0:
            if (line.IsEmpty())
            {
                for (int element = 0; element < variables[index].noelem; ++element)
                    Write("", 1u);
                break;
            }

        case 6:
            for (accumulator = 0u; accumulator < variables[index].noelem; ++accumulator)
            {
                STRING value;
                if (variables[index].noelem > 1)
                {
                    value = line.Before(" ");
                    line = line.After(" ");
                }
                else value = line;
                const int constant = constants.Location(value);
                if (constant >= 0) constants[constant].text.Write(this);
                else value.Write(this);
            }
            break;
        case 7:
        {
            float value = 0.0f;
            for (int element = 0; element < variables[index].noelem; ++element)
            {
                token = line.BeforeChar(" \t");
                line.Replace(token, STRING(""));
                line.RemoveBeginChars(" \t");
                if (!token.IsNumber() && token[0] != '.' && !token.IsEmpty())
                {
                    const int constant = constants.Location(token);
                    if (constant >= 0) token = constants[constant].text;
                    else ::Error->Window(STRING("Undefine constant '%s'"), token.CharPtr());
                }
                if (!token.IsEmpty() || variables.Name(index) != "Acceleration")
                    value = token.Float();
                Write(&value, sizeof(value));
                if (element < 15 && line.IsEmpty() &&
                    element != variables[index].noelem - 1)
                {
                    bool reportMissingElement = variables.Name(index) != "Acceleration";
                    if (reportMissingElement)
                        reportMissingElement = variables.Name(index) != "MaxZSpeed";
                    if (reportMissingElement)
                        reportMissingElement = variables.Name(index) != "MaxSpeed";
                    if (reportMissingElement)
                        ::Error->Window(STRING("INI:Not enough element %i in '%s[%i] in %i line'"),
                            element, variables.Name(index).CharPtr(),
                            variables[index].noelem, lineNumber);
                }
            }
            break;
        }
        case 1:
        case 2:
        case 4:
            for (int element = 0; element < variables[index].noelem; ++element)
            {

                if (variables.Name(index) == "NoFrame") accumulator = 0u;
                if (!line.IsEmpty())
                {
                    accumulator = 0u;
                    while (!line.IsEmpty())
                    {
                        token = line.BeforeChar("+ |\t");
                        if (token.IsNumber()) accumulator += static_cast<unsigned int>(token.Int());
                        else
                        {
                            const int constant = constants.Location(token);
                            if (constant >= 0)
                                accumulator += static_cast<unsigned int>(constants[constant].number);
                            else ::Error->Window(STRING("Undefine constant '%s'"), token.CharPtr());
                        }
                        line.Replace(token, STRING(""));
                        line.RemoveBeginChars(" \t");
                        if (!line.HaveFirst("+") && !line.HaveFirst("|")) break;
                        line.RemoveBeginChars("+ |\t");
                    }
                }
                Write(&accumulator, variables[index].type);
                line.RemoveBeginChars(" \t");
                if (element < 15 && line.IsEmpty() &&
                    element != variables[index].noelem - 1)
                {
                    bool reportMissingElement = variables.Name(index) != "Acceleration";
                    if (reportMissingElement)
                        reportMissingElement = variables.Name(index) != "DeathDamageMinMax";
                    if (reportMissingElement)
                        ::Error->Window(STRING("INI:Not enough element %i in '%s[%i]' in %i line"),
                            element, variables.Name(index).CharPtr(),
                            variables[index].noelem, lineNumber);
                }
            }
            break;
        case 5:
        {
            const int result = WriteFile(line);
            if (result == -1) ::Error->Window(STRING("INI:Can't open file '%s'"), line.CharPtr());
            if (result == -2) ::Error->Window(STRING("INI:Not enough memory for '%s'"), line.CharPtr());
            break;
        }
        default:
            break;
        }
        if (index == variables.No() - 1)
        {
            PostAppend();
            last = -1;
        }
    }
    return 0;
}
