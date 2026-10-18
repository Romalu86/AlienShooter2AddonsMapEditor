#include <windows.h>
#include <mmsystem.h>
#include <cstdlib>
#include "mylib.h"
#include "basetype.h"

namespace
{

int NextRandom2()
{
    static bool initialized = false;
    static int state = 0;
    if (!initialized)
    {
        initialized = true;
        state = static_cast<int>(::timeGetTime() & 0x3FFFFFFFu);
    }

    state = 48271*(state%44488)-3399*(state/44488);
    if (state < 0)
        state += 2147483647;
    return state;
}
}

int Random(int maximum)
{
    if (maximum == -1)
        return 0;
    const int divisor = static_cast<int>(static_cast<unsigned int>(maximum)+1u);
    return std::rand()%divisor;
}

float Random(float scale)
{
    return static_cast<float>(std::rand()*static_cast<double>(scale)/32767.0);
}

int Random2(int maximum)
{
    if (maximum == -1)
        return 0;
    const int divisor = static_cast<int>(static_cast<unsigned int>(maximum)+1u);
    return NextRandom2()%divisor;
}

float Random2(float scale)
{
    return static_cast<float>(NextRandom2()*static_cast<double>(scale)/2147483647.0);
}

VECTOR2 Random2XY(float minimum, float maximum)
{
    const float y = Random2(maximum-minimum)+minimum;
    const float x = Random2(maximum-minimum)+minimum;
    return VECTOR2(x, y);
}
