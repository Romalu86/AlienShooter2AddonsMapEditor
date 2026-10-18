#include "map_edit.h"
#include "engine_globals.h"
#include "sprite_collector.h"
#include <algorithm>
#include <cmath>

void MAP_EDIT::FillBox(VECTOR2 begin, VECTOR2 end)
{
    if (spriteType >= 64) return;
    if (begin.x > end.x) std::swap(begin.x, end.x);
    if (begin.y > end.y) std::swap(begin.y, end.y);
    if (options.optDelete)
    {
        for (float x = begin.x; x < end.x; x += Mouse->Vid()->sizeXYZ.x)
            for (float y = begin.y; y < end.y; y += Mouse->Vid()->sizeXYZ.y)
                DeleteUnit(VECTOR2(x, y), spriteType);
        return;
    }
    if (options.optChessSnap)
    {
        int row = 0;
        for (float y = static_cast<float>(begin.y+static_cast<double>(Mouse->Vid()->sizeXYZ.y)/2.0);
             y < end.y; ++row, y += Mouse->Vid()->sizeXYZ.y)
        {
            const float offset = (row&1) ? 0.0f : Mouse->Vid()->sizeXYZ.x/2.0f;
            for (float x = begin.x+offset; x < end.x; x += Mouse->Vid()->sizeXYZ.x)
            {
                if (options.optGround0)
                    InsertUnit(Mouse->Vid(), VECTOR(x, y,
                        GetGroundZ(Mouse->Vid(), VECTOR2(x, y), Mouse->Direction())+insertZ),
                        Mouse->Direction());
                else
                    InsertUnit(Mouse->Vid(), VECTOR(x, y, insertZ), Mouse->Direction());
            }
        }
    }
    else
    {
        for (float y = static_cast<float>(begin.y+static_cast<double>(Mouse->Vid()->sizeXYZ.y)/2.0);
             y < end.y; y += Mouse->Vid()->sizeXYZ.y)
        {
            for (float x = static_cast<float>(begin.x+static_cast<double>(Mouse->Vid()->sizeXYZ.x)/2.0);
                 x < end.x; x += Mouse->Vid()->sizeXYZ.x)
            {
                if (options.optGround0)
                    InsertUnit(Mouse->Vid(), VECTOR(x, y,
                        GetGroundZ(Mouse->Vid(), VECTOR2(x, y), Mouse->Direction())+insertZ),
                        Mouse->Direction());
                else
                    InsertUnit(Mouse->Vid(), VECTOR(x, y, insertZ), Mouse->Direction());
            }
        }
    }
}

void MAP_EDIT::FillRomb(const VECTOR2& begin,const VECTOR2& end)
{
    if (spriteType>=64) return;
    float stepX=options.optChessSnap ? Mouse->Vid()->sizeXYZ.x/2.0f : Mouse->Vid()->sizeXYZ.x;
    float stepY=Mouse->Vid()->sizeXYZ.y;
    const float across=static_cast<float>(static_cast<double>(std::fabs(end.x-begin.x))/stepX);
    const int rows=TruncateDoubleToInt32(
        (static_cast<double>(std::fabs(end.y-begin.y))/stepY+across)/2.0+1.0);
    const float down=static_cast<float>(static_cast<double>(std::fabs(end.y-begin.y))/stepY);
    const float difference=static_cast<float>(down-static_cast<double>(std::fabs(end.x-begin.x))/stepX);
    const int columns=TruncateDoubleToInt32(static_cast<double>(std::fabs(difference))/2.0+1.0);
    if (begin.x>end.x) stepX=-stepX;
    if (begin.y>end.y) stepY=-stepY;
    for (int row=0;row<rows;++row)
    {
        for (int column=0;column<columns;++column)
        {
            const int acrossIndex=SubSigned32Wrap(row,column);
            const int downIndex=AddSigned32Wrap(row,column);
            if (options.optDelete)
                DeleteUnit(VECTOR2(
                    static_cast<float>(begin.x+static_cast<double>(acrossIndex)*stepX),
                    static_cast<float>(begin.y+static_cast<double>(downIndex)*stepY)),spriteType);
            else if (options.optGround0)
                InsertUnit(Mouse->Vid(),VECTOR(
                    static_cast<float>(begin.x+static_cast<double>(acrossIndex)*stepX),
                    static_cast<float>(begin.y+static_cast<double>(downIndex)*stepY),
                    GetGroundZ(Mouse->Vid(),VECTOR2(
                        static_cast<float>(begin.x+static_cast<double>(acrossIndex)*stepX),
                        static_cast<float>(begin.y+static_cast<double>(downIndex)*stepY)),
                        Mouse->Direction())+insertZ),Mouse->Direction());
            else
                InsertUnit(Mouse->Vid(),VECTOR(
                    static_cast<float>(begin.x+static_cast<double>(acrossIndex)*stepX),
                    static_cast<float>(begin.y+static_cast<double>(downIndex)*stepY),insertZ),Mouse->Direction());
        }
    }
}

SPRITE* MAP_EDIT::InsertUnit(VID* vid,VECTOR position,ANGLE direction)
{
    if (!vid) return 0;
    int drawingIndex;
    if (spriteType<64 && options.optAirBrush) {
        const float maximum=static_cast<float>(static_cast<double>(optAirBrushSize)/2.0);
        const float minimum=static_cast<float>(static_cast<double>(
            BitsToSigned32(0u-static_cast<unsigned int>(optAirBrushSize)))/2.0);
        const float offsetY=::Random(maximum-minimum)+minimum;
        const float offsetX=::Random(maximum-minimum)+minimum;
        position.x+=offsetX; position.y+=offsetY;
        if (!InMap(position.XY())) return 0;
        if (options.optGround0) position.z=GetGroundZ(vid,position.XY(),direction)+insertZ;
        for (SPRITE* sprite=Sprites->FirstDrawingSprite(&drawingIndex,vid->layer);sprite;
             sprite=Sprites->NextDrawingSprite(&drawingIndex,vid->layer)) {
            if (sprite->Vid()==vid && sprite->IsXYCross(vid,position,direction)
                && sprite->Z()+sprite->Vid()->sizeXYZ.z>=position.z
                && position.z+vid->sizeXYZ.z>=sprite->Z()) return 0;
        }
    }
    if (vid->spriteClass==2u && Sprites->CanPlace(vid,position,direction)) return 0;
    for (SPRITE* sprite=Sprites->FirstAllSprite();sprite;sprite=Sprites->NextAllSprite()) {
        const VECTOR& existing=sprite->Coor();
        if (!(existing == position)) continue;
        VID* const existingVid=sprite->Vid();
        if (existingVid==vid && (sprite->Direction().value==direction.value || (existingVid->spriteType&2u))) return 0;

        if (!(existingVid->type&0x3Fu) && vid->sizeXYZ.XY()<=existingVid->sizeXYZ.XY()) {
            return 0;
        }
    }
    SPRITE* sprite;
    if ((vid->spriteType&0x20u) || vid->spriteClass==10u)
        sprite=CreateSprite(vid,VECTOR(ToScreenX(position.x),ToScreenY(position.y),position.z),direction,0);
    else sprite=CreateSprite(vid,position,direction,0);
    if (vid->type&0x4000u) sprite->Action(95,static_cast<int>(sprite->Behave()|8u),0,0);
    undo.AddInsert(sprite);
    if (options.optRandomDir) Mouse->ChangeDirection(ANGLE(static_cast<unsigned char>(::Random(255))));
    return sprite;
}

void MAP_EDIT::DeleteUnit(const VECTOR2& position,int type)
{
    const int selector=options.optAirBrush ? Mouse->Vid()->nVid : static_cast<int>(static_cast<unsigned int>(type)<<20);
    const float radius=options.optAirBrush ? static_cast<float>(optAirBrushSize) : 100.0f;
    undo.AddRemove(Sprites->FindNearestSprite(selector,position,radius,0));
}
