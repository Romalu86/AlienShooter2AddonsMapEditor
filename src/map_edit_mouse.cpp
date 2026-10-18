#include "map_edit.h"
#include "vid.h"
#include "region.h"
#include "engine_globals.h"
#include <commctrl.h>
#include <cstddef>
#include <cstring>

void MAP_EDIT::ChangeMouseVid(VID* vid, unsigned int type)
{
    static int previousSpriteType = -1;
    optShiftSnapX = 0;
    optShiftSnapY = 0;
    if (!vid) return;
    if (!(vid->spriteType & type))
    {
        if (GetRememberedVid(type))
            vid = GetRememberedVid(type);
        else
        {
            vid = Vid(NextVid(vid->nVid, type));
            if (vid == ::EmptyVid) return;
            SetRememberedVid(type, vid);
        }
    }
    else
        SetRememberedVid(type, vid);

    previousSpriteType = spriteType;
    spriteType = static_cast<int>(type);
    if (options.optTacticMode)
        vid = Vid(1);
    else
    {
        if (previousSpriteType != spriteType) selectedSprites.Release();
        if ((previousSpriteType >= 64 && spriteType < 64) ||
            (previousSpriteType < 64 && spriteType >= 64))
        {
            SendMessageA(hToolBar, TB_ENABLEBUTTON, 40025, spriteType < 64);
            SendMessageA(hToolBar, TB_ENABLEBUTTON, 40034, spriteType < 64);
            SendMessageA(hToolBar, TB_ENABLEBUTTON, 40053, spriteType <= 128);
            SendMessageA(hToolBar, TB_ENABLEBUTTON, 45085, spriteType < 128);
            SendMessageA(hToolBar, TB_ENABLEBUTTON, 45097, spriteType < 64);
            SendMessageA(hToolBar, TB_ENABLEBUTTON, 45099, spriteType < 64);
        }
    }

    if (Mouse->Vid() != vid || spriteType != previousSpriteType)
    {
        MENUITEMINFOA info = {};
        info.cbSize = sizeof(info);
        info.fMask = MIIM_TYPE;
        info.fType = MFT_STRING;
        STRING numberName;
        const char* mode;
        if (options.optTacticMode)
            mode = "Tactic mode";
        else if (spriteType >= 128)
        {
            numberName = vid->GetNumberName();
            mode = numberName.CharPtr();
        }
        else
            mode = "Unknown mode";
        info.dwTypeData = const_cast<char*>(mode);
        info.cch = static_cast<UINT>(std::strlen(mode));
        SetMenuItemInfoA(GetMenu(reinterpret_cast<HWND>(m_dialog)), 7, TRUE, &info);
        DrawMenuBar(reinterpret_cast<HWND>(m_dialog));
        Mouse->Action(62, vid->nVid, 0, 0);
        if (options.optControlPanel)
            DialogControlPanel(hControlPanel,
                spriteType == previousSpriteType ? 24577 : 24576, 0, 0);
        if (spriteType != previousSpriteType)
        {
            unsigned command = 0;
            switch (spriteType)
            {
            case 1: command = 41114; break;
            case 2: command = 40047; break;
            case 4: command = 40048; break;
            case 8: command = 40049; break;
            case 16: command = 40050; break;
            case 32: command = 40051; break;
            case 64: command = 45091; break;
            case 0x600: command = 41129; break;
            case 128: command = 45090; break;
            }
            if (command) SendMessageA(hToolBar, TB_CHECKBUTTON, command, 1);
        }
    }
    if (type == 128u)
    {
        if (!curRegion) Right();
        if (curRegion) Mouse->Action(62, curRegion->Vid()->nVid, 0, 0);
    }
}
