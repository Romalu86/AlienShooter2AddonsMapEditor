#include "polygon.h"
#include "map.h"
#include "graph.h"
#include "region.h"
#include "sprite_list.h"
#include <cstdlib>
#include <new>

POLYGON::POLYGON() : flags(0),head(0),noPoint(0),minPoint(9999,9999),maxPoint(-9999,-9999) {}
POLYGON::~POLYGON() { Release(); }
int POLYGON::NoPoint() const { return noPoint; }

POINTLIST* POLYGON::CreatePoint(const VECTOR2& position)
{
    POINTLIST* point=new(std::nothrow) POINTLIST;
    point->next=0;
    point->x=position.x;
    point->y=position.y;
    if (position.x<minPoint.x) minPoint.x=position.x;
    if (position.y<minPoint.y) minPoint.y=position.y;
    if (position.x>maxPoint.x) maxPoint.x=position.x;
    if (position.y>maxPoint.y) maxPoint.y=position.y;
    return point;
}

void POLYGON::AddLinedPoint(const VECTOR2& position)
{
    if (position.x<minPoint.x) minPoint.x=position.x;
    if (position.y<minPoint.y) minPoint.y=position.y;
    if (position.x>maxPoint.x) maxPoint.x=position.x;
    if (position.y>maxPoint.y) maxPoint.y=position.y;
    if (!head)
    {
        head=CreatePoint(position);
        head->next=head;
        noPoint=1;
        return;
    }
    if (noPoint>1)
    {
        const ANGLE previous(*head->next-*head);
        const ANGLE next(*head->next-position);
        if (std::abs(static_cast<int>(previous.value)-static_cast<int>(next.value))<2)
        {
            head->x=position.x;
            head->y=position.y;
            return;
        }
    }
    const VECTOR2 old(*head);
    head->x=position.x;
    head->y=position.y;
    POINTLIST* point=CreatePoint(old);
    point->next=head->next;
    head->next=point;
    ++noPoint;
}

int POLYGON::AskInside(const VECTOR2& position) const
{
    if (noPoint<=1) return 0;
    if ((position.x<minPoint.x && position.y<minPoint.y) ||
        (position.x>maxPoint.x && position.y>maxPoint.y)) return 0;
    int crossings=0;
    POINTLIST* point=head;
    for (int index=0;index<noPoint;++index,point=point->next)
    {
        POINTLIST* next=point->next;
        if ((position.x>point->x && position.x<next->x) ||
            (position.x<point->x && position.x>next->x))
        {
            const int deltaY=TruncateDoubleToInt32(static_cast<double>(next->y)-point->y);
            const double crossing=point->y+(static_cast<double>(position.x)-point->x)*deltaY/(static_cast<double>(next->x)-point->x);
            if (crossing>position.y) ++crossings;
        }
        if (next->x==position.x)
        {
            POINTLIST* after=next->next ? next->next : head->next;
            if (((position.x>point->x && position.x<after->x) ||
                 (position.x<point->x && position.x>after->x)) && next->y>position.y)
                ++crossings;
        }
    }
    return crossings&1;
}

void POLYGON::CreateBox(const VECTOR2& first,const VECTOR2& second)
{
    Release();
    head=CreatePoint(VECTOR2(first.x,second.y));
    head->next=CreatePoint(second);
    head->next->next=CreatePoint(VECTOR2(second.x,first.y));
    head->next->next->next=CreatePoint(first);
    head->next->next->next->next=head;
    noPoint=4;
    flags=2;
}

void POLYGON::Release()
{
    POINTLIST* point=head;
    for (int index=0;index<noPoint;++index)
    {
        POINTLIST* old=point;
        point=point->next;
        delete old;
    }
    head=0;
    flags=0;
    noPoint=0;
    minPoint=VECTOR2(999999,999999);
    maxPoint=VECTOR2(-999999,-999999);
}
void POLYGON::Closed() { if (head) AddLinedPoint(*head); }

void POLYGON::FilterSprites(SPRITE_LIST* sprites) const
{
    int index=sprites->No()-1;
    SPRITE* sprite=index>=0 ? *(*sprites)[index] : 0;
    while (sprite)
    {
        if (!AskInside(sprite->Coor().ViewCoor())) sprites->Delete(sprite);
        if (index>sprites->No()) index=sprites->No();
        --index;
        sprite=index>=0 ? *(*sprites)[index] : 0;
    }
}

void POLYGON::Draw(COLOR color) const
{
    POINTLIST* point=head;
    for (int index=0;index<noPoint;++index,point=point->next)
        Graph->Line(Map->ToScreen(*point),Map->ToScreen(*point->next),color);
}
