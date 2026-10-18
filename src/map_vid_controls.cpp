#include "map.h"
#include "dialog_controls.h"
#include "vid.h"
#include "engine_globals.h"
#include <cstdlib>

int __cdecl SortCallBack1(const int* first, const int* second)
{
    if (Map->Vid(*first) != ::EmptyVid)
    {
        if (Map->Vid(*second) != ::EmptyVid)
        {
            const char* right = Map->Vid(*second)->name.CharPtr();
            return lstrcmpiA(Map->Vid(*first)->name.CharPtr(), right);
        }
        return -1;
    }
    return Map->Vid(*second) != ::EmptyVid ? 1 : 0;
}

int MAP::VidToControlBox(DIALOG_COMBO_BOX* box, unsigned long type, int selected) const
{
    box->Reset();
    for (int index = 0; index < m_vidCount; ++index)
    {
        VID* definition = m_vids[index];
        if (!definition || !(definition->spriteType & type)) continue;
        int item;
        {
            const STRING label = definition->GetNumberName();
            item = box->AddStringWithData(&label, index);
        }
        if (m_vids[index]->nVid == selected) box->SetCurrent(item);
    }
    return box->NoString();
}

int MAP::VidToListBox(DIALOG_LIST_BOX* box, unsigned long type, int selected, int sort) const
{
    int indices[VidSlotCount];
    for (int index = 0; index < m_vidCount; ++index) indices[index] = index;
    if (sort) std::qsort(indices, m_vidCount, sizeof(indices[0]),
        reinterpret_cast<int(__cdecl*)(const void*, const void*)>(SortCallBack1));
    box->Reset();
    for (int order = 0; order < m_vidCount; ++order)
    {
        const int index = indices[order];
        VID* definition = m_vids[index];
        if (!definition || !(definition->spriteType & type)) continue;
        int item;
        {
            const STRING label = definition->GetNumberName();
            item = box->AddStringWithData(&label, index);
        }
        if (m_vids[index]->nVid == selected) box->SetCurrent(item);
    }
    return box->NoString();
}
