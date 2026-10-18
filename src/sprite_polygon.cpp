#include "sprite_collector.h"
#include "sprite.h"
#include "vid.h"
#include "map.h"
#include "polygon.h"

namespace {

int ClampSelectionCell(float coordinate,float scale,int count)
{
    const int value=TruncateDoubleToInt32(static_cast<double>(coordinate)*scale);
    return value<0 ? 0 : value>=count ? count-1 : value;
}

bool PolygonSelectionFilter(SPRITE* sprite,unsigned int type)
{
    if (sprite->RuntimeFlags()&0x800u) return false;
    if ((type&0x80000000u)!=0u && !sprite->IsCommand(0)) return false;
    if (type&0x20000000u) return sprite->IsSpriteClass(type&0xFFFu)!=0;
    return !(type&0xFFFu) || sprite->Vid()->nVid==static_cast<int>(type&0xFFFu);
}
void SelectPolygonSprite(SPRITE* sprite,unsigned int type,unsigned int categories,
                         unsigned int armies,const POLYGON* polygon,SPRITE_LIST* result,bool testArmy)
{
    if (sprite->ChildBacklink()) return;
    if (!(sprite->Vid()->spriteType&categories)) return;
    if (testArmy && !((0x1000u<<sprite->Army())&armies)) return;
    if (!PolygonSelectionFilter(sprite,type)) return;
    if (polygon->AskInside(sprite->Coor().ViewCoor())) result->InsertUnique(sprite);
}
}

void SPRITE_COLLECTOR::FindSpritesInsidePolygon(int requestedType, const POLYGON* polygon, SPRITE_LIST* result) const
{
    unsigned int type=static_cast<unsigned int>(requestedType);
    unsigned int armies=type&0xFF000u;
    if (!type) return;
    if (!armies) armies=0xFF000u;
    unsigned int categories;
    if ((type&0xFFFu)!=0u && !(type&0x20000000u)) {
        const VID* owner=Map->Vid(static_cast<int>(type&0xFFFu));
        if (owner->property&0x40u) type|=0x10000000u;
        categories=Map->Vid(static_cast<int>(type&0xFFFu))->spriteType;
    } else {
        categories=(type&0x6FF00000u)>>20;
        if (!categories) categories=0x6FFu;
    }
    if (type&0x10000000u) {
        for (int cell=gridWidth*gridHeight-1;cell>=0;--cell) {
            SPRITE_LIST& list=spatialCells[cell].sprites;
            int index=list.m_count-1;
            SPRITE* sprite=list.m_count ? list.m_data[index] : 0;
            while (sprite) {
                SelectPolygonSprite(sprite,type,categories,armies,polygon,result,true);
                if (index>list.m_count) index=list.m_count;
                --index;
                sprite=index>=0 ? list.m_data[index] : 0;
            }
        }
        return;
    }
    if ((categories&0x18u)!=0u && !(categories&0x6E7u)) {
        for (int army=0;army<8;++army) {
            if (!((0x1000u<<army)&armies)) continue;
            int iterator;
            for (SPRITE* sprite=FirstUnit(&iterator,army);sprite;sprite=NextUnit(&iterator))
                SelectPolygonSprite(sprite,type,categories,armies,polygon,result,false);
        }
        return;
    }
    if ((type&0xFFFu)!=0u && !(type&0x20000000u) &&
        (Map->Vid(static_cast<int>(type&0xFFFu))->runtimeFlags&0x80u)!=0u) {
        int iterator;
        for (SPRITE* sprite=FirstActiveSprite(&iterator);sprite;sprite=NextActiveSprite(&iterator))
            SelectPolygonSprite(sprite,type,categories,armies,polygon,result,true);
        return;
    }
    for (SPRITE* sprite=FirstAllSprite();sprite;sprite=NextAllSprite())
        SelectPolygonSprite(sprite,type,categories,armies,polygon,result,true);
}

void SPRITE_COLLECTOR::FindSpritesInsideBox(int requestedType,const VECTOR2& minPoint,
                                          const VECTOR2& maxPoint,SPRITE_LIST* result) const
{
    unsigned int type=static_cast<unsigned int>(requestedType);
    if (!type) return;
    unsigned int armies=type&0xFF000u;
    if (!armies) armies=0xFF000u;
    const unsigned int number=type&0xFFFu;
    unsigned int categories;
    if (number && !(type&0x20000000u))
    {
        const VID* vid=Map->Vid(static_cast<int>(number));
        if (vid->property&0x40u) type|=0x10000000u;
        categories=vid->spriteType;
    }
    else
    {
        categories=(type>>20)&0x6FFu;
        if (!categories) categories=0x6FFu;
    }

    const float firstX=static_cast<float>(static_cast<double>(minPoint.x)-1.0/gridScaleX);
    const float firstY=static_cast<float>(static_cast<double>(minPoint.y)-1.0/gridScaleY);
    const float lastX=static_cast<float>(static_cast<double>(maxPoint.x)+1.0/gridScaleX);
    const float lastY=static_cast<float>(static_cast<double>(maxPoint.y)+1.0/gridScaleY);
    const int minX=ClampSelectionCell(firstX,gridScaleX,gridWidth);
    const int minY=ClampSelectionCell(firstY,gridScaleY,gridHeight);
    const int maxX=ClampSelectionCell(lastX,gridScaleX,gridWidth);
    const int maxY=ClampSelectionCell(lastY,gridScaleY,gridHeight);
    for (int y=minY;y<=maxY;++y)
        for (int x=minX;x<=maxX;++x)
            for (int layer=0;layer<15;++layer)
            {
                SPRITE_LIST& sprites=spatialCells[(y<<gridShift)+x].layerSprites[layer];
                int index=sprites.No()-1;
                SPRITE* sprite=index>=0 ? *sprites[index] : 0;
                while (sprite)
                {
                    const VID* vid=sprite->Vid();
                    const unsigned int flags=sprite->RuntimeFlags();
                    bool matches=!sprite->ChildBacklink() && (vid->spriteType&categories) &&
                        ((0x1000u<<sprite->Army())&armies) && !(flags&0x800u) &&
                        (!(type&0x80000000u) || !(flags&0x7Cu));
                    if (matches)
                    {
                        if (type&0x20000000u)
                            matches=vid->spriteClass==number;
                        else
                            matches=(!number || vid->nVid==static_cast<int>(number)) &&
                                (!(type&0x10000000u) || (vid->property&0x40u));
                    }
                    if (matches)
                    {
                        const VECTOR2 point=sprite->Coor().ViewCoor();
                        if (point.x>=minPoint.x && point.y>=minPoint.y &&
                            point.x<=maxPoint.x && point.y<=maxPoint.y)
                            result->InsertUnique(sprite);
                    }
                    if (index>sprites.No()) index=sprites.No();
                    --index;
                    sprite=index>=0 ? *sprites[index] : 0;
                }
            }
}
