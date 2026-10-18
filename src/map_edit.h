#pragma once

#include "map.h"
#include "undo.h"
#include "dialog_controls.h"

class REGION;

template<> LIST<STRING>::LIST();
template<> LIST<STRING>::~LIST();
template<> void LIST<STRING>::Insert(STRING value);
template<> void LIST<STRING>::ExpandForInsert();
template<> void LIST<STRING>::Expand(int capacity);
template<> void LIST<STRING>::Release();
template<> int LIST<STRING>::No() const;
template<> STRING* LIST<STRING>::operator[](int index);
template<> const STRING* LIST<STRING>::operator[](int index) const;

class MAP_EDIT : public MAP
{
public:
    MAP_EDIT(HINSTANCE instance, HINSTANCE previous, const STRING& command, int show, GRAPH_INIT* init);
    ~MAP_EDIT() noexcept(false) override;
    int WorkWndMessage(HWND hwnd, unsigned long message, unsigned long wParam, unsigned long lParam) override;
    void DeletePointerToSprite(SPRITE* sprite) override;
    int Tact() override;
    void DrawSecondaryInfo() override;
    void Release() override;
    void Load(STRING name) override;
    SPRITE* CreateSprite(VID* vid, const VECTOR& position, ANGLE direction, SPRITE* parent) override;

    void DrawMapName();
    void ChangeMouseVid(VID* vid, unsigned int type);
    void ChangeMouseCoorWithSnap();
    void Control(INPUT* input);
    void ShiftActionCoordinates(SPRITE* sprite, const VECTOR& offset);
    SPRITE* InsertUnit(VID* vid, VECTOR position, ANGLE direction);
    void DeleteUnit(const VECTOR2& position, int spriteType);
    void FillBox(VECTOR2 begin, VECTOR2 end);
    void FillRomb(const VECTOR2& begin, const VECTOR2& end);
    void LoadTerrain(STRING filename);
    void CreateNewRail();
    void SetControlPanel(int flag);
    int Left();
    int Right();
    int IsLeftPossible();
    int IsRightPossible();
    int CallDialogBox(const STRING& name, DLGPROC procedure);
    void SetRememberedVid(unsigned int type, VID* vid);
    VID* GetRememberedVid(unsigned int type) const;

    HINSTANCE Instance() const { return static_cast<HINSTANCE>(m_hInstance); }

    int DialogControlPanel(HWND, UINT, WPARAM, LPARAM);
    int DialogSelectVid(HWND, UINT, WPARAM, LPARAM);
    int DialogMapProperty(HWND, UINT, WPARAM, LPARAM);
    int DialogConvertSprite(HWND, UINT, WPARAM, LPARAM);
    int DialogUnitProperty(HWND, UINT, WPARAM, LPARAM);
    int DialogTextProperty(HWND, UINT, WPARAM, LPARAM);
    int DialogRegionProperty(HWND, UINT, WPARAM, LPARAM);
    int DialogOptions(HWND, UINT, WPARAM, LPARAM);

    union {
        unsigned int optionBits;
        struct {
            unsigned int optDrawMap : 1;
            unsigned int optSortVid : 1;
            unsigned int optDrawGroup : 1;
            unsigned int optDelete : 1;
            unsigned int optGround0 : 1;
            unsigned int optSnap : 1;
            unsigned int optChessSnap : 1;
            unsigned int optAirBrush : 1;
            unsigned int optRandomDir : 1;
            unsigned int optTacticMode : 1;
            unsigned int optControlPanel : 1;
            unsigned int optMenuMode : 1;
            unsigned int optFlag12 : 1;
            unsigned int optFlag13 : 1;
            unsigned int optDrawName : 1;
            unsigned int optGammaOff : 1;
            unsigned int optFlag16 : 1;
            unsigned int optionUnused : 15;
        } options;
    };
    int optGridLineX;
    int optGridLineY;
    int optAirBrushSize;
    unsigned int optAirBrushDensity;
    int optBigStepZ;
    unsigned int optAutoSavePeriod;
    int optShiftSnapX;
    int optShiftSnapY;
    unsigned long lastSaveTime;
    SPRITE_LIST selectedSprites;
    int spriteType;
    VID* rememberedVid[16];
    float insertZ;
    UNDO undo;
    REGION* curRegion;
    HWND hToolBar;
    HWND hControlPanel;
    STRING editFileName;
    VID* savedVid;
    int savedGround0;
    float savedInsertZ;
    LIST<STRING> weaponNames;
    HMODULE questEditorModule;
    GAMMA savedGamma;
    int convertDrawingVid[5];
    STRING nameDrawingVid[5];
};
