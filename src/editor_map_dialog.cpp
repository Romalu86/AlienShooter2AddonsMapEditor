#include "map_edit.h"
#include "editor_dialogs.h"
#include "graph.h"
#include "region.h"
#include "sprite_collector.h"
#include "sprite.h"
#include "vid.h"
#include "engine_globals.h"

namespace
{
void DeleteEmptyTop(MAP_EDIT* editor)
{
    LIST<SPRITE*> sprites;
    for (int y = 0; y < Sprites->gridHeight; ++y)
    {
        for (int x = 0; x < Sprites->gridWidth; ++x)
        {
            const int cellIndex = (y << Sprites->gridShift) + x;
            const SPRITE_COLLECTOR::CELL& cell = Sprites->spatialCells[cellIndex];
            for (int layer = 0; layer < 15; ++layer)
            {
                const SPRITE_LIST& list = cell.layerSprites[layer];
                int index;
                for (SPRITE* sprite = list.BeginIterateNon0(&index); sprite;
                     sprite = list.NextIterateNon0(&index))
                {
                    if (!sprite->ChildBacklink())
                        sprites.Insert(sprite);
                }
            }
        }
    }
    SPRITE* upperSprite = 0;
    int index;
    for (SPRITE* sprite = sprites.BeginIterateNon0(&index); sprite;
         sprite = sprites.NextIterateNon0(&index))
    {
        if (!upperSprite || upperSprite->Y() > sprite->Y())
            upperSprite = sprite;
    }
    if (upperSprite && upperSprite->Y() > 0.0f)
    {
        const float top = upperSprite->Y();
        Error->Log("DELETE_EMPTY_TOP upper sprite vid[%i](%i, %i, %i)",
            upperSprite->Vid()->nVid, TruncateFloatToInt32OrMin(upperSprite->X()),
            TruncateFloatToInt32OrMin(top), TruncateFloatToInt32OrMin(upperSprite->Z()));
        for (SPRITE* sprite = sprites.BeginIterateNon0(&index); sprite;
             sprite = sprites.NextIterateNon0(&index))
            sprite->ChangeCoor(VECTOR(sprite->X(), sprite->Y() - top, sprite->Z()));
        editor->ChangeSizeXY(VECTOR2(Map->SizeXY().x, Map->SizeXY().y - top));
        editor->SetScrollBox(VECTOR2(-450, -450), Map->SizeXY() + VECTOR2(450, 450));
    }
}
}

void UpdateScreenForDlg(HWND window)
{
    Graph->ClearScreen(COLOR(0, 0, 0));
    Graph->PreTact();
    Graph->Tact(1);
    Graph->PostTact(1);
    RedrawWindow(window, 0, 0, 0x485u);
}

INT_PTR CALLBACK AppMapProperty(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    return static_cast<MAP_EDIT*>(Map)->DialogMapProperty(window, message, wParam, lParam);
}

int MAP_EDIT::DialogMapProperty(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    static GAMMA originalGamma;
    static int fadePosition;
    DIALOG_UNSIGNED sizeX(window, 0x3F0), sizeY(window, 0x3F1);
    DIALOG_UNSIGNED windDirection(window, 0x3F2), windSpeed(window, 0x3F3);
    DIALOG_TEXT gammaText(window, 0x3EF);
    DIALOG_UNSIGNED gammaValue(window, 0x3F9);
    DIALOG_SLIDER red(window, 0x409), green(window, 0x408), blue(window, 0x407);
    DIALOG_SLIDER fade(window, 0x418);
    DIALOG_SLIDER sunRed(window, 0x40A), sunGreen(window, 0x40B), sunBlue(window, 0x40C);
    DIALOG_SLIDER sunDirection(window, 0x45D), sunElevation(window, 0x45E);
    DIALOG_BUTTON sunLight(window, 0x45F);
    DIALOG_BUTTON environment(window, 0x41A);
    DIALOG_BUTTON environmentC00(window, 0x415), environmentC000(window, 0x419);

    if (gammaValue.IsNotify(message, static_cast<unsigned int>(wParam), EN_UPDATE))
    {
        Graph->SetGamma(GAMMA(static_cast<GAMMA::GAMMA_CREATE>(0), gammaValue.GetUnsigned()));
        const STRING value = Printf("%X", Graph->GetGamma().EncodeToDword());
        gammaText = &value;
        UpdateScreenForDlg(window);
    }

    if (sunLight.IsNotify(message, static_cast<unsigned int>(wParam), BN_CLICKED))
    {
        Graph->SetEnvironment(sunLight.GetCheck() ? 0x10000u : 0x80010000u);
        UpdateScreenForDlg(window);
    }

    if (message == WM_INITDIALOG)
    {
        originalGamma = Graph->GetGamma();
        sizeX = static_cast<unsigned int>(TruncateFloatToInt32(Map->SizeXY().x));
        sizeY = static_cast<unsigned int>(TruncateFloatToInt32(Map->SizeXY().y));
        windDirection = static_cast<unsigned int>(Graph->WindDirection().value);
        windSpeed = static_cast<unsigned int>(TruncateDoubleToInt32(static_cast<double>(Graph->WindSpeed()) * 1000.0));
        environment.SetCheck(Graph->IsEnvironment(1u));
        environmentC00.SetCheck(Graph->IsEnvironment(0xC00u));
        environmentC000.SetCheck(Graph->IsEnvironment(0xC000u));
        sunLight.SetCheck(Graph->IsEnvironment(0x10000u));
        red.SetRange(-255, 255);
        red = -Graph->GetGamma().Red();
        green.SetRange(-255, 255);
        green = -Graph->GetGamma().Green();
        blue.SetRange(-255, 255);
        blue = -Graph->GetGamma().Blue();
        fade.SetRange(-255, 255);
        fade = 0;
        fadePosition = 0;
        const STRING value = Printf("%X", Graph->GetGamma().EncodeToDword());
        gammaText = &value;
        gammaValue = Graph->GetGamma().EncodeToDword();
        sunRed.SetRange(0, 255);
        sunGreen.SetRange(0, 255);
        sunBlue.SetRange(0, 255);
        sunRed = 255 - static_cast<int>((Graph->sunColor.value >> 16) & 255u);
        sunGreen = 255 - static_cast<int>((Graph->sunColor.value >> 8) & 255u);
        sunBlue = 255 - static_cast<int>(Graph->sunColor.value & 255u);
        sunDirection.SetRange(0, 255);
        sunDirection = Graph->sunDirection;
        sunElevation.SetRange(0, 63);
        sunElevation = 63 - Graph->sunElevation;
        return 1;
    }

    if (message == WM_COMMAND)
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
            if (sizeX.GetUnsigned() != static_cast<unsigned int>(TruncateFloatToInt32(Map->SizeXY().x)) ||
                sizeY.GetUnsigned() != static_cast<unsigned int>(TruncateFloatToInt32(Map->SizeXY().y)))
                Map->ChangeSizeXY(VECTOR2(static_cast<float>(sizeX.GetUnsigned()),
                                          static_cast<float>(sizeY.GetUnsigned())));
            Map->SetScrollBox(VECTOR2(-450, -450), Map->SizeXY() + VECTOR2(450, 450));
            Graph->SetWind(BitsToSigned32(windSpeed.GetUnsigned()),
                           ANGLE(static_cast<unsigned char>(windDirection.GetUnsigned())));
            Graph->SetEnvironment(environment.GetCheck() ? 1u : 0x80000001u);
            Graph->SetEnvironment(environmentC00.GetCheck() ? 0xC00u : 0x80000C00u);
            Graph->SetEnvironment(environmentC000.GetCheck() ? 0xC000u : 0x8000C000u);
            Graph->SetEnvironment(sunLight.GetCheck() ? 0x10000u : 0x80010000u);
            EndDialog(window, 1);
            break;
        case IDCANCEL:
            Graph->SetGamma(originalGamma);
            EndDialog(window, 0);
            break;
        case 0x460:
            DeleteEmptyTop(this);
            EndDialog(window, 1);
            break;
        case 0x45F:
            UpdateScreenForDlg(window);
            break;
        case 0x437:
        case 0x438:
        case 0x439:
        {
            GAMMA gamma = Graph->GetGamma();
            if (LOWORD(wParam) == 0x437) { gamma.SetRed(0); red = 0; }
            if (LOWORD(wParam) == 0x438) { gamma.SetGreen(0); green = 0; }
            if (LOWORD(wParam) == 0x439) { gamma.SetBlue(0); blue = 0; }
            Graph->SetGamma(gamma);
            const STRING value = Printf("%X", Graph->GetGamma().EncodeToDword());
            gammaText = &value;
            gammaValue = Graph->GetGamma().EncodeToDword();
            UpdateScreenForDlg(window);
            break;
        }
        }
        return 0;
    }

    if (message == WM_MOVE)
        UpdateScreenForDlg(window);
    if (message == WM_VSCROLL && (LOWORD(wParam) == TB_ENDTRACK || LOWORD(wParam) == TB_THUMBTRACK))
    {
        if (GetDlgCtrlID(reinterpret_cast<HWND>(lParam)) == 0x418)
        {
            const int difference = static_cast<int>(fade) - fadePosition;
            fadePosition += difference;
            red = static_cast<int>(red) + difference;
            green = static_cast<int>(green) + difference;
            blue = static_cast<int>(blue) + difference;
            if (LOWORD(wParam) == TB_ENDTRACK)
            {
                fadePosition = 0;
                fade = 0;
            }
        }
        Graph->SetGamma(GAMMA(-static_cast<int>(red), -static_cast<int>(green), -static_cast<int>(blue)));
        const STRING value = Printf("%X", Graph->GetGamma().EncodeToDword());
        gammaText = &value;
        gammaValue = Graph->GetGamma().EncodeToDword();
        Graph->SetSunLight(COLOR(255 - static_cast<int>(sunRed),
            255 - static_cast<int>(sunGreen), 255 - static_cast<int>(sunBlue)),
            ANGLE(static_cast<unsigned char>(static_cast<int>(sunDirection))),
            ANGLE(static_cast<unsigned char>(63 - static_cast<int>(sunElevation))));
        UpdateScreenForDlg(window);
    }
    if (message == WM_HSCROLL && (LOWORD(wParam) == TB_ENDTRACK || LOWORD(wParam) == TB_THUMBTRACK))
    {
        Graph->SetSunLight(COLOR(255 - static_cast<int>(sunRed),
            255 - static_cast<int>(sunGreen), 255 - static_cast<int>(sunBlue)),
            ANGLE(static_cast<unsigned char>(static_cast<int>(sunDirection))),
            ANGLE(static_cast<unsigned char>(63 - static_cast<int>(sunElevation))));
        gammaValue = static_cast<unsigned int>(static_cast<int>(sunDirection));
        UpdateScreenForDlg(window);
    }
    return 0;
}
