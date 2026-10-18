#include "map_edit.h"
#include "editor_dialogs.h"
#include "region.h"
#include "registry.h"
#include "graph.h"
#include "sprite_collector.h"
#include "engine_globals.h"
#include <cstring>

namespace {
unsigned int regionSavedProperty;
int optionsShowHidden;
void AddEditorTabs(DIALOG_TABS& tabs)
{
    const char* names[]={"Dummy","Terrain","Object","Unit","Avia","Menu","Railway","Region"};
    for (int i=0;i<8;++i) { STRING text(names[i]); tabs.AddItemWithData(&text,1<<i); }
    STRING other("Other"); tabs.AddItemWithData(&other,0x600);
}
}

INT_PTR CALLBACK AppSelectVid(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{ return static_cast<MAP_EDIT*>(Map)->DialogSelectVid(hwnd,msg,wParam,lParam); }
int MAP_EDIT::DialogSelectVid(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
    DIALOG_LIST_BOX vids(hwnd,1013);
    DIALOG_BUTTON sort(hwnd,2003);
    DIALOG_TABS tabs(hwnd,1101);

    if (tabs.IsSelChange(msg,wParam,lParam)) {
        ChangeMouseVid(Mouse->Vid(),static_cast<unsigned int>(tabs.GetCurrentItemData()));
        VidToListBox(&vids,static_cast<unsigned long>(spriteType),Mouse->Vid()->nVid,options.optSortVid);
    } else if (vids.IsDoubleClicked(msg,wParam)) {
        ChangeMouseVid(Vid(vids.GetCurrentStringData()),static_cast<unsigned int>(spriteType));
        EndDialog(hwnd,1);
    } else if (sort.IsClicked(msg,wParam)) {
        options.optSortVid=!options.optSortVid;
        VidToListBox(&vids,static_cast<unsigned long>(spriteType),Mouse->Vid()->nVid,options.optSortVid);
    }

    if (msg==272u) {
        AddEditorTabs(tabs);
        tabs.SetMinWidth(50);
        tabs.SetCurrentWithData(spriteType);
        sort.SetCheck(options.optSortVid);
        vids.SetColumnWidth(210);
        VidToListBox(&vids,static_cast<unsigned long>(spriteType),Mouse->Vid()->nVid,options.optSortVid);
        return 1;
    }
    if (msg==273u) {
        switch (wParam & 0xFFFFu) {
        case 1:
            ChangeMouseVid(Vid(vids.GetCurrentStringData()),static_cast<unsigned int>(spriteType));
            EndDialog(hwnd,1);
            break;
        case 2:
            EndDialog(hwnd,0);
            break;
        }
    }
    return 0;
}

INT_PTR CALLBACK AppRegionProperty(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{ return static_cast<MAP_EDIT*>(Map)->DialogRegionProperty(hwnd,msg,wParam,lParam); }
int MAP_EDIT::DialogRegionProperty(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
    if (curRegion)
        return curRegion->DialogRegionProperty(hwnd,msg,wParam,lParam);
    EndDialog(hwnd,0);
    return 0;
}
int REGION::DialogRegionProperty(HWND hwnd,UINT msg,WPARAM wParam,LPARAM)
{
    DIALOG_COMBO_BOX texture(hwnd,1057);
    DIALOG_COMBO_BOX environment(hwnd,1058);
    DIALOG_INT x(hwnd,1007);
    DIALOG_INT y(hwnd,1009);
    DIALOG_INT z(hwnd,1011);
    DIALOG_UNSIGNED size_x(hwnd,1008);
    DIALOG_UNSIGNED size_y(hwnd,1010);
    DIALOG_INT fog_top(hwnd,1012);
    DIALOG_INT fog_bottom(hwnd,1013);
    DIALOG_UNSIGNED fog_red(hwnd,1014);
    DIALOG_UNSIGNED fog_green(hwnd,1015);
    DIALOG_UNSIGNED fog_blue(hwnd,1016);
    DIALOG_BUTTON fog_inverse(hwnd,1051);
    DIALOG_BUTTON fog_slow_born(hwnd,1055);
    DIALOG_UNSIGNED source_vid[6]={
        DIALOG_UNSIGNED(hwnd,1076),DIALOG_UNSIGNED(hwnd,1078),DIALOG_UNSIGNED(hwnd,1080),
        DIALOG_UNSIGNED(hwnd,1082),DIALOG_UNSIGNED(hwnd,1084),DIALOG_UNSIGNED(hwnd,1086)};
    DIALOG_UNSIGNED conversion_vid[6]={
        DIALOG_UNSIGNED(hwnd,1077),DIALOG_UNSIGNED(hwnd,1079),DIALOG_UNSIGNED(hwnd,1081),
        DIALOG_UNSIGNED(hwnd,1083),DIALOG_UNSIGNED(hwnd,1085),DIALOG_UNSIGNED(hwnd,1087)};

    if (msg==3u) {
        UpdateScreenForDlg(hwnd);
        return 0;
    }
    if (msg==272u) {
        regionSavedProperty=m_fogFlags;
        Map->VidToControlBox(&texture,0x80u,Vid()->nVid);
        Map->VidToControlBox(&environment,0x80u,m_regionVid->nVid);
        x=TruncateFloatToInt32(X());
        y=TruncateFloatToInt32(Y());
        z=TruncateFloatToInt32(Z());
        size_x=static_cast<unsigned int>(TruncateFloatToInt32(m_halfWidth));
        size_y=static_cast<unsigned int>(TruncateFloatToInt32(m_halfHeight));
        fog_top=m_fogEnd;
        fog_bottom=m_fogStart;
        fog_red=((m_fogColor.value>>16)&255u);
        fog_green=((m_fogColor.value>>8)&255u);
        fog_blue=(m_fogColor.value&255u);
        fog_inverse.SetCheck((m_fogFlags&2u)!=0u);
        fog_slow_born.SetCheck((m_fogFlags&1u)!=0u);
        for (int i=0;i<6;++i) {
            source_vid[i]=m_sourceVid[i] ? static_cast<unsigned int>(m_sourceVid[i]->nVid) : 0u;
            conversion_vid[i]=m_targetVid[i] ? static_cast<unsigned int>(m_targetVid[i]->nVid) : 0u;
        }
        SendDlgItemMessageA(hwnd,2001,241u,m_fogFlags&8u,0);
        SendDlgItemMessageA(hwnd,2002,241u,m_fogFlags&4u,0);
        SendDlgItemMessageA(hwnd,2003,241u,m_fogFlags&0x10u,0);
        const int enabled=(m_fogFlags&8u) ? 0 : 1;
        x.Enable(enabled); y.Enable(enabled); z.Enable(enabled); size_x.Enable(enabled); size_y.Enable(enabled);
        return 1;
    }
    if (msg!=273u)
        return 0;

    switch (wParam&0xFFFFu) {
    case 1: {
        Action(62,texture.GetCurrentStringData(),0,0);
        m_regionVid=Map->Vid(environment.GetCurrentStringData());
        const COLOR fog(static_cast<int>(static_cast<unsigned int>(fog_red)),
            static_cast<int>(static_cast<unsigned int>(fog_green)),
            static_cast<int>(static_cast<unsigned int>(fog_blue)));
        SetFogParameters(static_cast<int>(fog_bottom),static_cast<int>(fog_top),fog);
        ChangeCoor(VECTOR(static_cast<float>(static_cast<int>(x)),
                   static_cast<float>(static_cast<int>(y)),
                   static_cast<float>(static_cast<int>(z))));
        SetSize(static_cast<float>(static_cast<unsigned int>(size_x)),
                static_cast<float>(static_cast<unsigned int>(size_y)));
        for (int i=0;i<6;++i) {
            m_sourceVid[i]=Map->Vid(static_cast<int>(static_cast<unsigned int>(source_vid[i])));
            if (m_sourceVid[i]->nVid<=0) m_sourceVid[i]=0;
            m_targetVid[i]=Map->Vid(static_cast<int>(static_cast<unsigned int>(conversion_vid[i])));
            if (m_targetVid[i]->nVid<=0) m_targetVid[i]=0;
        }
        EndDialog(hwnd,1);
        break;
    }
    case 2:
        m_fogFlags=regionSavedProperty;
        EndDialog(hwnd,0);
        break;
    case 1051: m_fogFlags^=2u; break;
    case 1055: m_fogFlags^=1u; break;
    case 2001: {
        m_fogFlags^=8u;
        const int enabled=(m_fogFlags&8u) ? 0 : 1;
        x.Enable(enabled); y.Enable(enabled); z.Enable(enabled); size_x.Enable(enabled); size_y.Enable(enabled);
        break;
    }
    case 2002: m_fogFlags^=4u; break;
    case 2003: m_fogFlags^=0x10u; break;
    default: break;
    }
    return 0;
}

INT_PTR CALLBACK AppOptions(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{ return static_cast<MAP_EDIT*>(Map)->DialogOptions(hwnd,msg,wParam,lParam); }
int MAP_EDIT::DialogOptions(HWND hwnd,UINT msg,WPARAM wParam,LPARAM)
{
    DIALOG_UNSIGNED airBrushSize(hwnd,1007);
    DIALOG_UNSIGNED airBrushDensity(hwnd,1008);
    DIALOG_UNSIGNED bigStepZ(hwnd,1009);
    DIALOG_LIST_BOX vids(hwnd,1013);
    DIALOG_BUTTON showHidden(hwnd,2001), drawName(hwnd,2002), flag16(hwnd,2003);
    DIALOG_UNSIGNED autosave(hwnd,1010), gridX(hwnd,1023), gridY(hwnd,1026);
    DIALOG_UNSIGNED conversion[5]={DIALOG_UNSIGNED(hwnd,1011),DIALOG_UNSIGNED(hwnd,1014),
        DIALOG_UNSIGNED(hwnd,1016),DIALOG_UNSIGNED(hwnd,1018),DIALOG_UNSIGNED(hwnd,1020)};
    DIALOG_TEXT names[5]={DIALOG_TEXT(hwnd,1012),DIALOG_TEXT(hwnd,1015),
        DIALOG_TEXT(hwnd,1017),DIALOG_TEXT(hwnd,1019),DIALOG_TEXT(hwnd,1021)};

    if (msg==272u) {
        airBrushSize=static_cast<unsigned int>(optAirBrushSize);
        airBrushDensity=optAirBrushDensity;
        bigStepZ=optBigStepZ;
        autosave=optAutoSavePeriod/1000u;
        gridX=static_cast<unsigned int>(optGridLineX); gridY=static_cast<unsigned int>(optGridLineY);
        drawName.SetCheck(options.optDrawName); flag16.SetCheck(options.optFlag16);
        for (int i=0;i<5;++i) { conversion[i]=static_cast<unsigned int>(convertDrawingVid[i]); names[i]=&nameDrawingVid[i]; }
        showHidden.SetCheck(optionsShowHidden);
        VidToListBox(&vids,0xDFu,-2,optionsShowHidden);
        for (int i=vids.NoString()-1;i>=0;--i) {
            if (Vid(vids.GetData(i))->PropHide())
                vids.SetSelected(i);
        }
        return 1;
    }

    if (msg==273u) {
        const unsigned int id=wParam & 0xFFFFu;
        switch (id) {
        case 1:
            optAirBrushSize=static_cast<int>(static_cast<unsigned int>(airBrushSize));
            optAirBrushDensity=static_cast<unsigned int>(airBrushDensity);
            optBigStepZ=static_cast<int>(static_cast<unsigned int>(bigStepZ));
            optGridLineX=static_cast<int>(static_cast<unsigned int>(gridX));
            optGridLineY=static_cast<int>(static_cast<unsigned int>(gridY));
            optAutoSavePeriod=static_cast<unsigned int>(autosave)*1000u;
            options.optDrawName=drawName.GetCheck(); options.optFlag16=flag16.GetCheck();
            Registry->SetInt(STRING("BigStepZ"),optBigStepZ);
            for (int i=0;i<5;++i) { convertDrawingVid[i]=static_cast<int>(static_cast<unsigned int>(conversion[i])); nameDrawingVid[i]=static_cast<STRING>(names[i]); }
            for (int i=vids.NoString()-1;i>=0;--i)
                Vid(vids.GetData(i))->SetPropHide(vids.IsSelected(i));
            EndDialog(hwnd,1);
            break;
        case 2:
            EndDialog(hwnd,0);
            break;
        case 2001:
            optionsShowHidden^=1;
            VidToListBox(&vids,0xDFu,-2,optionsShowHidden);
            for (int i=vids.NoString()-1;i>=0;--i) {
                if (Vid(vids.GetData(i))->PropHide())
                    vids.SetSelected(i);
            }
            break;
        case 1073:
            airBrushSize=5u;
            airBrushDensity=20u;
            break;
        case 1074:
            airBrushSize=10u;
            airBrushDensity=20u;
            break;
        case 1075:
            airBrushSize=50u;
            airBrushDensity=5u;
            break;
        case 1022:
        case 1068:
        case 1066:
        case 1067:
        case 1024:
        case 1025: {
            unsigned int typeMask=0;
            switch (id) {
            case 1022: typeMask=0xDFu; break;
            case 1068: typeMask=1u; break;
            case 1066: typeMask=2u; break;
            case 1067: typeMask=4u; break;
            case 1024: typeMask=8u; break;
            default:    typeMask=16u; break;
            }
            DIALOG_BUTTON typeButton(hwnd,static_cast<int>(id));
            const int checked=typeButton.GetCheck() ? 1 : 0;
            for (int i=vids.NoString()-1;i>=0;--i) {
                VID* vid=Vid(vids.GetData(i));
                if ((vid->spriteType&typeMask)!=0u) {
                    if (checked) vids.SetSelected(i);
                    else vids.SetUnSelected(i);
                }
                vid->SetPropHide(vids.IsSelected(i));
            }
            UpdateScreenForDlg(hwnd);
            break;
        }
        default:
            break;
        }
        return 0;
    }

    if (msg==3u) {
        for (int i=vids.NoString()-1;i>=0;--i)
            Vid(vids.GetData(i))->SetPropHide(vids.IsSelected(i));
        UpdateScreenForDlg(hwnd);
    }
    return 0;
}

STRING& EditorSpriteSearchText() { static STRING value; return value; }
INT_PTR CALLBACK AppFindSprite(HWND hwnd,UINT msg,WPARAM wParam,LPARAM)
{
    DIALOG_TEXT text(hwnd,1011);
    if (msg==WM_INITDIALOG) { text=&EditorSpriteSearchText(); return TRUE; }
    if (msg==WM_COMMAND && LOWORD(wParam)==IDOK) {
        EditorSpriteSearchText()=static_cast<STRING>(text); EndDialog(hwnd,1);
    } else if (msg==WM_COMMAND && LOWORD(wParam)==IDCANCEL) EndDialog(hwnd,0);
    return FALSE;
}

INT_PTR CALLBACK AppAbout(HWND hwnd,UINT msg,WPARAM wParam,LPARAM)
{
    if (msg==WM_INITDIALOG) return TRUE;
    if (msg==WM_COMMAND && LOWORD(wParam)==IDOK) EndDialog(hwnd,1);
    return FALSE;
}

namespace {
int controlPresetVid[12];
int controlSavedVid=-1;
int controlRecursion;
}

INT_PTR CALLBACK AppControlPanel(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{ return static_cast<MAP_EDIT*>(Map)->DialogControlPanel(hwnd,msg,wParam,lParam); }

int MAP_EDIT::DialogControlPanel(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
    DIALOG_TEXT coor(hwnd,1007);
    DIALOG_TREE tree(hwnd,1111);
    DIALOG_TABS tabs(hwnd,1104);
    DIALOG_BUTTON sort(hwnd,2003);
    DIALOG_RADIO custom(hwnd,1092,1103);
    ++controlRecursion;
    auto mainWindow=[this]() { return reinterpret_cast<HWND>(static_cast<UINT_PTR>(m_dialog)); };
    auto rebuild=[&]() { VidToTree(&tree,spriteType,options.optTacticMode?-1:Mouse->Vid()->nVid,options.optSortVid); };
    auto checkPreset=[&]() {
        int i=0; for (;i<12;++i) if (controlPresetVid[i]==Mouse->Vid()->nVid) break;
        custom.Check(i==12?-1:i);
    };
    int index=custom.GetIndexClicked(msg,wParam);
    if (index>=0) {
        controlSavedVid=Mouse->Vid()->nVid;
        if (controlPresetVid[index]>=0) {
            if (options.optTacticMode) SendMessageA(mainWindow(),WM_COMMAND,40032,0);
            ChangeMouseVid(Vid(controlPresetVid[index]),Vid(controlPresetVid[index])->spriteType);
        }
    } else if ((index=custom.GetIndexDblClicked(msg,wParam))>=0) {
        if (controlSavedVid<0) controlSavedVid=Mouse->Vid()->nVid;
        controlPresetVid[index]=controlSavedVid;
        const STRING label=Vid(controlSavedVid)->GetNumberName(); custom.SetItem(index,&label);
        Registry->SetInt("CustomVid"+Int2Str(index),controlPresetVid[index]);
        ChangeMouseVid(Vid(controlSavedVid),Vid(controlSavedVid)->spriteType);
        controlSavedVid=-1;
    } else if (controlRecursion==1 && tree.IsSelChange(msg,wParam,lParam) && tree.IsLeaf()) {
        controlSavedVid=tree.GetCurrentData();
        if (options.optTacticMode) SendMessageA(mainWindow(),WM_COMMAND,40032,0);
        ChangeMouseVid(Vid(controlSavedVid),Vid(controlSavedVid)->spriteType);
    } else if (sort.IsClicked(msg,wParam)) {
        options.optSortVid=!options.optSortVid; rebuild();
    } else if (tabs.IsSelChange(msg,wParam,lParam)) {
        ChangeMouseVid(Mouse->Vid(),static_cast<unsigned int>(tabs.GetCurrentItemData()));
    }
    int result=FALSE;
    switch (msg) {
    case 24576:
        tabs.SetCurrentWithData(spriteType); rebuild(); checkPreset(); break;
    case 24577:
        if (Mouse->Vid()->treeItem) tree.SetCurrent(Mouse->Vid()->treeItem);
        checkPreset(); break;
    case WM_INITDIALOG: {
        for (int i=0;i<12;++i) {
            controlPresetVid[i]=Registry->GetInt("CustomVid"+Int2Str(i),-1);
            const STRING label=ValidateVid(controlPresetVid[i])?Vid(controlPresetVid[i])->GetNumberName():STRING("unsetted");
            custom.SetItem(i,&label);
        }
        rebuild(); AddEditorTabs(tabs);
        STRING all("All"); tabs.AddItemWithData(&all,0x61F);
        tabs.SetMinWidth(42);
        tabs.SetCurrentWithData(spriteType); sort.SetCheck(options.optSortVid);
        const VECTOR2& mouse=m_input.MouseXY();
        const STRING label=Printf("%-8i %-8i %6i+%-20i %i",TruncateFloatToInt32OrMin(mouse.x),TruncateFloatToInt32OrMin(mouse.y),
            TruncateFloatToInt32OrMin(insertZ),TruncateFloatToInt32OrMin(GetGroundZ(Mouse->Vid(),mouse,Mouse->Direction())),
            Graph->RealZBuffer(m_input.ClientMouseXY()));
        coor=&label; result=TRUE; break;
    }
    case WM_NCHITTEST: {
        RECT rect; GetWindowRect(mainWindow(),&rect);
        m_input.m_clientMouseXY=VECTOR2(
            BitsToSigned32(static_cast<unsigned int>(LOWORD(lParam))-static_cast<unsigned int>(rect.left)),
            BitsToSigned32(static_cast<unsigned int>(HIWORD(lParam))-static_cast<unsigned int>(rect.top)));
        break;
    }
    default: break;
    }
    --controlRecursion;
    return result;
}

namespace {
int convertSourceVid=5,convertTargetVid=5,convertKeepOld=1;
unsigned int convertFields[4];
int convertOffset[3];
}

INT_PTR CALLBACK AppConvertSprite(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{ return static_cast<MAP_EDIT*>(Map)->DialogConvertSprite(hwnd,msg,wParam,lParam); }
int MAP_EDIT::DialogConvertSprite(HWND hwnd,UINT msg,WPARAM wParam,LPARAM)
{
    DIALOG_COMBO_BOX source(hwnd,1057),target(hwnd,1058);
    DIALOG_BUTTON keepOld(hwnd,2001);
    DIALOG_UNSIGNED fields[4]={DIALOG_UNSIGNED(hwnd,1008),DIALOG_UNSIGNED(hwnd,1009),
        DIALOG_UNSIGNED(hwnd,1011),DIALOG_UNSIGNED(hwnd,1012)};
    DIALOG_INT offsets[3]={DIALOG_INT(hwnd,1007),DIALOG_INT(hwnd,1010),DIALOG_INT(hwnd,1013)};
    if (msg==WM_INITDIALOG) {
        VidToControlBox(&source,0x7E,convertSourceVid); VidToControlBox(&target,0x7E,convertTargetVid);
        keepOld.SetCheck(convertKeepOld);
        for (int i=0;i<4;++i) fields[i]=convertFields[i];
        for (int i=0;i<3;++i) offsets[i]=convertOffset[i];
        return TRUE;
    }
    if (msg!=WM_COMMAND) return FALSE;
    if (LOWORD(wParam)==IDCANCEL) { EndDialog(hwnd,0); return FALSE; }
    if (LOWORD(wParam)!=IDOK) return FALSE;
    convertSourceVid=source.GetCurrentStringData(); convertTargetVid=target.GetCurrentStringData();
    convertKeepOld=keepOld.GetCheck();
    for (int i=0;i<4;++i) convertFields[i]=fields[i];
    for (int i=0;i<3;++i) convertOffset[i]=offsets[i];
    const VECTOR delta(static_cast<float>(convertOffset[0]),static_cast<float>(convertOffset[1]),static_cast<float>(convertOffset[2]));
    int index;
    for (SPRITE* sprite=Sprites->FirstDrawingSprite(&index,Vid(convertSourceVid)->layer);sprite;
         sprite=Sprites->NextDrawingSprite(&index,Vid(convertSourceVid)->layer)) {
        if (sprite->Vid()->nVid!=convertSourceVid) continue;
        const ANGLE direction=sprite->Direction(); const VECTOR position(sprite->Coor());
        if (!convertKeepOld) delete sprite;
        CreateSprite(Vid(convertTargetVid),VECTOR(position.x+delta.x,position.y+delta.y,position.z+delta.z),direction,0);
    }
    EndDialog(hwnd,1); return FALSE;
}
