#pragma once
#include "mylib.h"
#include "list.h"

struct INIVAR
{
    unsigned short type;
    unsigned short noelem;
    INIVAR() {}
    INIVAR(int typeValue, int count)
        : type(static_cast<unsigned short>(typeValue)),
          noelem(static_cast<unsigned short>(count)) {}
};

struct CONSTVAR
{
    STRING text;
    int number;
    CONSTVAR() : text() {}
    explicit CONSTVAR(const STRING& value) : text(), number(value.Int())
    {
        text = value;
    }
};
