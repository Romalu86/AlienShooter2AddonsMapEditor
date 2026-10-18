#include "editor_dialogs.h"
#include "engine_globals.h"
#include "sprite_collector.h"
#include "sprite.h"
#include "map.h"
#include "vid.h"
#include "resread.h"
#include <cstring>

namespace {

void MarkUsedVid(VID* vid, int* used)
{
    if (!vid || used[vid->nVid])
        return;
    used[vid->nVid] = 1;
    MarkUsedVid(vid->linkVid, used);
    for (int animation = 0; animation < VID::AnimationSlotCount; ++animation)
        MarkUsedVid(vid->childVid[animation], used);
}

void InsertMapInfoName(STRING& names,STRING name)
{
    if (std::strstr(names.CharPtr(),("["+name+"]").CharPtr())) return;
    if (names!="")
    {
        STRING remaining(names);
        for (;remaining!="";remaining=remaining.After("]  "))
        {
            const STRING current=remaining.Before("]").After("[");
            if (name[0]=='m')
            {
                if (current.After("m").Int()>name.After("m").Int()) break;
            }
            else if (name[0]=='h')
            {
                if (current.After("h").Int()>name.After("h").Int()) break;
            }
            else if (current.Before("_").Int()>name.Before("_").Int() ||
                (current.Before("_").Int()==name.Before("_").Int() &&
                    current.After("_").Int()>name.After("_").Int()))
                break;
        }
        if (remaining=="") names+="["+name+"]  ";
        else names=names.Before(remaining)+"["+name+"]  "+remaining;
    }
    else names+="["+name+"]  ";
}
}

INT_PTR CALLBACK AppUnusedVid(HWND window, UINT message, WPARAM wParam, LPARAM)
{
    if (message == WM_INITDIALOG)
    {
        STRING files[4096];
        int fileCount = 0;
        int referencedFiles[4096] = {};
        int usedVids[4096] = {};
        int missingVids[4096];
        int missingCount = 0;
        STRING report("Unused vids in vid folder: ");
        const char* masks[] = {"vid\\*.vid", "vid\\*.bmp", "vid\\*.tga", "vid\\*.jpg"};
        for (const char* mask : masks)
        {
            int findHandle;
            for (STRING file = FFirstFile(&findHandle, STRING(mask), 0); !file.IsEmpty();
                 file = FNextFile(&findHandle, 0))
                files[fileCount++] = file;
        }

        for (int fileIndex = 0; fileIndex < fileCount; ++fileIndex)
        {
            const STRING file = files[fileIndex].ToLower();
            const int number = files[fileIndex].Before(".").Int();
            if (Map->Vid(number)->vidName.ToLower().After("vid\\") == file)
                referencedFiles[fileIndex] = 1;
            else
                for (int vidIndex = 0; vidIndex < 4096; ++vidIndex)
                    if (Map->ValidateVid(vidIndex) &&
                        Map->Vid(vidIndex)->vidName.ToLower().After("vid\\") == file)
                    {
                        referencedFiles[fileIndex] = 1;
                        break;
                    }
        }

        for (int fileIndex = 0; fileIndex < fileCount; ++fileIndex)
            if (!referencedFiles[fileIndex] && std::strchr(files[fileIndex].CharPtr(), '_'))
                report += files[fileIndex] + " ";
        for (int fileIndex = 0; fileIndex < fileCount; ++fileIndex)
            if (!referencedFiles[fileIndex] && !std::strchr(files[fileIndex].CharPtr(), '_'))
                report += files[fileIndex] + " ";

        report += "\r\nUnused in maps vid: ";
        int findHandle;
        for (STRING file = FFirstFile(&findHandle, STRING("maps\\*.map"), 0); !file.IsEmpty();
             file = FNextFile(&findHandle, 0))
        {
            RESOURCE resource;
            if (resource.OpenForRead("maps\\" + file, ResourceType::Map))
            {
                ::Error->Log("!!!ERROR!!!LOAD: Invalid map file %s", file.CharPtr());
                continue;
            }
            if (resource.GoBegin(ResourceType::Sprites))
                continue;

            int spriteReference;
            resource.Read(&spriteReference, sizeof(spriteReference));
            while (spriteReference != -1)
            {
                int number;
                resource.Read(&number, sizeof(number));
                if (Map->ValidateVid(number))
                    MarkUsedVid(Map->Vid(number), usedVids);
                else
                    missingVids[missingCount++] = number;
                resource.Shift(40);
                resource.Read(&spriteReference, sizeof(spriteReference));
            }
        }

        for (int number = 0; number < 4096; ++number)
            if (Map->ValidateVid(number) && !usedVids[number] && (Map->Vid(number)->spriteType & 14u))
                report += Int2Str(number) + " ";
        report += "\r\nUsed in maps, but not exist: ";
        for (int index = 0; index < missingCount; ++index)
            report += Int2Str(missingVids[index]) + " ";
        report += "\r\nUnused in men menu vid: ";
        DIALOG_TEXT output(window, 1007);
        output = &report;
        return 1;
    }
    if (message == WM_COMMAND)
    {
        if (LOWORD(wParam) == IDOK)
            EndDialog(window, 1);
        else if (LOWORD(wParam) == IDCANCEL)
            EndDialog(window, 0);
    }
    return 0;
}

INT_PTR CALLBACK AppMapInfo(HWND window,UINT message,WPARAM wParam,LPARAM)
{
    DIALOG_TEXT output(window,1007);
    if (message==WM_INITDIALOG) {
        STRING scriptVariables,waitForVariables,setQuests,waitForQuests,dialogs,mNames,hNames,report;
        int invalidGotos=0;
        int iterator;
        for (SPRITE* sprite=Sprites->FirstActiveSprite(&iterator);sprite;sprite=Sprites->NextActiveSprite(&iterator)) {
            if (sprite->GetName()[0]=='m' && sprite->GetName()[1]>='0' && sprite->GetName()[1]<='9')
                InsertMapInfoName(mNames,sprite->GetName().Before(" "));
            if (sprite->GetName()[0]=='h' && sprite->GetName()[1]>='0' && sprite->GetName()[1]<='9')
                InsertMapInfoName(hNames,sprite->GetName().Before(" "));
            for (int lane=0;lane<4;++lane) {
                const LIST<ACT>& actions=sprite->ActionStack(lane);
                for (int index=0;index<actions.m_count;++index) {
                    const ACT action=actions.m_data[index];
                    const STRING pair=Int2Str(action.arg1)+"_"+Int2Str(action.arg2);
                    switch (action.command) {
                    case 79: InsertMapInfoName(waitForVariables,pair); break;
                    case 131: InsertMapInfoName(scriptVariables,pair); break;
                    case 77: InsertMapInfoName(waitForQuests,pair); break;
                    case 78: InsertMapInfoName(setQuests,pair); break;
                    case 115: case 116: InsertMapInfoName(dialogs,pair); break;
                    case 71: if (action.arg1>=actions.m_count) ++invalidGotos; break;
                    }
                }
            }
        }
        STRING remaining(scriptVariables);
        int lastVariable=-1;
        while (!remaining.IsEmpty()) {
            const int variable=remaining.Before("_").After("[").Int();
            if (lastVariable>=0 && variable!=lastVariable)
                scriptVariables=scriptVariables.Before(remaining)+"\r\n"+remaining;
            lastVariable=variable;
            remaining=remaining.After("]  ");
        }
        report="   SCRIPT_VAR\r\n"+scriptVariables+
            "\r\n\r\n   WHILE_NOT_SCRIPT_VAR\r\n"+waitForVariables+
            "\r\n\r\n   SET_QUEST\r\n"+setQuests+
            "\r\n\r\n   WHILE_NOT_QUEST\r\n"+waitForQuests+
            "\r\n\r\n   SET_DIALOG\r\n"+dialogs+
            "\r\n\r\n   mN name use\r\n"+mNames+
            "\r\n\r\n   hN name use\r\n"+hNames;
        if (invalidGotos) report+="\r\n\r\n   Invalidate ACT_GOTO_STACK "+Int2Str(invalidGotos);
        output=&report;
        return 1;
    }
    if (message==WM_COMMAND) {
        if (LOWORD(wParam)==IDOK) EndDialog(window,1);
        else if (LOWORD(wParam)==IDCANCEL) EndDialog(window,0);
    }
    return 0;
}
