#pragma once

#include "mylib.h"

class PROFILE
{
public:
    PROFILE(const STRING& path);
    ~PROFILE();

    int Load(const STRING& path);
    int GetInt(const STRING& section, const STRING& key, int defaultValue);
    STRING GetString(const STRING& section, const STRING& key,
                     const STRING& defaultValue);
    STRING GetSection(const STRING& section, const STRING& defaultValue);

private:
    STRING fileName;
};
