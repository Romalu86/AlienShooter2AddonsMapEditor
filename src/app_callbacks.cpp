#include "dialog_controls.h"
#include "runtime/win32_callbacks.h"
#include "graph.h"
#include "map.h"
#include "registry.h"

INT_PTR __stdcall AppStart(HWND hwnd, UINT message, WPARAM wParam, LPARAM)
{
    DIALOG_COMBO_BOX drivers(hwnd, 1057);
    DIALOG_COMBO_BOX modes(hwnd, 1058);
    DIALOG_BUTTON fullScreen(hwnd, 2001);
    DIALOG_BUTTON soundHighQuality(hwnd, 2007);

    if (message == WM_INITDIALOG)
    {
        soundHighQuality.SetCheck(Registry->GetInt(STRING("SoundHighQuality"), 0));
        Graph->UpdateStartupDialog(&drivers, &modes, &fullScreen);
        return TRUE;
    }
    if (message == WM_COMMAND)
    {
        const unsigned int command = LOWORD(wParam);
        if (command == IDOK)
        {
            Graph->UpdateStartupDialog(&drivers, &modes, &fullScreen);
            Registry->SetInt(STRING("SoundHighQuality"), soundHighQuality.GetCheck());
            EndDialog(hwnd, 1);
        }
        else if (command == IDCANCEL)
            EndDialog(hwnd, 0);
        else if ((command == 1057u || command == 1058u) && HIWORD(wParam) == CBN_SELENDOK)
            Graph->UpdateStartupDialog(&drivers, &modes, &fullScreen);
    }
    return FALSE;
}

LRESULT __stdcall AppWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (Map->WorkWndMessage(hwnd, message, wParam, static_cast<unsigned long>(lParam)))
        return 1;
    return DefWindowProcA(hwnd, message, wParam, lParam);
}
