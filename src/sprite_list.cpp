#include "mylib.h"
#include "sprite.h"
#include "sprite_list.h"
#include "list.h"
#include "myerror.h"
#include "basetype.h"
#include <new>

template<> LIST<SPRITE*>::LIST()
    : m_count(0), m_capacity(0), m_data(0)
{
}

template<> LIST<SPRITE*>::~LIST()
{
    ::operator delete(m_data);
    m_data = 0;
    m_count = 0;
}

template<> SPRITE** LIST<SPRITE*>::operator[](int index)
{
    return m_data + index;
}

template<> SPRITE* const* LIST<SPRITE*>::operator[](int index) const
{
    return m_data + index;
}

template<> SPRITE*& LIST<SPRITE*>::Item(int index) const
{
    return m_data[index];
}

template<> int LIST<SPRITE*>::No() const
{
    return m_count;
}

template<> void LIST<SPRITE*>::Expand(int requestedCapacity)
{
    if (requestedCapacity <= m_capacity)
        return;
    SPRITE** const previous = m_data;
    m_data = static_cast<SPRITE**>(::operator new(
        sizeof(SPRITE*) * static_cast<unsigned int>(requestedCapacity), std::nothrow));
    if (!m_data)
        Error->LogExit(STRING("!!!ERROR!!!::LIST: Not enough memory %i"), requestedCapacity);
    if (previous)
    {
        for (int index = 0; index < m_capacity; ++index)
            m_data[index] = previous[index];
        ::operator delete(previous);
    }
    m_capacity = requestedCapacity;
}

template<> void LIST<SPRITE*>::ExpandForInsert()
{
    if (m_count >= m_capacity)
        Expand(AddSigned32Wrap(MultiplySigned32Wrap(m_capacity, 2), 4));
}

template<> void LIST<SPRITE*>::Insert(SPRITE* value)
{
    ExpandForInsert();
    m_data[m_count] = value;
    m_count = AddSigned32Wrap(m_count, 1);
}

template<> void LIST<SPRITE*>::InsertFirst(SPRITE* value)
{
    ExpandForInsert();
    const int previousCount = m_count;
    m_count = AddSigned32Wrap(m_count, 1);
    for (int index = previousCount; index != 0; --index)
        m_data[index] = m_data[index - 1];
    m_data[0] = value;
}

template<> void LIST<SPRITE*>::InsertBefore(int index, SPRITE* value)
{
    ExpandForInsert();
    const int previousCount = m_count;
    m_count = AddSigned32Wrap(m_count, 1);
    for (int position = previousCount; position > index; --position)
        m_data[position] = m_data[position - 1];
    m_data[index] = value;
}

template<> int LIST<SPRITE*>::Location(SPRITE* const& value) const
{
    int index = m_count;
    while (index != 0)
    {
        --index;
        if (m_data[index] == value)
            return index;
    }
    return -1;
}

template<> int LIST<SPRITE*>::DeleteNumber(int index)
{
    if (index < 0 || index >= m_count)
        return 1;
    --m_count;
    m_data[index] = m_data[m_count];
    return 0;
}

template<> void LIST<SPRITE*>::Release()
{
    m_capacity = 0;
    m_count = 0;
    if (m_data)
        ::operator delete(m_data);
    m_data = 0;
}

template<> SPRITE* LIST<SPRITE*>::BeginIterateNon0(int* index) const
{
    *index = m_count;
    return NextIterateNon0(index);
}

template<> SPRITE* LIST<SPRITE*>::NextIterateNon0(int* index) const
{
    int position = --*index;
    while (position >= 0 && position < m_count && !m_data[position])
        position = --*index;
    return position >= 0 && position < m_count ? m_data[position] : 0;
}

SPRITE_LIST::SPRITE_LIST()
{
}

SPRITE_LIST::~SPRITE_LIST()
{
}

void SPRITE_LIST::Insert(SPRITE* sprite)
{
    if (!sprite)
        return;
    sprite->AddReference();
    LIST<SPRITE*>::Insert(sprite);
}

void SPRITE_LIST::InsertSorted(SPRITE* sprite)
{
    if (!sprite)
        return;
    sprite->AddReference();
    int index = 0;
    while (index < m_count)
    {
        const float current = m_data[index]->Z();
        const float key = sprite->Z();
        if (key >= current)
            break;
        ++index;
    }
    LIST<SPRITE*>::InsertBefore(index, sprite);
}

void SPRITE_LIST::InsertSorted(SPRITE* sprite, float key)
{
    if (!sprite)
        return;
    sprite->AddReference();
    int index = 0;
    while (index < m_count && !(key >= m_data[index]->Z()))
        ++index;
    LIST<SPRITE*>::InsertBefore(index, sprite);
}

int SPRITE_LIST::InsertUnique(SPRITE* sprite)
{
    if (!sprite)
        return 1;
    sprite->AddReference();
    if (Location(sprite) >= 0)
        return 1;
    LIST<SPRITE*>::Insert(sprite);
    return 0;
}

void SPRITE_LIST::InsertFirst(SPRITE* sprite)
{
    if (!sprite)
        return;
    sprite->AddReference();
    LIST<SPRITE*>::InsertFirst(sprite);
}

void SPRITE_LIST::InsertBefore(int index, SPRITE* sprite)
{
    if (!sprite)
        return;
    sprite->AddReference();
    LIST<SPRITE*>::InsertBefore(index, sprite);
}

int SPRITE_LIST::Delete(SPRITE* sprite)
{
    if (!sprite || DeleteNumber(Location(sprite)) != 0)
        return 1;
    sprite->Release();
    return 0;
}

int SPRITE_LIST::DeleteSorted(SPRITE* sprite)
{
    if (!sprite)
        return 1;
    const int index = Location(sprite);
    if (index < 0)
        return 1;
    --m_count;
    for (int position = index; position < m_count; ++position)
        m_data[position] = m_data[position + 1];
    if (!m_count)
        LIST<SPRITE*>::Release();
    sprite->Release();
    return 0;
}

int SPRITE_LIST::DeleteSpriteNumber(int index)
{
    if (index < 0 || index >= m_count)
        return 1;
    SPRITE* const sprite = m_data[index];
    DeleteNumber(index);
    if (sprite->Release())
        delete sprite;
    return 0;
}

void SPRITE_LIST::DeleteAll()
{
    for (int first = 0; first < m_count; ++first)
    {
        for (int scan = m_count - 1; scan > first; --scan)
        {
            if (m_data[first] && m_data[first] == m_data[scan])
            {
                m_data[scan]->Release();
                DeleteNumber(scan);
            }
        }
    }
    for (int index = m_count - 1; index >= 0; --index)
        if (m_data[index])
            DeleteSpriteNumber(index);
    LIST<SPRITE*>::Release();
}

void SPRITE_LIST::Release()
{
    for (int index = m_count - 1; index >= 0; --index)
        if (m_data[index] && m_data[index]->Release())
            DeleteNumber(index);
    LIST<SPRITE*>::Release();
}

int SPRITE_LIST::IsEqual(const SPRITE_LIST* other) const
{
    if (!other || other->m_count != m_count)
        return 0;
    for (int index = m_count - 1; index >= 0; --index)
        if (other->Location(m_data[index]) < 0)
            return 0;
    return 1;
}
