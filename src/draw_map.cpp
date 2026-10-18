#include "draw_map.h"
#include "graph.h"
#include "input.h"
#include "map.h"
#include "player.h"
#include "railway.h"
#include "region.h"
#include "sprite_collector.h"
#include "engine_globals.h"

DRAW_MAP::DRAW_MAP(float x, float y, float width, float height)
{
    optionBits |= 1u;
    origin = VECTOR2(x, y);
    size = VECTOR2(width, height);
}

VECTOR2 DRAW_MAP::ToScreen(const VECTOR2& point) const
{
    const VECTOR2 product = point * size;
    const VECTOR2& dimensions = Map->SizeXY();
    return VECTOR2(product.x / dimensions.x, product.y / dimensions.y) + origin;
}

VECTOR2 DRAW_MAP::FromScreen(const VECTOR2& point) const
{
    const VECTOR2 product = (point - origin) * Map->SizeXY();
    return VECTOR2(product.x / size.x, product.y / size.y);
}

int DRAW_MAP::IsInside(const VECTOR2& point) const
{
    return point >= origin && point < origin + size;
}

void DRAW_MAP::DrawDot(VECTOR2 point, COLOR color) const
{
    point = ToScreen(point);
    if (IsInside(point)) Graph->PutPixel(point, color);
}

void DRAW_MAP::DrawDot2x2(VECTOR2 point, COLOR color) const
{
    point = ToScreen(point);
    if (IsInside(point)) Graph->PutBigPixel(point, color);
}

void DRAW_MAP::DrawDot4x4(VECTOR2 point, COLOR color) const
{
    point = ToScreen(point);
    if (!IsInside(point)) return;
    Graph->PutBigPixel(point - VECTOR2(2, 2), color);
    Graph->PutBigPixel(point - VECTOR2(2, 0), color);
    Graph->PutBigPixel(point - VECTOR2(0, 2), color);
    Graph->PutBigPixel(point, color);
}

int DRAW_MAP::Control(INPUT* input)
{
    if (!IsInside(input->ClientMouseXY())) return 0;
    if (input->GetAction(0x23u)) input->RemoveEvent(0x23u, 0u);
    if (!input->GetAction(0x38u)) return 0;
    Map->SetShiftCoor(FromScreen(input->ClientMouseXY()), 2);
    input->RemoveEvent(0x38u, 0u);
    return 1;
}

int DRAW_MAP::IsClickOnMap(const INPUT* input) const
{
    return input->GetAction(0x38u) && IsInside(input->ClientMouseXY());
}

void DRAW_MAP::ClickOnMap(const INPUT* input) const
{
    Map->SetShiftCoor(FromScreen(input->ClientMouseXY()), 2);
}

namespace {

VECTOR2 MinimapPosition(const SPRITE* sprite)
{
    const VECTOR& point = sprite->Coor();
    return VECTOR2(point.x, point.y - point.z);
}

bool HasFightSymbol(const SPRITE* sprite)
{
    return sprite->IsSpriteClass(0x15u) || sprite->Vid()->CanFight() ||
        (sprite->HaveLink() && sprite->Link()->Vid()->CanFight());
}
}

void DRAW_MAP::Draw()
{
    if (optionBits & 1u) Graph->Bar(origin, origin + size, COLOR(170, 0, 0, 150));

    for (int index = 0; index < RailMap.m_dots.m_count; ++index)
    {
        const R_DOT* dot = RailMap.m_dots.m_data[index];
        const int y = BitsToSigned32(static_cast<unsigned int>(dot->m_y) -
                                     static_cast<unsigned int>(dot->m_z));
        DrawDot(VECTOR2(dot->m_x, y), GRAPH::GRAY);
    }

    const VECTOR2 screenOrigin = Map->ToScreen(VECTOR2(0, 0));
    const VECTOR2 worldOrigin(-screenOrigin.x, -screenOrigin.y);
    Graph->LightBar(ToScreen(worldOrigin),
        ToScreen(worldOrigin + Graph->ViewMax() - Graph->ViewMin()), 0x00CFCFCFu);

    for (SPRITE* sprite = Sprites->FirstAllSprite(); sprite; sprite = Sprites->NextAllSprite())
    {
        const unsigned int type = sprite->Vid()->spriteType;
        const unsigned int property = sprite->Vid()->property;
        const bool hashProperty = (property & 0x40u) && !(sprite->RuntimeFlags() & 0x08000000u);
        if ((type & 4u) && (hashProperty || (property & 0x28u)))
            DrawDot(MinimapPosition(sprite), COLOR(128, 128, 128));
    }

    for (SPRITE* sprite = Sprites->FirstUnit(); sprite; sprite = Sprites->NextUnit())
    {
        const VECTOR2 point = MinimapPosition(sprite);
        PLAYER* const player = Map->Player(sprite->Army());
        if (player->RelationTo(0) == 2)
        {
            if (sprite->Vid()->spriteType & 0x10u)
                DrawDot2x2(point, COLOR(0, 255, 210));
            else if (HasFightSymbol(sprite))
                DrawDot2x2(point, GRAPH::GREEN);
            else if (sprite->IsSpriteClass(3u) || sprite->IsSpriteClass(0x18u))
                DrawDot4x4(point, GRAPH::GREEN);
        }
        else if (player->RelationTo(1) == 2)
        {
            if (sprite->Vid()->spriteType & 0x10u)
                DrawDot2x2(point, GRAPH::LIGHTRED);
            else if (HasFightSymbol(sprite))
                DrawDot2x2(point, GRAPH::RED);
            else if (sprite->IsSpriteClass(3u) || sprite->IsSpriteClass(0x18u))
                DrawDot4x4(point, GRAPH::RED);
        }
        else if (sprite->IsSpriteClass(0x15u))
            DrawDot2x2(point, GRAPH::YELLOW);
        else if (sprite->IsSpriteClass(3u) || sprite->IsSpriteClass(0x18u))
            DrawDot4x4(point, GRAPH::YELLOW);
    }
}
