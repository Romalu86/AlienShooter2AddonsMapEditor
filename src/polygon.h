#pragma once
#include "basetype.h"

class COLOR;
class SPRITE_LIST;

struct POINTLIST : VECTOR2
{
    POINTLIST* next;
};

class POLYGON
{
public:
    POLYGON();
    ~POLYGON();
    int NoPoint() const;
    POINTLIST* CreatePoint(const VECTOR2& point);
    void AddLinedPoint(const VECTOR2& point);
    void CreateBox(const VECTOR2& first,const VECTOR2& second);
    void Closed();
    void Release();
    int AskInside(const VECTOR2& point) const;
    void FilterSprites(SPRITE_LIST* sprites) const;
    void Draw(COLOR color) const;

    int flags;
    POINTLIST* head;
    int noPoint;
    VECTOR2 minPoint;
    VECTOR2 maxPoint;
};
