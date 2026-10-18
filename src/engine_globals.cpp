class MAP;
class SOUND;
class PROFILE;
struct ADD_CREATE_SPRITE;

MAP* Map = nullptr;
SOUND* Sound = nullptr;

PROFILE* Strings = nullptr;
ADD_CREATE_SPRITE* AddCreateSprite = nullptr;

int NoBuildAlpha = 0;

namespace
{

void DefaultDrawLoadScreenTact(int)
{
}
}

void (*DrawLoadScreenTact)(int) = &DefaultDrawLoadScreenTact;
