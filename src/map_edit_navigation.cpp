#include "map_edit.h"
#include "vid.h"
#include "region.h"
#include "graph.h"
#include "sprite_collector.h"
#include "engine_globals.h"
#include "editor_dialogs.h"
#include <commctrl.h>

void MAP_EDIT::ChangeMouseCoorWithSnap()
{
    int x = TruncateFloatToInt32OrMin(Mouse->X());
    int y = TruncateFloatToInt32OrMin(Mouse->Y());
    const int stepX = TruncateFloatToInt32OrMin(Mouse->Vid()->sizeXYZ.x);
    const int stepY = TruncateFloatToInt32OrMin(Mouse->Vid()->sizeXYZ.y);
    if (stepY) y /= stepY;
    if (stepX)
    {
        x /= stepX;
        const int offset = options.optChessSnap && (y & 1) ? stepX / 2 : 0;
        x = x * stepX + stepX / 2 - offset;
    }
    if (stepY) y = y * stepY + stepY / 2;
    Mouse->ChangeCoor(VECTOR(static_cast<float>(static_cast<double>(x) + optShiftSnapX),
                            static_cast<float>(static_cast<double>(y) + optShiftSnapY), Mouse->Z()));
}

int MAP_EDIT::Left()
{
    if (options.optTacticMode)
    {
        if (m_groups.First())
        {
            selectedSprites.Release();
            m_groups.ShiftFirstLeft();
            GROUP* group = m_groups.First();
            for (int index = 0; index < group->No(); ++index)
                selectedSprites.Insert(*(*group)[index]);
            if (group->No())
                SetShiftCoor(VECTOR2(group->m_centerX, group->m_centerY), 0);
        }
    }
    else if (spriteType == 0x80)
    {
        if (curRegion)
        {
            int index;
            if (curRegion != ::Sprites->FirstDrawingSprite(&index, 5))
            {
                REGION* previous = 0;
                for (SPRITE* sprite = ::Sprites->FirstDrawingSprite(&index, 5);
                     sprite && sprite != curRegion;
                     sprite = ::Sprites->NextDrawingSprite(&index, 5))
                    if (sprite->IsSpriteClass(23))
                        previous = static_cast<REGION*>(sprite);
                if (previous)
                {
                    curRegion = previous;
                    SetShiftCoor(VECTOR2(curRegion->X(), curRegion->Y()), 0);
                }
                if (curRegion) return curRegion->Vid()->nVid;
            }
        }
    }
    else
        return PrevVid(Mouse->Vid()->nVid, spriteType);
    return Mouse->Vid()->nVid;
}

int MAP_EDIT::Right()
{
    if (options.optTacticMode)
    {
        if (m_groups.First())
        {
            selectedSprites.Release();
            m_groups.ShiftFirstRight();
            GROUP* group = m_groups.First();
            for (int index = 0; index < group->No(); ++index)
                selectedSprites.Insert(*(*group)[index]);
            SetShiftCoor(VECTOR2(group->m_centerX, group->m_centerY), 0);
        }
    }
    else if (spriteType == 0x80)
    {
        SPRITE* sprite = curRegion;
        if (!sprite)
        {
            int index;
            sprite = ::Sprites->FirstDrawingSprite(&index, 5);
            while (sprite && !sprite->IsSpriteClass(23))
                sprite = ::Sprites->NextDrawingSprite(&index, 5);
        }
        if (sprite)
        {
            curRegion = static_cast<REGION*>(sprite);
            SetShiftCoor(VECTOR2(curRegion->X(), curRegion->Y()), 0);
        }
        if (curRegion) return curRegion->Vid()->nVid;
    }
    else
        return NextVid(Mouse->Vid()->nVid, spriteType);
    return Mouse->Vid()->nVid;
}

int MAP_EDIT::IsLeftPossible()
{
    return options.optTacticMode ? m_groups.First() != 0 : spriteType != 0x80;
}
int MAP_EDIT::IsRightPossible()
{
    return options.optTacticMode ? m_groups.First() != 0 : 1;
}

int MAP_EDIT::CallDialogBox(const STRING& name, DLGPROC procedure)
{
    Graph->BeginPause();
    const int result = static_cast<int>(DialogBoxParamA(
        static_cast<HINSTANCE>(m_hInstance), name.CharPtr(),
        reinterpret_cast<HWND>(m_dialog), procedure, 0));
    Graph->EndPause();
    return result;
}

void MAP_EDIT::SetControlPanel(int flag)
{
    static VECTOR2 previousViewMax(0, 0);
    if (options.optControlPanel && !flag)
    {
        if (hControlPanel) DestroyWindow(hControlPanel);
        hControlPanel = 0;
        RECT toolbar;
        GetWindowRect(hToolBar, &toolbar);
        Graph->SetViewPort(Graph->ViewMin(), previousViewMax);
    }
    else if (!options.optControlPanel && flag)
    {
        previousViewMax = Graph->ViewMax();
        hControlPanel = CreateDialogParamA(static_cast<HINSTANCE>(m_hInstance),
            "CONTROL_PANEL", reinterpret_cast<HWND>(m_dialog), AppControlPanel, 0);
        RECT client, toolbar, panel;
        GetClientRect(reinterpret_cast<HWND>(m_dialog), &client);
        GetWindowRect(hToolBar, &toolbar);
        GetWindowRect(hControlPanel, &panel);
        const int width = panel.right - panel.left;
        SetWindowPos(hControlPanel, 0,
            TruncateFloatToInt32OrMin(Graph->SizeX()) -
                TruncateFloatToInt32OrMin(Graph->ViewMin().x) - width,
            client.top + toolbar.bottom - toolbar.top - 2, width,
            TruncateFloatToInt32OrMin(Graph->SizeY()) + 2 -
                TruncateFloatToInt32OrMin(Graph->ViewMin().y), SWP_SHOWWINDOW);
        Graph->SetViewPort(Graph->ViewMin(),
            VECTOR2(static_cast<float>(static_cast<double>(Graph->SizeXY().x) - width),
                    Graph->ViewMax().y));
        SetFocus(reinterpret_cast<HWND>(m_dialog));
        EnableWindow(hControlPanel, FALSE);
    }
    options.optControlPanel = flag != 0;
    SendMessageA(hToolBar, TB_CHECKBUTTON, 41107, options.optControlPanel);
    CheckMenuItem(GetMenu(reinterpret_cast<HWND>(m_dialog)), 41107,
        options.optControlPanel ? MF_CHECKED : MF_UNCHECKED);
}
