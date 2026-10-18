#include "map_edit.h"
#include "graph.h"
#include "vid.h"
#include "registry.h"
#include "crc32.h"
#include "engine_globals.h"
#include <commctrl.h>
#include <cstdio>
#include <cstring>

MAP_EDIT::MAP_EDIT(HINSTANCE instance, HINSTANCE previous, const STRING& command, int show, GRAPH_INIT* init)
    : MAP(instance, previous, command, show, init)
{
    if (std::strstr(command.CharPtr(), "cr_crc"))
    {
        CRC32 resourceCheck;
        resourceCheck.CheckListFromFile(STRING("maps\\check_list.lst"));
        ::Error->Log("Check right resource = %u", resourceCheck.value);
        CRC32 stringsCheck;
        stringsCheck.CheckListFromFile(STRING("maps\\check_list2.lst"));
        ::Error->Log("Check (check_list2.lst) strings.ini = %u", stringsCheck.value);
        m_flags &= ~FLAG_INITIALIZED;
        return;
    }
    if (!IsInitSuccess())
        return;

    insertZ = 0.0f;
    spriteType = 4;
    hToolBar = 0;
    hControlPanel = 0;
    options.optDrawMap = 0;
    options.optDrawGroup = 0;
    options.optTacticMode = 0;
    options.optMenuMode = 0;
    options.optFlag13 = 0;
    options.optFlag12 = 0;
    options.optSnap = 0;
    options.optChessSnap = 0;
    m_flags |= FLAG_EDITOR;
    options.optRandomDir = 0;
    options.optDelete = 0;
    options.optSortVid = 0;
    options.optAirBrush = 0;
    optAirBrushSize = 10;
    optAirBrushDensity = 20;
    optShiftSnapX = 0;
    optShiftSnapY = 0;
    lastSaveTime = 0;
    options.optGammaOff = Registry->GetInt(STRING("GammaOff"), 0);
    CheckMenuItem(GetMenu(reinterpret_cast<HWND>(m_dialog)), 0xA0A3,
                  options.optGammaOff ? MF_CHECKED : MF_UNCHECKED);
    options.optDrawName = Registry->GetInt(STRING("DrawName"), 0);
    options.optDrawMap = Registry->GetInt(STRING("DrawMap"), options.optDrawMap);
    CheckMenuItem(GetMenu(reinterpret_cast<HWND>(m_dialog)), 41140,
                  options.optDrawMap ? MF_CHECKED : MF_UNCHECKED);
    optBigStepZ = Registry->GetInt(STRING("BigStepZ"), 90);
    options.optGround0 = Registry->GetInt(STRING("Ground0"), 1);
    optGridLineX = Registry->GetInt(STRING("GridLineX"), 0);
    optGridLineY = Registry->GetInt(STRING("GridLineY"), 0);
    optAutoSavePeriod = Registry->GetInt(STRING("AutoSavePeriod"), 60000);
    options.optFlag16 = 0;
    for (int index = 0; index < 16; ++index)
        rememberedVid[index] = 0;

    for (int index = 0; index < 5; ++index)
    {
        const STRING suffix = Int2Str(index);
        convertDrawingVid[index] = Registry->GetInt("convertDrawingVid" + suffix, 0);
        nameDrawingVid[index] = Registry->GetString("nameDrawingVid" + suffix, STRING(""));
        const int number = convertDrawingVid[index];
        if (!number || !ValidateVid(number) || nameDrawingVid[index].IsEmpty())
            continue;
        VID* const vid = m_vids[number];
        RESOURCE resource;
        if (resource.OpenForRead(nameDrawingVid[index], ResourceType::Video))
        {
            ::Error->Log("LOAD::Can't open file %s", nameDrawingVid[index].CharPtr());
            continue;
        }
        resource.GoBegin(ResourceType::Header);
        resource.Read(&vid->type, 2);
        resource.Read(&vid->frameSpeedDefault, 2);
        resource.Read(&vid->noCadr, 2);
        resource.Read(&vid->vidSizeX, 2);
        resource.Read(&vid->vidSizeY, 2);
        vid->buildSizeX = static_cast<float>(vid->vidSizeX);
        vid->buildSizeY = static_cast<float>(vid->vidSizeY);
        vid->Load(&resource);
    }

    options.optControlPanel = 0;
    curRegion = 0;
    savedVid = 0;
    if (optBigStepZ > 10000 || optBigStepZ < -10000)
        optBigStepZ = 90;

    TBBUTTON buttons[35] = {
        {0,45087,4,0,{0,0},0,-1}, {1,40001,4,0,{0,0},0,-1},
        {2,40002,4,0,{0,0},0,-1}, {0,0,4,1,{0,0},0,-1},
        {3,40078,4,0,{0,0},0,-1}, {4,45092,4,0,{0,0},0,-1},
        {0,0,4,1,{0,0},0,-1}, {5,40025,4,2,{0,0},0,-1},
        {6,40034,4,2,{0,0},0,-1}, {7,40053,4,2,{0,0},0,-1},
        {8,45085,static_cast<BYTE>(4u|options.optGround0),2,{0,0},0,-1},
        {9,45097,4,2,{0,0},0,-1}, {10,45099,4,2,{0,0},0,-1},
        {0,0,4,1,{0,0},0,-1}, {11,41129,4,6,{0,0},0,-1},
        {12,41114,4,6,{0,0},0,-1}, {13,40047,4,6,{0,0},0,-1},
        {14,40048,5,6,{0,0},0,-1}, {15,40049,4,6,{0,0},0,-1},
        {16,40050,4,6,{0,0},0,-1}, {17,45091,4,6,{0,0},0,-1},
        {18,40051,4,6,{0,0},0,-1}, {19,45090,4,6,{0,0},0,-1},
        {0,0,4,1,{0,0},0,-1}, {20,40032,4,2,{0,0},0,-1},
        {0,0,4,1,{0,0},0,-1}, {21,40067,4,2,{0,0},0,-1},
        {0,0,4,1,{0,0},0,-1}, {22,40073,4,0,{0,0},0,-1},
        {0,0,4,1,{0,0},0,-1}, {23,40061,4,0,{0,0},0,-1},
        {24,40062,4,0,{0,0},0,-1}, {0,0,4,1,{0,0},0,-1},
        {25,45098,4,0,{0,0},0,-1}, {26,41107,4,2,{0,0},0,-1}
    };
    hToolBar = CreateToolbarEx(reinterpret_cast<HWND>(m_dialog), 0x50800100u,
        0x73, 27, instance, 0x73, buttons, 35, 0, 0, 0, 0, sizeof(TBBUTTON));
    RECT toolbarRect;
    GetWindowRect(hToolBar, &toolbarRect);
    Graph->SetViewPort(VECTOR2(Graph->viewLeft,
        Graph->viewTop + static_cast<float>(toolbarRect.bottom-toolbarRect.top)),
        VECTOR2(Graph->viewRight, Graph->viewBottom));
    SetControlPanel(Registry->GetInt(STRING("ControlPanel"), 0));
    Mouse->HardwareOff();

    if (std::strcmp(m_pendingMapFileName.CharPtr(), "maps\\logo.map") == 0)
    {
        m_pendingMapFileName = "";
        options.optMenuMode = 1;
    }
    Load(m_pendingMapFileName);

    const int noHide = Registry->GetInt(STRING("NoHideVids"), 0);
    if (noHide)
    {
        unsigned int hide[VidSlotCount];
        Registry->GetData(STRING("HideVids"), hide, noHide * sizeof(hide[0]));
        for (int index = 0; index < noHide; ++index)
            if (m_vids[hide[index]])
                m_vids[hide[index]]->SetPropHide(1);
    }

    std::FILE* const weapons = FOpen(STRING("weapon.cfg"), STRING("rt"));
    if (weapons)
    {
        char buffer[1024];
        const char* const prefixes[] = {"Weapon=", "Ammo=", "Armor=", "Implant=", "Equip=", "Objects="};
        while (std::fgets(buffer, sizeof(buffer), weapons))
        {
            STRING line = STRING(buffer).Before(";");
            line.RemoveEndChars(" \n\r\t");
            bool accepted = false;
            for (const char* prefix : prefixes)
            {
                if (std::strncmp(line.CharPtr(), prefix, std::strlen(prefix)) == 0)
                {
                    line = line.After(prefix);
                    accepted = true;
                    break;
                }
            }
            if (accepted && !line.IsEmpty())
                weaponNames.Insert(line);
        }
        std::fclose(weapons);
    }
    questEditorModule = LoadLibraryA("QuestEditor.dll");
    if (!questEditorModule)
        Error(E_LOAD, STRING("QuestEditor.dll"), 0);
    ChangeMouseVid(Vid(1), spriteType);
}

MAP_EDIT::~MAP_EDIT() noexcept(false)
{
    if (questEditorModule)
    {
        typedef void (__cdecl *DestroyQuestEditor)();
        DestroyQuestEditor destroy = reinterpret_cast<DestroyQuestEditor>(
            GetProcAddress(questEditorModule, "Delete_zQuestEditorMainWnd"));
        if (destroy)
            destroy();
        FreeLibrary(questEditorModule);
    }
    if (Mouse)
        Mouse->HardwareOn();
    if (!IsInitSuccess())
        return;

    Registry->SetInt(STRING("GraphDetail"), Graph->graphDetail);
    Registry->SetInt(STRING("GammaOff"), options.optGammaOff);
    Registry->SetInt(STRING("DrawName"), options.optDrawName);
    Registry->SetInt(STRING("DrawMap"), options.optDrawMap);
    Registry->SetInt(STRING("Ground0"), options.optGround0);
    Registry->SetInt(STRING("ControlPanel"), options.optControlPanel);
    Registry->SetInt(STRING("GridLineX"), optGridLineX);
    Registry->SetInt(STRING("GridLineY"), optGridLineY);

    unsigned int hide[VidSlotCount];
    unsigned int noHide = 0;
    for (int number = 0; number < m_vidCount; ++number)
        if (m_vids[number] && m_vids[number]->PropHide())
            hide[noHide++] = number;
    Registry->SetInt(STRING("NoHideVids"), noHide);
    Registry->SetData(STRING("HideVids"), hide, noHide * sizeof(hide[0]));
    for (int index = 0; index < 5; ++index)
    {
        const STRING suffix = Int2Str(index);
        Registry->SetInt("convertDrawingVid" + suffix, convertDrawingVid[index]);
        Registry->SetString("nameDrawingVid" + suffix, nameDrawingVid[index]);
    }
}
