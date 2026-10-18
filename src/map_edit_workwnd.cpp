#include "map_edit.h"
#include "editor_dialogs.h"
#include "string_stream.h"
#include "polygon.h"
#include "sprite_collector.h"
#include "frame.h"
#include "vid.h"
#include "graph.h"
#include "engine_globals.h"
#include "myerror.h"
#include <commctrl.h>
#include <cstring>
#include <cstdio>
#include <new>
#include <initializer_list>

namespace
{

int clipboardCount = 0;
int selectedSpriteIndex = 0;
int invalidActionIndex = -1;
int blockedUnitIndex = -1;
int stackSearchIndex = -1;
int nameSearchIndex = -1;
int vidSearchIndex = 100000;
char tooltipText[256];

const char saveFilter[] =
    "Map file\0*.map\0Menu file\0*.men\0AllMap(bmp) file\0*.bmp\0GridZ() file\0*.p\0\0";
const char openFilter[] =
    "Map files\0*.map\0Menu files\0*.men\0Terrain files\0*.vid\0GridZ files\0*.tga;*.z\0All Files\0*.*\0\0";

VECTOR2 MenuSpriteHalfSize(SPRITE* sprite)
{
    const VECTOR scale = sprite->GetScale();
    const VECTOR2 scaled(sprite->Vid()->buildSizeX * scale.x,
                         sprite->Vid()->buildSizeY * scale.y);
    return (scaled * 1.3f) / 2.0f;
}
}

int MAP_EDIT::WorkWndMessage(HWND window, unsigned long message,
                            unsigned long wParam, unsigned long lParam)
{

    static STRING_STREAM spriteClipboard;
    static VECTOR2 clipboardView;
    static VECTOR2 clipboardShift;
    static VECTOR clipboardAverage;
    const HWND mainWindow = reinterpret_cast<HWND>(m_dialog);

    if (message == WM_NOTIFY)
    {
        NMTTDISPINFOA* tooltip = reinterpret_cast<NMTTDISPINFOA*>(lParam);
        if (tooltip->hdr.code == static_cast<UINT>(TTN_NEEDTEXTA))
        {
            LoadStringA(Instance(), static_cast<UINT>(tooltip->hdr.idFrom), tooltipText, 256);
            tooltip->lpszText = tooltipText;
        }
        return 0;
    }
    if (message == WM_INITMENUPOPUP) return 0;

    if (message == WM_COMMAND)
    {
        const unsigned int command = wParam & 0xFFFFu;
        const auto selectAndCenter = [&](SPRITE* sprite)
        {
            if (!options.optTacticMode) SendMessageA(mainWindow, WM_COMMAND, 40032, 0);
            selectedSprites.Release();
            selectedSprites.Insert(sprite);
            SetShiftCoor(VECTOR2(sprite->X(), sprite->Y()), 2);
        };

        switch (command)
        {
        case 666:
            if (((wParam >> 16) & 0xFFFFu) == 666u) Graph->EndPause();
            break;
        case 41138:
        {
            const STRING time = GetTimeString();
            STRING file = "\\Screens\\" + GetDateString() + " " + time + ".bmp";
            file.Replace(":", "h");
            file.Replace(":", "m");
            file.Replace(":", "s");
            Graph->SaveScreenShot(file, 0, 0,
                TruncateFloatToInt32OrMin(Graph->SizeX()), TruncateFloatToInt32OrMin(Graph->SizeY()));
            break;
        }
        case 41115:
            if (questEditorModule)
            {
                typedef int (__cdecl *CREATE_QUEST_WINDOW)(HMODULE, HINSTANCE, const char*, int, HWND);
                FARPROC address = GetProcAddress(questEditorModule, "CreateNew_zQuestEditorMainWnd");
                if (address)
                {
                    CREATE_QUEST_WINDOW createWindow;

                    std::memcpy(&createWindow, &address, sizeof(createWindow));
                    Graph->BeginPause();
                    if (!createWindow(GetModuleHandleA(0), 0, GetCommandLineA(), 0, window))
                        Error(E_CREATE, STRING("CreateWnd for QuestEditor"), 0);
                }
                else Error(E_GET, STRING("GetProcAddress(CreateNew_zQuestEditorMainWnd)"), 0);
            }
            break;
        case 41111:
        case 41134:
            if (options.optTacticMode && selectedSprites.No())
            {
                m_relation.Release();
                clipboardCount = selectedSprites.No();
                spriteClipboard.Clear();
                clipboardAverage = VECTOR(0, 0, 0);
                for (int i = 0; i < selectedSprites.No(); ++i)
                    selectedSprites.m_data[i]->Save(&spriteClipboard);
                for (int i = 0; i < selectedSprites.No(); ++i)
                {
                    SPRITE* sprite = selectedSprites.m_data[i];
                    sprite->Action(80, BitsToSigned32(PtrToU32(&spriteClipboard)), 0, 0);
                    clipboardAverage = clipboardAverage + sprite->Coor();
                    if (sprite->Vid()->spriteType & 32u)
                    {
                        clipboardView = Graph->SizeXY();
                        clipboardShift = FromScreen(VECTOR2(0, 0));
                    }
                }
                const float inverseCount = 1.0f/static_cast<float>(selectedSprites.No());
                clipboardAverage = clipboardAverage*inverseCount;
                if (command == 41134)
                {
                    undo.Begin();
                    for (int i = 0; i < selectedSprites.No(); ++i)
                        undo.AddRemove(selectedSprites.m_data[i]);
                    undo.End();
                    selectedSprites.Release();
                }
            }
            else
                Printf("%i,%i,%i", TruncateFloatToInt32OrMin(Mouse->X()),
                       TruncateFloatToInt32OrMin(Mouse->Y()), TruncateFloatToInt32OrMin(Mouse->Z()))
                    .WriteToClipboard(mainWindow);
            break;
        case 41109:
            if (options.optTacticMode && clipboardCount)
            {

                static LIST<SPRITE*> pastedSprites;
                pastedSprites.Release();
                spriteClipboard.ResetPosition();
                for (int i = 0; i < clipboardCount; ++i)
                    pastedSprites.Insert(LoadSprite(&spriteClipboard, 20));
                if (clipboardCount > 1 || (pastedSprites.m_data[0]->Vid()->spriteType & 32u))
                    clipboardAverage.z = Mouse->Z();
                selectedSprites.Release();
                for (int i = 0; i < clipboardCount; ++i)
                {
                    SPRITE* sprite = pastedSprites.m_data[i];
                    if (!sprite) continue;
                    sprite->Action(81, BitsToSigned32(PtrToU32(&spriteClipboard)), 20, 999);
                    const VECTOR offset = Mouse->Coor() - clipboardAverage;
                    sprite->ChangeCoor(offset + sprite->Coor());
                    if (sprite->Vid()->spriteType & 32u)
                    {
                        const VECTOR2 position = VECTOR2(sprite->X(), sprite->Y()) - clipboardShift
                            - clipboardView / 2.0f + Graph->SizeXY() / 2.0f
                            + VECTOR2(0.0f, sprite->Z());
                        sprite->ChangeCoor(VECTOR(position.x, position.y, sprite->Z()));
                    }
                    if (options.optFlag16) ShiftActionCoordinates(sprite, Mouse->Coor() - clipboardAverage);
                    selectedSprites.Insert(sprite);
                }
            }
            else
            {
                STRING coordinates;
                coordinates.ReadFromClipboard(mainWindow);
                float x, y;
                std::sscanf(coordinates.CharPtr(), "%f,%f", &x, &y);
                SetShiftCoor(VECTOR2(x, y), 2);
            }
            break;
        case 41121:
            if (options.optTacticMode && selectedSprites.No())
            {
                if (++selectedSpriteIndex >= selectedSprites.No()) selectedSpriteIndex = 0;
                SPRITE* sprite = selectedSprites.m_data[selectedSpriteIndex];
                SetShiftCoor(VECTOR2(sprite->X(), sprite->Y()), 2);
            }
            break;
        case 41126:
        case 41127:
        case 41128:
            if (options.optTacticMode && selectedSprites.No())
            {
                const VECTOR first = selectedSprites.m_data[0]->Coor();
                for (int i = 1; i < selectedSprites.No(); ++i)
                {
                    SPRITE* sprite = selectedSprites.m_data[i];
                    VECTOR position = sprite->Coor();
                    if (command == 41126) position.x = first.x;
                    else if (command == 41127) position.y = first.y;
                    else position.z = first.z;
                    sprite->ChangeCoor(position);
                }
            }
            break;
        case 41122:
            if (options.optTacticMode && selectedSprites.No())
                for (int i = 0; i < selectedSprites.No(); ++i)
                {
                    VID* vid = selectedSprites.m_data[i]->Vid();
                    vid->SetPropHide(!vid->PropHide());
                }
            else Mouse->Vid()->SetPropHide(!Mouse->Vid()->PropHide());
            break;
        case 41118:
        case 41119:
        case 41120:
        case 41117:
            if (options.optTacticMode && ((command != 41120 && command != 41117) || selectedSprites.No()))
            {
                VECTOR2 minPoint,maxPoint;
                if (command == 41119 || command == 41117)
                {
                    minPoint=ShiftXY()+Graph->ViewMin();
                    maxPoint=ShiftXY()+Graph->ViewMax();
                }
                else
                {
                    minPoint=VECTOR2(-400,-400);
                    maxPoint=SizeXY()+VECTOR2(400,400);
                }
                unsigned int mask = static_cast<unsigned int>(spriteType) << 20;
                if (command == 41120 || command == 41117)
                    mask += selectedSprites.m_data[0]->Vid()->nVid;
                ::Sprites->FindSpritesInsideBox(BitsToSigned32(mask),minPoint,maxPoint,&selectedSprites);
            }
            break;
        case 40036: CallDialogBox(STRING("CONVERTSPRITE"), AppConvertSprite); break;
        case 41123:
            options.optGammaOff = !options.optGammaOff;
            CheckMenuItem(GetMenu(mainWindow), command, options.optGammaOff ? MF_CHECKED : MF_UNCHECKED);
            if (options.optGammaOff)
            {
                savedGamma = Graph->GetGamma();
                Graph->SetGamma(GAMMA());
            }
            else Graph->SetGamma(savedGamma);
            break;
        case 45098:
            CallDialogBox(STRING("OPTIONS"), AppOptions);
            for (int i = 0; i < 5; ++i)
            {
                if (!convertDrawingVid[i] || !m_vids[convertDrawingVid[i]]) continue;
                VID* vid = m_vids[convertDrawingVid[i]];
                RESOURCE resource;
                const STRING file = !nameDrawingVid[i].IsEmpty() ? nameDrawingVid[i] : vid->vidName;
                vid->InvalidateDeviceObjects();
                if (resource.OpenForRead(file, ResourceType::Video))
                {
                    ::Error->Log("LOAD::Can't open file %s", file.CharPtr());
                    continue;
                }
                resource.GoBegin(ResourceType::Header);
                resource.Read(&vid->type, 2);
                resource.Read(&vid->frameSpeedDefault, 2);
                resource.Read(&vid->noCadr, 2);
                resource.Read(&vid->vidSizeX, 2);
                resource.Read(&vid->vidSizeY, 2);
                vid->buildSizeX = vid->vidSizeX;
                vid->buildSizeY = vid->vidSizeY;
                vid->Load(&resource);
            }
            break;
        case 41112:
            options.optMenuMode = 1;
            editFileName = "maps\\logo.map";

        case 45087:
            if (options.optGammaOff) Graph->SetGamma(savedGamma);
            CallDialogBox(STRING("MAP_PROPERTY"), AppMapProperty);
            if (options.optGammaOff)
            {
                savedGamma = Graph->GetGamma();
                Graph->SetGamma(GAMMA());
            }
            break;
        case 40083: CallDialogBox(STRING("SELECT_VID"), AppSelectVid); break;
        case 40073:
            if (options.optTacticMode && selectedSprites.No())
            {
                if (selectedSprites.m_data[0]->IsSpriteClass(19))
                    CallDialogBox(STRING("TEXT_PROPERTY"), AppTextProperty);
                else if (spriteType & 128)
                    CallDialogBox(STRING("REGION_PROPERTY"), AppRegionProperty);
                else CallDialogBox(STRING("UNIT_PROPERTY"), AppUnitProperty);
            }
            break;
        case 40078: undo.Undo(); break;
        case 45092: undo.Redo(); break;
        case 41133: CallDialogBox(STRING("MAP_INFO"), AppMapInfo); break;
        case 41141: CallDialogBox(STRING("UNUSED_VID"), AppUnusedVid); break;
        case 40002:
            if (!options.optMenuMode)
            {
                if (options.optGammaOff) Graph->SetGamma(savedGamma);
                undo.Reset();

                const STRING backup = editFileName.BeforeLast(".") + ".bak";
                MoveFileExA(editFileName.CharPtr(), backup.CharPtr(), 9);
                if (std::strstr(editFileName.CharPtr(), ".men")) m_menu.Save(editFileName);
                else Save(editFileName);
                if (options.optGammaOff) Graph->SetGamma(GAMMA());
                break;
            }

        case 40075:
        {
            const STRING file = OpenSaveDialog(1, saveFilter);
            if (file.IsEmpty()) break;
            const STRING extension = file.AfterLast(".").ToLower();
            if (std::strcmp(extension.CharPtr(), "bmp") == 0)
            {
                m_input.ChangeCoor(VECTOR2(600, 600));
                const int width = TruncateDoubleToInt32(
                    static_cast<double>(Graph->ViewXMax()) - Graph->ViewXMin());
                const int height = TruncateDoubleToInt32(
                    static_cast<double>(Graph->ViewYMax()) - Graph->ViewYMin());
                for (int y = 0; static_cast<float>(y) < SizeXY().y; y += height)
                    for (int x = 0; static_cast<float>(x) < SizeXY().x; x += width)
                    {
                        m_shiftXY = VECTOR2(static_cast<float>(x) - Graph->ViewXMin(),
                                            static_cast<float>(y) - Graph->ViewYMin());
                        Graph->ClearScreen(COLOR(0, 0, 0), 0.0f);
                        Graph->PreTact();
                        Graph->Tact(1);
                        const STRING image = file.BeforeLast(".") + "_" + Int2Str(x / width) +
                            "_" + Int2Str(y / height) + ".bmp";
                        Graph->SaveScreenShot(image,
                            TruncateFloatToInt32OrMin(Graph->ViewXMin()), TruncateFloatToInt32OrMin(Graph->ViewYMin()),
                            TruncateFloatToInt32OrMin(Graph->ViewXMax()), TruncateFloatToInt32OrMin(Graph->ViewYMax()));
                        Graph->PostTact(1);
                    }
            }
            else if (std::strcmp(extension.CharPtr(), "men") == 0)
            {
                editFileName = m_currentMapFileName = file;
                undo.Reset();
                m_menu.Save(file);
                options.optMenuMode = 0;
            }
            else
            {
                if (options.optGammaOff) Graph->SetGamma(savedGamma);
                editFileName = m_currentMapFileName = file;
                undo.Reset();
                Save(m_currentMapFileName);
                options.optMenuMode = 0;
                if (options.optGammaOff) Graph->SetGamma(GAMMA());
            }
            DrawMapName();
            break;
        }
        case 40001:
        case 45084:
        {
            const STRING file = OpenDialog(openFilter);
            if (file.IsEmpty()) break;
            const STRING extension = file.AfterLast(".").ToLower();
            if (std::strcmp(extension.CharPtr(), "tga") == 0 ||
                std::strcmp(extension.CharPtr(), "z") == 0)
            {

            }
            else if (std::strcmp(extension.CharPtr(), "vid") == 0) LoadTerrain(file);
            else if (std::strcmp(extension.CharPtr(), "men") == 0)
            {
                options.optFlag12 = 0;
                options.optMenuMode = 0;
                Release();
                ResetGroundZ();
                editFileName = file;
                m_menu.Load(file);
                DrawMapName();
            }
            else
            {
                options.optFlag12 = 0;
                options.optMenuMode = 0;
                SendMessageA(hToolBar, TB_ENABLEBUTTON, 40002, 1);
                EnableMenuItem(GetMenu(mainWindow), 40002, MF_ENABLED);
                EnableMenuItem(GetMenu(mainWindow), 40075, MF_ENABLED);
                Load(file);
            }
            break;
        }
        case 40014:
            undo.Begin();
            FillBox(VECTOR2(0, 0), SizeXY());
            undo.End();
            break;
        case 41113:
            options.optFlag13 = !options.optFlag13;
            CheckMenuItem(GetMenu(mainWindow), command, options.optFlag13 ? MF_CHECKED : MF_UNCHECKED);
            if (!options.optFlag12)
            {
                options.optFlag12 = 1;
                SendMessageA(hToolBar, TB_ENABLEBUTTON, 40002, 0);
                EnableMenuItem(GetMenu(mainWindow), 40002, MF_GRAYED);
                EnableMenuItem(GetMenu(mainWindow), 40075, MF_GRAYED);
            }
            break;
        case 41124:

            {
                if (m_menu.No())
                {
                    int index = m_menu.No()-1;
                    SPRITE* sprite = m_menu.m_data[index];
                    while (sprite)
                    {
                        const VECTOR2 half = sprite->IsSpriteClass(19)
                            ? static_cast<STEXT*>(sprite)->TextSizeXY()/2.0f
                            : MenuSpriteHalfSize(sprite);
                        const VECTOR2 center = sprite->Coor().ViewCoor();
                        if (center-half > ShiftXY()+Graph->SizeXY() || center+half < ShiftXY())
                            delete sprite;
                        if (index>m_menu.No()) index=m_menu.No();
                        --index;
                        sprite=index>=0 ? m_menu.m_data[index] : 0;
                    }
                }
            }
            break;
        case 40070:
        {
            undo.Begin();
            int index;
            for (SPRITE* sprite = ::Sprites->FirstDrawingSprite(&index, Mouse->Vid()->layer); sprite;
                 sprite = ::Sprites->NextDrawingSprite(&index, Mouse->Vid()->layer))
                if (sprite->Vid() == Mouse->Vid()) undo.AddRemove(sprite);
            undo.End();
            break;
        }
        case 41103: ResetGroundZ(); break;
        case 40069:
            while (m_groups.First()) delete m_groups.First();
            break;
        case 45086: DeleteExtraVid(); break;
        case 40077: m_menu.DeleteAll(); break;
        case 40025:
            optShiftSnapX = optShiftSnapY = 0;
            options.optSnap = !options.optSnap;
            SendMessageA(hToolBar, TB_CHECKBUTTON, command, options.optSnap);
            break;
        case 40034: options.optChessSnap = !options.optChessSnap; break;
        case 45097: options.optAirBrush = !options.optAirBrush; break;
        case 45099: options.optRandomDir = !options.optRandomDir; break;
        case 40053: options.optDelete = !options.optDelete; break;
        case 45085: options.optGround0 = !options.optGround0; break;
        case 41107: SetControlPanel(!options.optControlPanel); break;
        case 41140:
            options.optDrawMap = !options.optDrawMap;
            CheckMenuItem(GetMenu(mainWindow), command,
                options.optDrawMap ? MF_CHECKED : MF_UNCHECKED);
            break;
        case 41105: SetShiftCoor(SizeXY() / 2.0f, 0); break;
        case 41106: m_input.ChangeCoor(Graph->SizeXY() / 2.0f); break;
        case 40062: ChangeMouseVid(Vid(Right()), spriteType); break;
        case 40061: ChangeMouseVid(Vid(Left()), spriteType); break;
        case 41129: ChangeMouseVid(Mouse->Vid(), 0x600); break;
        case 41114: ChangeMouseVid(Mouse->Vid(), 1); break;
        case 40047: ChangeMouseVid(Mouse->Vid(), 2); break;
        case 40048: ChangeMouseVid(Mouse->Vid(), 4); break;
        case 40049: ChangeMouseVid(Mouse->Vid(), 8); break;
        case 40050: ChangeMouseVid(Mouse->Vid(), 16); break;
        case 40051: ChangeMouseVid(Mouse->Vid(), 32); break;
        case 45090: ChangeMouseVid(Vid(Right()), 128); break;
        case 45091: ChangeMouseVid(Mouse->Vid(), 64); break;
        case 40032:
            options.optTacticMode = !options.optTacticMode;
            SendMessageA(hToolBar, TB_CHECKBUTTON, command, options.optTacticMode);
            for (unsigned int id : {40025u, 40034u, 40053u, 45097u, 45099u})
                SendMessageA(hToolBar, TB_ENABLEBUTTON, id, !options.optTacticMode);
            SendMessageA(hToolBar, TB_ENABLEBUTTON, 40073, !(options.optTacticMode && !selectedSprites.No()));
            SendMessageA(hToolBar, TB_ENABLEBUTTON, 40067, options.optTacticMode);
            CheckMenuItem(GetMenu(mainWindow), command,
                options.optTacticMode ? MF_CHECKED : MF_UNCHECKED);
            if (options.optTacticMode)
            {
                savedVid = Mouse->Vid();
                savedGround0 = options.optGround0;
                savedInsertZ = insertZ;
                ChangeMouseVid(Vid(1), savedVid->spriteType);
            }
            else
            {
                if (selectedSprites.No() == 1)
                {
                    SPRITE* sprite = selectedSprites.m_data[0];
                    const ANGLE direction = sprite->Direction();
                    ChangeMouseVid(sprite->Vid(), sprite->Vid()->spriteType);
                    Mouse->ChangeDirection(direction);
                }
                else ChangeMouseVid(savedVid, savedVid->spriteType);
            }
            SendMessageA(hToolBar, TB_CHECKBUTTON, 45085, options.optGround0);
            break;
        case 40067:
            if (selectedSprites.IsEqual(m_groups.First())) delete m_groups.First();
            else if (selectedSprites.No())
            {
                m_groups.DeletePointerToSprite(selectedSprites.m_data[0]);
                GROUP* group = new (std::nothrow) GROUP(&m_groups, selectedSprites.m_data[0]);
                for (int i = 1; i < selectedSprites.No(); ++i)
                {
                    m_groups.DeletePointerToSprite(selectedSprites.m_data[i]);
                    group->Insert(selectedSprites.m_data[i]);
                }
            }
            break;
        case 41135:
            DialogBoxParamA(Instance(), "CONVERT_STACK_LINE", window, AppConvertStackLine, 0);
            break;
        case 41136:
        {
            SPRITE* sprite = ::Sprites->NextActiveSprite(&invalidActionIndex);
            if (!sprite) sprite = ::Sprites->FirstActiveSprite(&invalidActionIndex);
            for (; sprite; sprite = ::Sprites->NextActiveSprite(&invalidActionIndex))
                for (int lane = 0; lane < 4; ++lane)
                {
                    const LIST<ACT>& actions = sprite->ActionStack(lane);
                    for (int i = 0; i < actions.No(); ++i)
                        if (actions.m_data[i].command == 71 && actions.m_data[i].arg1 >= actions.No())
                        {
                            selectAndCenter(sprite);
                            return 0;
                        }
                }
            sprite = ::Sprites->NextUnit(&blockedUnitIndex);
            if (!sprite) sprite = ::Sprites->FirstUnit(&blockedUnitIndex, -1);
            for (; sprite; sprite = ::Sprites->NextUnit(&blockedUnitIndex))
                if (sprite->CanPlace(sprite->Coor()))
                {
                    selectAndCenter(sprite);
                    return 0;
                }
            break;
        }
        case 41131:
        {
            ACT* pattern = EditorStackSearchAction();
            if (stackSearchIndex < 0) pattern->arg1 = pattern->arg2 = pattern->arg3 = -999999;
            if (!DialogBoxParamA(Instance(), "EDIT_STACK_LINE", window, AppEditStackLine, 0)) break;
            SPRITE* sprite = ::Sprites->NextActiveSprite(&stackSearchIndex);
            if (!sprite) sprite = ::Sprites->FirstActiveSprite(&stackSearchIndex);
            for (; sprite; sprite = ::Sprites->NextActiveSprite(&stackSearchIndex))
                for (int lane = 0; lane < 4; ++lane)
                    if (sprite->HaveAction(lane, *pattern))
                    {
                        selectAndCenter(sprite);
                        return 0;
                    }
            break;
        }
        case 41137:
        {
            if (!DialogBoxParamA(Instance(), "FIND_SPRITE", window, AppFindSprite, 0)) break;
            SPRITE* sprite = ::Sprites->NextActiveSprite(&nameSearchIndex);
            if (!sprite) sprite = ::Sprites->FirstActiveSprite(&nameSearchIndex);
            for (; sprite; sprite = ::Sprites->NextActiveSprite(&nameSearchIndex))
                for (int lane = 0; lane < 4; ++lane)
                    if (std::strstr(sprite->GetName().CharPtr(), EditorSpriteSearchText().CharPtr()))
                    {
                        selectAndCenter(sprite);
                        return 0;
                    }
            break;
        }
        case 40082:
        {
            SPRITE* sprite = ::Sprites->NextActiveSprite(&vidSearchIndex);
            if (!sprite) sprite = ::Sprites->FirstActiveSprite(&vidSearchIndex);
            for (; sprite; sprite = ::Sprites->NextActiveSprite(&vidSearchIndex))
                if (sprite->Vid() == Mouse->Vid())
                {
                    SetShiftCoor(VECTOR2(sprite->X(), sprite->Y()), 2);
                    break;
                }
            break;
        }
        case 40004: PostMessageA(window, WM_CLOSE, 0, 0); break;
        default: break;
        }
        return 0;
    }

    if (message == WM_NCHITTEST)
    {
        RECT rectangle;
        GetWindowRect(mainWindow, &rectangle);
        const VECTOR2 cursor(static_cast<int>((lParam & 0xFFFFu) - rectangle.left),
            static_cast<int>(((lParam >> 16) & 0xFFFFu) - rectangle.top));
        if (Graph->ViewXMax() < cursor.x && Graph->ViewYMin() < cursor.y)
        {
            m_input.m_clientMouseXY = cursor;
            EnableWindow(hControlPanel, TRUE);
        }
        else if (!EnableWindow(hControlPanel, FALSE)) SetFocus(mainWindow);
    }
    else if (message == WM_KEYDOWN && options.optTacticMode && m_input.GetAction(33))
    {
        if (wParam == 219 || wParam == 221)
        {
            for (int i = 0; i < selectedSprites.No(); ++i)
            {
                const int stepY = m_input.GetAction(34) ? optBigStepZ : 1;
                const int stepZ = m_input.GetAction(34) ? optBigStepZ : 1;
                SPRITE* sprite = selectedSprites.m_data[i];
                const VECTOR delta(0, stepY, stepZ);
                sprite->ChangeCoor(wParam == 219 ? sprite->Coor() + delta : sprite->Coor() - delta);
            }
        }
    }
    return MAP::WorkWndMessage(window, message, wParam, lParam);
}
