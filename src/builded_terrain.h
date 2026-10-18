#pragma once
#include "sprite.h"

class BUILDED_TERRAIN : public SPRITE
{
public:
    BUILDED_TERRAIN(VID* vid, const VECTOR& position, ANGLE direction, SPRITE* parent=0);
    virtual ~BUILDED_TERRAIN();
};
