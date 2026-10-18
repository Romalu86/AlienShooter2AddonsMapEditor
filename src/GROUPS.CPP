#include "mylib.h"

#include "basetype.h"
#include "map.h"
#include "sprite_collector.h"
#include "sprite.h"
#include "stream.h"
#include "resread.h"
#include "graph.h"
#include "region.h"
#include "groups.h"
#include <new>

GROUP::GROUP(GROUP* owner, SPRITE* sprite)
{
    if (owner)
    {
        m_next = owner->m_next;
        owner->m_next = this;
    }
    else
    {
        m_next = this;
    }
    if (sprite)
        Insert(sprite);
}

GROUP::~GROUP()
{
    GROUP* const next = m_next;
    GROUP* previous = next;
    while (previous->m_next != this)
        previous = previous->m_next;
    previous->m_next = next;
}

void GROUP::Draw() const
{
    const VECTOR2 center(m_centerX, m_centerY);
    for (int index = 0; index < m_count; ++index)
        Graph->Line(Map->ToScreen(center), m_data[index]->ScreenXY(), GRAPH::WHITE);
}

void GROUP::DrawNumber(int number) const
{
    for (int i = 0;i<m_count;++i)
        Graph->PrintfXY(m_data[i]->ScreenXY(), "%i", number);
}

void GROUP::Insert(SPRITE* value)
{
    if (!value)
        return;
    if (m_count != 0)
    {
        const float scale = 0.5f;
        m_centerX = (m_centerX+value->X())*scale;
        m_centerY = (m_centerY+value->Y())*scale;
    }
    else
    {
        m_centerX = value->X();
        m_centerY = value->Y();
    }
    SPRITE_LIST::Insert(value);
}

GROUPS::GROUPS() : GROUP(0, 0)
{
}

GROUPS::~GROUPS()
{
}

void GROUPS::Save(RESOURCE* resource) const
{
    const int endOfGroup = -1;
    for (const GROUP* node = m_next; node && node != this; node = node->m_next)
    {
        for (int i = 0; i<node->m_count; ++i)
            (void)resource->Write(&node->Item(i), 4u);
        if (node->m_count)
            (void)resource->Write(&endOfGroup, sizeof(endOfGroup));
    }
    (void)resource->Write(&endOfGroup, sizeof(endOfGroup));
}

void GROUPS::Load(RESOURCE* resource)
{
    for (;;)
    {
        SPRITE* const first = Map->ReadPointer(resource);
        if (PtrToU32(first) == static_cast<unsigned int>(-1))
            break;
        GROUP* const group = new (std::nothrow) GROUP(this, first);
        for (;;)
        {
            SPRITE* const next = Map->ReadPointer(resource);
            if (PtrToU32(next) == static_cast<unsigned int>(-1))
                break;
            group->Insert(next);
        }
    }
}

void GROUPS::DeletePointerToSprite(SPRITE* sprite)
{
    GROUP* node = m_next;
    while (node && node != this)
    {
        if (node->Delete(sprite) == 0 && node->m_count == 0)
        {
            GROUP* const victim = node;
            node = node->m_next;
            if (node == this)
                node = 0;
            delete victim;
        }
        else
        {
            node = node->m_next;
            if (node == this)
                break;
        }
    }
}

void GROUPS::DrawNumber() const
{
    unsigned int index = 0;
    for (const GROUP* node = m_next; node && node != this; node = node->m_next)
        node->DrawNumber(static_cast<int>(index++));
}

float GROUP::DistanceTo(const SPRITE* sprite) const
{
    const VECTOR2 position(sprite->X(), sprite->Y());
    return (position - VECTOR2(m_centerX, m_centerY)).NearLength();
}

GROUP* GROUPS::Next(const GROUP* group)
{
    return group && group->m_next != this ? group->m_next : 0;
}

void GROUPS::InsertToNearGroup(SPRITE* sprite)
{
    GROUP* nearest = Next(this);
    for (GROUP* group = nearest; group; group = Next(group))
        if (nearest->DistanceTo(sprite) > group->DistanceTo(sprite))
            nearest = group;
    if (!nearest || nearest->DistanceTo(sprite) > 300.0f)
        new (std::nothrow) GROUP(this, sprite);
    else
        nearest->Insert(sprite);
}

void GROUPS::ShiftFirstLeft()
{
    GROUP* const first = Next(this);
    if (!first || first->m_next == this)
        return;
    GROUP* last = first;
    while (last->m_next != this)
        last = last->m_next;
    GROUP* previous = last->m_next;
    while (previous->m_next != last)
        previous = previous->m_next;
    previous->m_next = this;
    last->m_next = m_next;
    m_next = last;
}

void GROUPS::ShiftFirstRight()
{
    GROUP* const first = Next(this);
    if (!first || first->m_next == this)
        return;
    GROUP* last = first;
    while (last->m_next != this)
        last = last->m_next;
    last->m_next = first;
    m_next = first->m_next;
    first->m_next = this;
}

GROUP* GROUPS::First()
{
    return m_next == this ? 0 : m_next;
}

const GROUP* GROUPS::First() const
{
    return m_next == this ? 0 : m_next;
}
