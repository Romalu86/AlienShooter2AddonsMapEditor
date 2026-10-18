#include "map_edit.h"
#include "graph.h"
#include "draw_map.h"
#include "region.h"
#include "sound.h"
#include "sprite_collector.h"
#include "engine_globals.h"
#include <commctrl.h>
#include <cstring>
#include <new>

template<> LIST<GAMMA>::LIST() : m_count(0), m_capacity(0), m_data(0) {}
template<> LIST<GAMMA>::~LIST()
{
    delete[] m_data;
    m_data=0;
    m_count=0;
}
template<> GAMMA* LIST<GAMMA>::operator[](int index) { return m_data+index; }
template<> void LIST<GAMMA>::Expand(int capacity)
{
    if (capacity<=m_capacity) return;
    GAMMA* const previous=m_data;
    m_data=new(std::nothrow) GAMMA[capacity];
    if (!m_data) ::Error->LogExit(STRING("!!!ERROR!!!::LIST: Not enough memory %i"),capacity);
    if (previous)
    {
        for (int index=0;index<m_capacity;++index) m_data[index]=previous[index];
        delete[] previous;
    }
    m_capacity=capacity;
}
template<> void LIST<GAMMA>::ExpandForInsert()
{
    if (m_count>=m_capacity) Expand(m_capacity*2+4);
}
template<> void LIST<GAMMA>::Insert(GAMMA value)
{
    ExpandForInsert();
    m_data[m_count++]=value;
}

namespace {

float movementMultiplier=1.0f;
int deleteLatch=0;
unsigned long movementBegin=0;
unsigned long movementLast=0;
}

int MAP_EDIT::Tact()
{
    if (StartTact()) return 1;
    if (!(m_flags&FLAG_ACTIVE) || Graph->IsPaused())
    {
        WaitMessage();
        return 0;
    }

    if (RealCurrentTime-lastSaveTime>30000u)
    {
        ::Error->LogTmp(STRING("AutoSave ")+GetTimeString());
        lastSaveTime=RealCurrentTime;
        if (options.optGammaOff) Graph->SetGamma(savedGamma);
        STRING directory=editFileName.BeforeLast(STRING("\\"));
        if (directory.Length()) directory+=STRING("\\");
        if (std::strstr(editFileName.CharPtr(),".men"))
            m_menu.Save(directory+"autosave.men");
        else
            Save(directory+"autosave.map");
        if (options.optGammaOff) Graph->SetGamma(GAMMA());
    }

    static VECTOR2 previousViewMouse(-1, -1);
    if (m_input.GetAction(34) && m_input.GetAction(33))
    {
        if (previousViewMouse != VECTOR2(-1, -1))
        {
            const float movement = m_input.ClientMouseXY().x - previousViewMouse.x;
            const int step = movement > 0.0f ? 1 : movement < 0.0f ? -1 : 0;
            Graph->SetCameraDirection(ANGLE(static_cast<unsigned char>(Graph->CameraDirection().value + step)));
        }
        previousViewMouse = m_input.ClientMouseXY();
    }
    else
        previousViewMouse = VECTOR2(-1, -1);

    switch (m_input.GetAction(0x48))
    {
    case '!': Graph->SetGraphDetail(1); break;
    case '@': Graph->SetGraphDetail(2); break;
    case '#': Graph->SetGraphDetail(3); break;
    case '$': Graph->SetGraphDetail(4); break;
    case '%': Graph->SetGraphDetail(5); break;
    case '^': Graph->SetGraphDetail(6); break;
    case '&': Graph->SetGraphDetail(7); break;
    case '*': Graph->SetGraphDetail(8); break;
    case '(': Graph->SetGraphDetail(9); break;
    case ')': Graph->SetGraphDetail(10); break;
    case 'G': Graph->drawFlags=(Graph->drawFlags&~0x18u)|((Graph->drawFlags+8u)&0x18u); break;
    case 'J': Graph->SetDrawShadow((Graph->deviceState&1u)^1u); break;
    case 'L': Graph->drawFlags^=1u; break;
    case 'M': Graph->drawFlags^=2u; break;
    case 'O': m_flags^=FLAG_DRAW_SPRITE_BOUNDS|FLAG_DRAW_GROUND; break;
    }

    const int draw=(m_flags&FLAG_ACTIVE)!=0;
    if (draw && Graph->PreTact()) return 0;
    ++m_simulationFrameCount;
    if (Graph->InViewPort(m_input.ClientMouseXY()) && (options.optSnap || spriteType==64) && !options.optTacticMode)
    {
        Mouse->ChangeCoor(VECTOR(m_input.MouseXY(),Mouse->Z()));
        ChangeMouseCoorWithSnap();
    }
    const float ground=options.optGround0 ? Map->GetGroundZ(Mouse->Vid(),Mouse->Coor().XY(),Mouse->Direction()) : 0.0f;
    float z=ground+insertZ;
    if (spriteType==64)
    {
        if (z>=0.0f)
        {
            const float numerator=z+Mouse->Vid()->sizeXYZ.z-1.0f;
            const float quotient=numerator/Mouse->Vid()->sizeXYZ.z;
            z=quotient*Mouse->Vid()->sizeXYZ.z;
        }
        else
        {
            const float numerator=z-Mouse->Vid()->sizeXYZ.z+1.0f;
            const float quotient=numerator/Mouse->Vid()->sizeXYZ.z;
            z=quotient*Mouse->Vid()->sizeXYZ.z;
        }
    }
    Mouse->ChangeZCoor(z);

    SPRITE* hovered=0;
    GAMMA hoveredGamma;
    LIST<GAMMA> selectedGamma;
    if (options.optTacticMode)
    {
        SPRITE* first=selectedSprites.No() ? *selectedSprites[0] : 0;
        hovered=::Sprites->GetSpriteScr(spriteType<<20,m_input.MouseXY(),first);
        if (hovered)
        {
            hoveredGamma=hovered->Gamma();
            hovered->SetGamma(GAMMA(-128,64,-128));
        }
        for (int index=0;index<selectedSprites.No();++index)
        {
            SPRITE* sprite=*selectedSprites[index];
            selectedGamma.Insert(sprite->Gamma());
            sprite->SetGamma(GAMMA(64,-128,-128));
        }
    }
    Graph->Tact(draw);
    if (options.optTacticMode)
    {
        for (int index=0;index<selectedSprites.No();++index)
            (*selectedSprites[index])->SetGamma(*selectedGamma[index]);
        if (hovered) hovered->SetGamma(hoveredGamma);
    }

    int cursor;
    for (SPRITE* sprite=::Sprites->FirstActiveSprite(&cursor);sprite;
         sprite=::Sprites->NextActiveSprite(&cursor))
    {
        if (options.optFlag13) sprite->Tact();
        else sprite->PrimitiveTact();
    }
    if (draw)
    {
        if (optGridLineX && optGridLineY)
        {
            for (float y=0.0f;y<SizeXY().y;y+=static_cast<float>(static_cast<unsigned int>(optGridLineY)))
                if (y>=ShiftXY().y && y<ShiftXY().y+Graph->SizeY())
                    Graph->Line(0.0f,ToScreenY(y),Graph->SizeX(),ToScreenY(y),GRAPH::GRAY);
            for (float x=0.0f;x<SizeXY().x;x+=static_cast<float>(static_cast<unsigned int>(optGridLineX)))
                if (x>=ShiftXY().x && x<ShiftXY().x+Graph->SizeX())
                    Graph->Line(ToScreenX(x),0.0f,ToScreenX(x),Graph->SizeY(),GRAPH::GRAY);
        }
        DrawSecondaryInfo();
    }
    if (options.optDrawMap)
    {
        DRAW_MAP map(Graph->ViewXMax()-120.0f,Graph->ViewYMax()-80.0f,120.0f,80.0f);
        map.Draw();
        map.Control(&m_input);
        for (int dy=-17;dy<=17;++dy)
            for (int dx=-17;dx<=17;++dx)
            {
                const int x=TruncateFloatToInt32OrMin(m_input.MouseXY().x)/8+dx;
                const int y=TruncateFloatToInt32OrMin(m_input.MouseXY().y)/8+dy;
                if (x<0 || y<0 || x>=m_groundWidth || y>=m_groundHeight) continue;
                const int offset=y*m_groundWidth+x;
                const int primary=m_groundPrimary[offset],secondary=m_groundSecondary[offset];
                const int height=(primary>secondary ? primary : secondary)/2;
                Graph->PutBigPixel(Graph->ViewMax()-VECTOR2(160,40)+VECTOR2(dx,dy)*2.0f,COLOR(height,height,height));
            }
    }
    Control(&m_input);

    if (!options.optTacticMode || !selectedSprites.No())
    {
        const float step=m_input.GetAction(0x22) ? static_cast<float>(optBigStepZ) : spriteType==64 ? Mouse->Vid()->sizeXYZ.z : 1.0f;
        if (m_input.GetAction(0x48)=='[' || m_input.GetAction(0x48)=='{') insertZ-=step;
        else if (m_input.GetAction(0x48)=='}' || m_input.GetAction(0x48)==']') insertZ+=step;
        else if (m_input.GetAction(0x48)>='0' && m_input.GetAction(0x48)<='9')
            insertZ=static_cast<float>(m_input.GetAction(0x48)-'0')*10.0f;
        else if (m_input.GetAction(0x49) && (!options.optControlPanel || Graph->ViewXMax()>=m_input.ClientMouseXY().x))
            insertZ+=static_cast<float>(m_input.GetAction(0x49))*step;
    }

    if (options.optTacticMode)
    {
        if (m_input.GetAction(0x48) || m_input.GetAction(0x49))
        {
            VECTOR offset(0,0,0);
            for (int index=selectedSprites.No()-1;index>=0;--index)
            {
                switch (m_input.GetAction(0x48))
                {
                case 0x2600: if (m_input.GetAction(0x22)) offset=VECTOR(0,-1,0); break;
                case 0x2700: if (m_input.GetAction(0x22)) offset=VECTOR(1,0,0); break;
                case 0x2800: if (m_input.GetAction(0x22)) offset=VECTOR(0,1,0); break;
                case 0x2500: if (m_input.GetAction(0x22)) offset=VECTOR(-1,0,0); break;
                case '[': offset=VECTOR(0,0,-1); break;
                case ']': offset=VECTOR(0,0,1); break;
                case '{': offset=VECTOR(0,0,-optBigStepZ); break;
                case '}': offset=VECTOR(0,0,optBigStepZ); break;
                case '\'':
                {
                    offset=VECTOR(Mouse->Coor().XY()-(*selectedSprites[0])->Coor().XY(),0.0f);
                    break;
                }
                }
                if (!m_input.GetAction(0x22))
                {
                    SPRITE* sprite=*selectedSprites[index];
                    switch (m_input.GetAction(0x48))
                    {
                    case 0x2600: sprite->ChangeDirection(ANGLE(0)); break;
                    case 0x2100: sprite->ChangeDirection(ANGLE(28)); break;
                    case 0x2700: sprite->ChangeDirection(ANGLE(64)); break;
                    case 0x2200: sprite->ChangeDirection(ANGLE(100)); break;
                    case 0x2800: sprite->ChangeDirection(ANGLE(128)); break;
                    case 0x2300: sprite->ChangeDirection(ANGLE(156)); break;
                    case 0x2500: sprite->ChangeDirection(ANGLE(192)); break;
                    case 0x2400: sprite->ChangeDirection(ANGLE(228)); break;
                    case ',': case '.':
                    {
                        int direction=sprite->RealDirection();
                        if (m_input.GetAction(0x48)==',')
                        {
                            --direction;
                            if (direction<0) direction=sprite->Vid()->noDir-1;
                        }
                        else
                        {
                            ++direction;
                            if (direction>=sprite->Vid()->noDir) direction=0;
                        }
                        sprite->ChangeRealDirection(static_cast<unsigned int>(direction));
                        break;
                    }
                    }
                }
                if (m_input.GetAction(0x49))
                {
                    if (m_input.GetAction(0x21))
                    {
                        const VECTOR movement(0,m_input.GetAction(0x22) ? optBigStepZ : 1,m_input.GetAction(0x22) ? optBigStepZ : 1);
                        offset=movement*static_cast<float>(m_input.GetAction(0x49));
                    }
                    else
                    {
                        const float step=spriteType==64 ? (*selectedSprites[index])->Vid()->sizeXYZ.z :
                            static_cast<float>(m_input.GetAction(0x22) ? optBigStepZ : 1);
                        offset=VECTOR(0.0f,0.0f,static_cast<float>(m_input.GetAction(0x49))*step);
                    }
                }
                if (offset!=VECTOR(0,0,0))
                {
                    offset=offset*movementMultiplier;
                    if (!((*selectedSprites[index])->Vid()->spriteType&32u) && options.optGround0 && offset.XY()!=VECTOR2(0,0))
                    {
                        const float height=GetGroundZ((*selectedSprites[index])->Vid(),
                            (*selectedSprites[index])->Coor().XY()+offset.XY(),(*selectedSprites[index])->Direction());
                        offset.z=height-(*selectedSprites[index])->Z();
                    }
                    (*selectedSprites[index])->ChangeCoor((*selectedSprites[index])->Coor()+offset);
                    if (options.optFlag16) ShiftActionCoordinates(*selectedSprites[index],offset);
                }
            }
            if (offset!=VECTOR(0,0,0))
            {
                movementLast=RealCurrentTime;
                if (!movementBegin) movementBegin=RealCurrentTime;
                if (RealCurrentTime-movementBegin>500u && movementMultiplier<15.0f)
                    movementMultiplier*=1.02f;
            }
        }
        else if (RealCurrentTime-movementLast>80u)
        {
            movementBegin=0;
            movementMultiplier=1.0f;
        }
    }
    else
    {
        if (m_input.GetAction(0x22))
        {
            VECTOR2 offset(0,0);
            switch (m_input.GetAction(0x48))
            {
            case 0x2600: if (options.optSnap) --optShiftSnapY; else offset.y=-1.0f; break;
            case 0x2700: if (options.optSnap) ++optShiftSnapX; else offset.x=1.0f; break;
            case 0x2800: if (options.optSnap) ++optShiftSnapY; else offset.y=1.0f; break;
            case 0x2500: if (options.optSnap) --optShiftSnapX; else offset.x=-1.0f; break;
            }
            if (offset.x!=0.0f || offset.y!=0.0f)
                SetShiftCoor(FromScreen(Graph->SizeXY()/2.0f)+offset,0);
        }
        else
        {
            switch (m_input.GetAction(0x48))
            {
            case 0x2600: Mouse->ChangeDirection(ANGLE(0)); break;
            case 0x2100: Mouse->ChangeDirection(ANGLE(32)); break;
            case 0x2700: Mouse->ChangeDirection(ANGLE(64)); break;
            case 0x2200: Mouse->ChangeDirection(ANGLE(96)); break;
            case 0x2800: Mouse->ChangeDirection(ANGLE(128)); break;
            case 0x2300: Mouse->ChangeDirection(ANGLE(160)); break;
            case 0x2500: Mouse->ChangeDirection(ANGLE(192)); break;
            case 0x2400: Mouse->ChangeDirection(ANGLE(224)); break;
            case 0x1100: if (!options.optDelete) deleteLatch=1; options.optDelete=1; break;
            case 0x2E00: options.optDelete=1; break;
            case 0x2D00: options.optDelete=0; break;
            case ',': case '.':
            {
                int direction=Mouse->RealDirection();
                if (m_input.GetAction(0x48)==',')
                {
                    --direction;
                    if (direction<0) direction=Mouse->Vid()->noDir-1;
                }
                else
                {
                    ++direction;
                    if (direction>=Mouse->Vid()->noDir) direction=0;
                }
                Mouse->ChangeRealDirection(static_cast<unsigned int>(direction));
                break;
            }
            }
        }
        if (m_input.GetAction(0x49) && options.optControlPanel && Graph->ViewXMax()<m_input.ClientMouseXY().x)
        {
            const int wheel=m_input.GetAction(0x49);
            for (int count=wheel;count>0;--count) ChangeMouseVid(Vid(Left()),spriteType);
            for (int count=wheel;count<0;++count) ChangeMouseVid(Vid(Right()),spriteType);
        }
    }

    if (!m_input.GetAction(0x21) && deleteLatch) options.optDelete=0;
    ::Sound->Tact();
    SendMessageA(hToolBar,TB_ENABLEBUTTON,0x9C7D,IsLeftPossible());
    SendMessageA(hToolBar,TB_ENABLEBUTTON,0x9C7E,IsRightPossible());
    SendMessageA(hToolBar,TB_ENABLEBUTTON,0x9C8E,undo.IsUndo());
    SendMessageA(hToolBar,TB_ENABLEBUTTON,0xB024,undo.IsRedo());
    EnableMenuItem(GetMenu(reinterpret_cast<HWND>(m_dialog)), 40078,
        undo.IsUndo() ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(GetMenu(reinterpret_cast<HWND>(m_dialog)), 45092,
        undo.IsRedo() ? MF_ENABLED : MF_GRAYED);
    SendMessageA(hToolBar,TB_CHECKBUTTON,0x9C75,options.optDelete);
    if (options.optTacticMode)
    {
        EnableMenuItem(GetMenu(reinterpret_cast<HWND>(m_dialog)), 40073,
            selectedSprites.No() ? MF_ENABLED : MF_GRAYED);
        SendMessageA(hToolBar,TB_ENABLEBUTTON,0x9C89,selectedSprites.No());
        SendMessageA(hToolBar,TB_ENABLEBUTTON,0x9C83,selectedSprites.No());
        m_groups.DrawNumber();
        const int same=selectedSprites.IsEqual(m_groups.First());
        if (same) m_groups.First()->Draw();
        SendMessageA(hToolBar,TB_CHECKBUTTON,0x9C83,same);
    }
    if (draw) Graph->PostTact(1);
    return 0;
}
