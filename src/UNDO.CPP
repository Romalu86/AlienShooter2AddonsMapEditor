#include "undo.h"
#include "sprite.h"
#include "myerror.h"
#include <new>
#include <cstring>

namespace
{
// The editor has one live Undo history. This pointer does not change UNDO layout.
UNDO* activeUndo = 0;
}

UNDO::UNDO_DATA::UNDO_DATA() : sprite(0)
{
}

UNDO::UNDO_DATA::UNDO_DATA(TYPE_CREATE createType, SPRITE* object)
    : type(createType), sprite(object)
{
}

void UNDO::UNDO_DATA::Undo()
{
    // Runtime can destroy a sprite held in an older editor transaction.
    if (!sprite) return;
    if (type == INSERTED)
    {
        sprite->Action(ACT_UNDO_REMOVE, 0, 0, 0);
        type = DELETED;
    }
    else
    {
        sprite->Action(ACT_UNDO_INSERT, 0, 0, 0);
        type = INSERTED;
    }
}

void UNDO::UNDO_DATA::DeleteSprite()
{
    if (sprite && type == DELETED)
        delete sprite;
    sprite = 0;
}

template<> LIST<UNDO::UNDO_DATA>::LIST()
    : m_count(0), m_capacity(0), m_data(0)
{
}

template<> LIST<UNDO::UNDO_DATA>::~LIST()
{
    if (m_data) ::operator delete(m_data);
    m_data = 0;
    m_count = 0;
}

template<> int LIST<UNDO::UNDO_DATA>::No() const
{
    return m_count;
}

template<> UNDO::UNDO_DATA* LIST<UNDO::UNDO_DATA>::operator[](int index)
{
    return m_data + index;
}

template<> const UNDO::UNDO_DATA* LIST<UNDO::UNDO_DATA>::operator[](int index) const
{
    return m_data + index;
}

template<> void LIST<UNDO::UNDO_DATA>::Release()
{
    m_capacity = 0;
    m_count = 0;
    ::operator delete(m_data);
    m_data = 0;
}

template<> void LIST<UNDO::UNDO_DATA>::Expand(int capacity)
{
    if (capacity <= m_capacity)
        return;

    UNDO::UNDO_DATA* previous = m_data;
    m_data = static_cast<UNDO::UNDO_DATA*>(
        ::operator new(sizeof(UNDO::UNDO_DATA) * static_cast<unsigned int>(capacity), std::nothrow));
    if (m_data)
    {
        for (int i = 0; i < capacity; ++i)
            new (m_data + i) UNDO::UNDO_DATA;
    }
    else
    {
        Error->LogExit(STRING("!!!ERROR!!!::LIST: Not enough memory %i"), capacity);
    }

    if (previous)
    {
        std::memcpy(m_data, previous,
            sizeof(UNDO::UNDO_DATA) * static_cast<unsigned int>(m_capacity));
        ::operator delete(previous);
    }
    m_capacity = capacity;
}

template<> void LIST<UNDO::UNDO_DATA>::ExpandForInsert()
{
    if (m_count >= m_capacity)
        Expand(m_capacity * 2 + 4);
}

template<> void LIST<UNDO::UNDO_DATA>::Insert(UNDO::UNDO_DATA value)
{
    ExpandForInsert();
    m_data[m_count++] = value;
}

template<> int LIST<UNDO::UNDO_DATA>::DeleteNumber(int index)
{
    if (index < 0 || index >= m_count)
        return 1;
    --m_count;
    m_data[index] = m_data[m_count];
    return 0;
}

template<> void LIST<UNDO::UNDO_DATA>::GetReleased(LIST<UNDO::UNDO_DATA>* source)
{
    if (this == source)
        return;
    ::operator delete(m_data);
    m_count = source->m_count;
    m_capacity = source->m_capacity;
    m_data = source->m_data;
    source->m_count = 0;
    source->m_capacity = 0;
    source->m_data = 0;
}

UNDO::UNDO() : noUndo(0)
{
    activeUndo = this;
}

UNDO::~UNDO()
{
    if (activeUndo == this)
        activeUndo = 0;
}

void UNDO::OnSpriteDestroyed(SPRITE* sprite)
{
    if (activeUndo && sprite)
        activeUndo->ForgetDestroyedSprite(sprite);
}

void UNDO::ForgetDestroyedSprite(SPRITE* sprite)
{
    // Compare identities only. A destroyed sprite must never be dereferenced.
    // Null every occurrence: the same pointer may appear in several transactions.
    for (int history = 0; history < HistorySize; ++history)
    {
        const int count = sprites[history].No();
        for (int index = 0; index < count; ++index)
        {
            UNDO_DATA* const record = sprites[history][index];
            if (record->sprite == sprite)
                record->sprite = 0;
        }
    }
}

int UNDO::IsUndo() const
{
    return noUndo;
}

int UNDO::IsRedo() const
{
    return noUndo < HistorySize && sprites[noUndo].No() != 0;
}

void UNDO::Undo()
{
    if (!IsUndo())
        return;
    --noUndo;
    for (int i = 0; i < sprites[noUndo].No(); ++i)
        sprites[noUndo][i]->Undo();
}

void UNDO::Redo()
{
    if (!IsRedo())
        return;
    for (int i = 0; i < sprites[noUndo].No(); ++i)
        sprites[noUndo][i]->Undo();
    ++noUndo;
}

void UNDO::Begin()
{
    End();
    for (int i = noUndo; i < HistorySize; ++i)
        ReleaseUndo(i);
    if (noUndo >= HistorySize)
    {
        ReleaseUndo(0);
        for (int i = 1; i < HistorySize; ++i)
            sprites[i - 1].GetReleased(&sprites[i]);
        noUndo = HistorySize - 1;
    }
}

void UNDO::End()
{
    if (noUndo < HistorySize && sprites[noUndo].No() != 0)
        ++noUndo;
}

void UNDO::ReleaseUndo(int index)
{
    for (int i = sprites[index].No() - 1; i >= 0; --i)
        sprites[index][i]->DeleteSprite();
    sprites[index].Release();
}

void UNDO::AddInsert(SPRITE* sprite)
{
    if (sprite && !sprite->IsSpriteClass(21))
        sprites[noUndo].Insert(UNDO_DATA(UNDO_DATA::INSERTED, sprite));
}

void UNDO::AddRemove(SPRITE* sprite)
{
    if (!sprite)
        return;
    if (!sprite->IsSpriteClass(21))
    {
        sprites[noUndo].Insert(UNDO_DATA(UNDO_DATA::DELETED, sprite));
        sprite->Action(ACT_UNDO_REMOVE, 0, 0, 0);
    }
    else
    {
        delete sprite;
    }
}

void UNDO::DeleteLast()
{
    sprites[noUndo].DeleteNumber(sprites[noUndo].No() - 1);
}

void UNDO::Reset()
{
    for (int i = 0; i < HistorySize; ++i)
        ReleaseUndo(i);
    noUndo = 0;
}
