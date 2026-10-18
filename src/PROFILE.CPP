#include "profile.h"

#include <windows.h>

PROFILE::PROFILE(const STRING& path) : fileName()
{
    Load(path);
}

PROFILE::~PROFILE() = default;

int PROFILE::Load(const STRING& path)
{
    fileName = StringGlobals::EmptyText();
    fileName = path;
    return 0;
}

int PROFILE::GetInt(const STRING& section, const STRING& key, int defaultValue)
{
    return static_cast<int>(GetPrivateProfileIntA(
        section.CharPtr(), key.CharPtr(), defaultValue, fileName.CharPtr()));
}

STRING PROFILE::GetString(const STRING& section, const STRING& key,
                          const STRING& defaultValue)
{
    char buffer[32767];
    GetPrivateProfileStringA(section.CharPtr(), key.CharPtr(),
                            defaultValue.CharPtr(), buffer, sizeof(buffer),
                            fileName.CharPtr());
    return STRING(buffer);
}

STRING PROFILE::GetSection(const STRING& section, const STRING& defaultValue)
{
    char buffer[32767] = {0};
    if (GetPrivateProfileSectionA(section.CharPtr(), buffer, sizeof(buffer),
                                 fileName.CharPtr()) == 0)
        return defaultValue;

    for (char* cursor = buffer;
         (*cursor != 0 || cursor[1] != 0) && cursor < buffer + sizeof(buffer);
         ++cursor)
    {
        if (*cursor == 0)
            *cursor = '\n';
    }
    return STRING(buffer);
}
