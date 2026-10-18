#include "map.h"
#include "dialog_controls.h"
#include "vid.h"
#include "engine_globals.h"
#include <cstdlib>

int __cdecl SortCallBack1(const int* first, const int* second);

int VID::NoTreeName() const
{
    STRING branch(name);
    int count=0;
    while (branch!="")
    {
        branch=branch.After(":");
        ++count;
    }
    return count;
}

STRING VID::GetTreeName(int level) const
{
    if (level==0)
        return Printf("%04i %s",nVid,name.CountChar(':')?name.AfterLast(":").CharPtr():name.CharPtr());
    STRING branch(name);
    for (;level<0;++level) branch=branch.BeforeLast(":");
    return branch.CountChar(':')?branch.AfterLast(":"):branch;
}

void MAP::VidToTree(DIALOG_TREE* tree,unsigned long type,int selected,int sort) const
{
    int indices[VidSlotCount];
    for (int i=0;i<m_vidCount;++i) indices[i]=i;
    if (sort) std::qsort(indices,m_vidCount,sizeof(indices[0]),
        reinterpret_cast<int(__cdecl*)(const void*,const void*)>(SortCallBack1));
    tree->Hide(); tree->Reset();
    for (int order=0;order<m_vidCount;++order) {
        const int index=indices[order];
        if (!m_vids[index]) continue;
        if (!(m_vids[index]->spriteType&type)) { m_vids[index]->treeItem=0; continue; }
        HTREEITEM parent=0; int level=1-m_vids[index]->NoTreeName();
        for (;level<0;++level) {
            HTREEITEM found=reinterpret_cast<HTREEITEM>(tree->GetStringIndex(
                m_vids[index]->GetTreeName(level),reinterpret_cast<int>(parent)));
            if (!found) found=tree->Add(m_vids[index]->GetTreeName(level),parent,0,true);
            parent=found;
        }
        const HTREEITEM item=tree->Add(m_vids[index]->GetTreeName(level),parent,index,false);
        m_vids[index]->treeItem=item;
    }
    tree->Message(TVM_SORTCHILDREN,0,0);
    tree->SetCurrent(ValidateVid(selected)?Vid(selected)->treeItem:0);
    tree->Show();
}
