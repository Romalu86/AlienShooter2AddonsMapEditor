#include <cstdio>

#include "myerror.h"
#include "mylib.h"
#include "registry.h"
#include "basetype.h"
#include <cstring>
#include <cstdlib>
#include <windows.h>

REGISTRY* Registry = nullptr;

const STRING STRING::SplitRegPath(HKEY__** root) const
{
    if (HaveFirst("HKEY_USERS\\"))
        *root = HKEY_USERS;
    else if (HaveFirst("HKEY_CURRENT_USER\\"))
        *root = HKEY_CURRENT_USER;
    else if (HaveFirst("HKEY_CLASSES_ROOT\\"))
        *root = HKEY_CLASSES_ROOT;
    else if (HaveFirst("HKEY_CURRENT_CONFIG\\"))
        *root = HKEY_CURRENT_CONFIG;
    else if (HaveFirst("HKEY_LOCAL_MACHINE\\"))
        *root = HKEY_LOCAL_MACHINE;
    else
    {
        *root = HKEY_LOCAL_MACHINE;
        return STRING(*this);
    }
    return After("\\");
}

REGISTRY::REGISTRY(const STRING& source)
    : path(source)
{
}

REGISTRY::~REGISTRY() = default;

STRING REGISTRY::GetString(const STRING& valueName, const STRING& defaultValue)
{
    HKEY root, opened;
    const STRING subKey = path.SplitRegPath(&root);
    if (RegOpenKeyExA(root, subKey.value, 0, KEY_QUERY_VALUE, &opened) == ERROR_SUCCESS)
    {
        DWORD type = 0u;
        DWORD size = 511u;
        static unsigned char buffer[512];
        const LONG queryResult = RegQueryValueExA(opened, valueName.value, 0, &type, buffer, &size);
        (void)RegCloseKey(opened);
        if (queryResult == ERROR_SUCCESS)
        {
            if (type == REG_SZ)
                return STRING(reinterpret_cast<const char*>(buffer));
            if (type == REG_BINARY || type == REG_DWORD)
            {
                int integerValue;
                memcpy(&integerValue, buffer, sizeof(integerValue));
                STRING result = Int2Str(integerValue);
                return result;
            }
        }
    }
    return STRING(defaultValue);
}

int REGISTRY::GetInt(const STRING& valueName, int defaultValue)
{
    int result = defaultValue;
    HKEY root, opened;
    const STRING subKey = path.SplitRegPath(&root);
    if (RegOpenKeyExA(root, subKey.value, 0, KEY_QUERY_VALUE, &opened) == ERROR_SUCCESS)
    {
        DWORD type = 0u;
        DWORD size = 511u;
        static unsigned char buffer[512];
        const LONG queryResult = RegQueryValueExA(opened, valueName.value, 0,
            &type, buffer, &size);
        if (queryResult == ERROR_SUCCESS)
        {
            if (type == REG_SZ)
                result = std::atoi(reinterpret_cast<const char*>(buffer));
            else if (type == REG_BINARY || type == REG_DWORD)
                memcpy(&result, buffer, sizeof(result));
        }
        (void)RegCloseKey(opened);
    }
    return result;
}

void REGISTRY::SetString(const STRING& valueName, const STRING& value)
{
    HKEY root, opened;
    const STRING subKey = path.SplitRegPath(&root);
    DWORD disposition;
    const LONG createResult = RegCreateKeyExA(root, subKey.value, 0, const_cast<char*>(StringGlobals::EmptyText()),
        REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, 0, &opened, &disposition);
    if (createResult == ERROR_SUCCESS)
    {
        const char* const data = value.value;
        const unsigned int byteCount = static_cast<unsigned int>(strlen(data)+1u);
        const LONG setResult = RegSetValueExA(opened, valueName.value, 0u, REG_SZ,
            reinterpret_cast<const BYTE*>(data), byteCount);
        (void)RegCloseKey(opened);
        if (setResult != ERROR_SUCCESS)
        {
            const STRING message("string");
            Error(E_SET, message, setResult);
        }
    }
    else
    {
        const STRING message("key for SetString");
        Error(E_CREATE, message, createResult);
    }
}

void REGISTRY::SetInt(const STRING& valueName, int value)
{
    HKEY root, opened;
    const STRING subKey = path.SplitRegPath(&root);
    DWORD disposition;
    const LONG createResult = RegCreateKeyExA(root, subKey.value, 0, const_cast<char*>(StringGlobals::EmptyText()),
        REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, 0, &opened, &disposition);
    if (createResult == ERROR_SUCCESS)
    {
        const LONG setResult = RegSetValueExA(opened, valueName.value, 0u, REG_DWORD,
            reinterpret_cast<const BYTE*>(&value), sizeof(value));
        (void)RegCloseKey(opened);
        if (setResult != ERROR_SUCCESS)
        {
            const STRING message("int");
            Error(E_SET, message, setResult);
        }
    }
    else
    {
        const STRING message("key for SetInt");
        Error(E_CREATE, message, createResult);
    }
}

void REGISTRY::DeleteKey(const STRING& keyName)
{
    HKEY root, opened;
    const STRING subKey = path.SplitRegPath(&root);
    const LONG openResult = RegOpenKeyExA(root, subKey.value, 0, KEY_ALL_ACCESS, &opened);
    if (openResult == ERROR_SUCCESS)
    {
        (void)RegDeleteKeyA(opened, keyName.value);
        (void)RegCloseKey(opened);
    }
    else
    {
        const STRING message("key for DeleteKey");
        Error(E_CREATE, message, openResult);
    }
}

void REGISTRY::Delete(const STRING& valueName)
{
    HKEY root, opened;
    const STRING subKey = path.SplitRegPath(&root);
    const LONG openResult = RegOpenKeyExA(root, subKey.value, 0, KEY_ALL_ACCESS, &opened);
    if (openResult == ERROR_SUCCESS)
    {
        (void)RegDeleteValueA(opened, valueName.value);
        (void)RegCloseKey(opened);
    }
    else
    {
        const STRING message("key for Delete");
        Error(E_CREATE, message, openResult);
    }
}

void REGISTRY::SaveToFile(const STRING& keyName, const STRING& fileName) const
{
    HKEY root, opened;
    const STRING subKey = path.SplitRegPath(&root);
    LONG openResult;
    {
        const STRING withSlash = subKey+"\\";
        const STRING fullKey = withSlash+keyName;
        openResult = RegOpenKeyExA(root, fullKey.value, 0, KEY_ALL_ACCESS, &opened);
    }
    if (openResult != ERROR_SUCCESS)
    {
        const STRING message = keyName+" key for SaveToFile";
        if (::Error)
        {
            const STRING category("REGISTRY");
            ::Error->Error(category, E_CREATE, message, openResult);
        }
        return;
    }

    std::FILE* file;
    {
        const STRING mode("wb");
        file = FOpen(fileName, mode);
    }
    if (file)
    {
        char nameBuffer[4096];
        unsigned char dataBuffer[4096];
        const auto enumerateValue = RegEnumValueA;
        unsigned int index = 0u;
        DWORD type;
        for (;;++index)
        {
            DWORD nameLength = 4096u;
            DWORD dataLength = 4096u;
            const LONG enumResult = enumerateValue(opened, index, nameBuffer, &nameLength,
                0, &type, dataBuffer, &dataLength);
            if (enumResult != ERROR_SUCCESS) break;
            (void)std::fwrite(&type, 4u, 1u, file);
            (void)std::fwrite(&nameLength, 4u, 1u, file);
            (void)std::fwrite(nameBuffer, nameLength, 1u, file);
            (void)std::fwrite(&dataLength, 4u, 1u, file);
            (void)std::fwrite(dataBuffer, dataLength, 1u, file);
        }
        (void)RegCloseKey(opened);
        (void)std::fclose(file);
    }
}

void REGISTRY::RestoreFromFile(const STRING& keyName, const STRING& fileName)
{
    std::FILE* file;
    {
        const STRING mode("rb");
        file = FOpen(fileName, mode);
    }
    if (!file) return;

    HKEY root, opened;
    const STRING subKey = path.SplitRegPath(&root);
    LONG createResult;
    {
        const STRING withSlash = subKey+"\\";
        const STRING fullKey = withSlash+keyName;
        DWORD disposition;
        createResult = RegCreateKeyExA(root, fullKey.value, 0, const_cast<char*>(StringGlobals::EmptyText()),
            REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, 0, &opened, &disposition);
    }
    if (createResult == ERROR_SUCCESS)
    {
        char nameBuffer[4096];
        unsigned char dataBuffer[4096];
        DWORD type;
        DWORD nameLength = 0u;
        DWORD dataLength;
        while (std::feof(file) == 0)
        {
            (void)std::fread(&type, 4u, 1u, file);
            (void)std::fread(&nameLength, 4u, 1u, file);
            (void)std::fread(nameBuffer, nameLength, 1u, file);
            nameBuffer[nameLength] = '\0';
            (void)std::fread(&dataLength, 4u, 1u, file);
            if (std::feof(file) != 0) break;
            (void)std::fread(dataBuffer, dataLength, 1u, file);
            const LONG setResult = RegSetValueExA(opened, nameBuffer, 0u, type, dataBuffer, dataLength);
            if (setResult != ERROR_SUCCESS)
            {
                const STRING message("data for RestoreFromFile");
                Error(E_SET, message, setResult);
            }
        }
        (void)RegCloseKey(opened);
    }
    else
    {
        const STRING message = keyName+" key for RestoreFromFile";
        Error(E_CREATE, message, createResult);
    }

    (void)std::fclose(file);
}

unsigned long REGISTRY::GetData(const STRING& valueName, void* data, unsigned long size)
{
    HKEY root, opened;
    const STRING subKey = path.SplitRegPath(&root);
    if (RegOpenKeyExA(root, subKey.value, 0, KEY_QUERY_VALUE, &opened) != ERROR_SUCCESS)
        return 0;
    DWORD type = REG_BINARY;
    DWORD bytes = size;
    (void)RegQueryValueExA(opened, valueName.value, 0, &type,
        static_cast<BYTE*>(data), &bytes);
    (void)RegCloseKey(opened);
    return bytes;
}

void REGISTRY::SetData(const STRING& valueName, const void* data, unsigned long size)
{
    HKEY root, opened;
    const STRING subKey = path.SplitRegPath(&root);
    DWORD disposition;
    LONG result = RegCreateKeyExA(root, subKey.value, 0,
        const_cast<char*>(StringGlobals::EmptyText()), REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS, 0, &opened, &disposition);
    if (result != ERROR_SUCCESS) {
        Error(E_CREATE, "key for SetData", result);
        return;
    }
    result = RegSetValueExA(opened, valueName.value, 0, REG_BINARY,
        static_cast<const BYTE*>(data), size);
    (void)RegCloseKey(opened);
    if (result != ERROR_SUCCESS)
        Error(E_SET, "data", result);
}

void REGISTRY::Error(TYPE_ERROR type, const STRING& subject, unsigned long code)
{
    if (::Error)
        ::Error->Error(STRING("REGISTRY"), type, subject, code);
}
