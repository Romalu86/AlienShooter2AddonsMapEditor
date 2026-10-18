#pragma once

#include "unit.h"

class MAN : public UNIT
{
public:
    MAN(VID* vid, const VECTOR& position, ANGLE direction, SPRITE* parent=0);
    virtual ~MAN();
    virtual void DetectTact();
    virtual void MoveTact();
    virtual int Action(int actionCode, int firstParameter, int secondParameter, int thirdParameter);

private:
    int m_weaponAmmo[10];
};
