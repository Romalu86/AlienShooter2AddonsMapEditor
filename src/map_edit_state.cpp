#include "map_edit.h"
#include "region.h"
#include "graph.h"
#include <new>
#include <cstddef>

template<> LIST<STRING>::LIST() : m_count(0), m_capacity(0), m_data(0) {}

template<> LIST<STRING>::~LIST()
{
    delete[] m_data;
    m_data = 0;
    m_count = 0;
}

template<> void LIST<STRING>::Expand(int capacity)
{
    if (capacity <= m_capacity)
        return;
    STRING* previous = m_data;
    m_data = new(std::nothrow) STRING[capacity];
    if (!m_data)
        ::Error->LogExit(STRING("!!!ERROR!!!::LIST: Not enough memory %i"), capacity);
    if (previous)
    {
        for (int index = 0; index < m_capacity; ++index)
            m_data[index] = previous[index];
        delete[] previous;
    }
    m_capacity = capacity;
}

template<> void LIST<STRING>::ExpandForInsert()
{
    if (m_count >= m_capacity)
        Expand(m_capacity * 2 + 4);
}

template<> void LIST<STRING>::Insert(STRING value)
{
    ExpandForInsert();
    m_data[m_count++] = value;
}

void MAP_EDIT::SetRememberedVid(unsigned int type, VID* vid)
{
    int index = 0;
    while (index < 12 && (1u << index) != type)
        ++index;
    rememberedVid[index] = vid;
}

VID* MAP_EDIT::GetRememberedVid(unsigned int type) const
{
    int index = 0;
    while (index < 12 && (1u << index) != type)
        ++index;
    return rememberedVid[index];
}

void MAP_EDIT::DrawMapName()
{
    MENUITEMINFOA info = {};
    info.cbSize = sizeof(info);
    info.fMask = MIIM_TYPE;
    info.fType = MFT_RIGHTJUSTIFY;
    info.dwTypeData = const_cast<char*>(editFileName.CharPtr());
    info.cch = editFileName.Length();
    SetMenuItemInfoA(GetMenu(reinterpret_cast<HWND>(m_dialog)), 0xB01C, FALSE, &info);
    DrawMenuBar(reinterpret_cast<HWND>(m_dialog));
}

void MAP_EDIT::DeletePointerToSprite(SPRITE* sprite)
{
    if (sprite->NoRef() > 1)
        selectedSprites.Delete(sprite);
    if (static_cast<SPRITE*>(curRegion) == sprite)
        curRegion = 0;
    MAP::DeletePointerToSprite(sprite);
}

void MAP_EDIT::Release()
{
    ::Error->Log("Undo   release");
    undo.Reset();
    ::Error->Log("Select release");
    selectedSprites.Release();
    curRegion = 0;
    MAP::Release();
}
