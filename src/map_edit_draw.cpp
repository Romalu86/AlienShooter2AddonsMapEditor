#include "map_edit.h"
#include "graph.h"
#include "region.h"
#include "sprite_collector.h"
#include "engine_globals.h"

void MAP_EDIT::DrawSecondaryInfo()
{
    if (options.optDrawName) ::Sprites->DrawNamed();
    MAP::DrawSecondaryInfo();
    Graph->PrintfXY(200.0f,Graph->ViewYMin(),"%-8i %-8i",
        TruncateFloatToInt32OrMin(m_input.MouseXY().x),TruncateFloatToInt32OrMin(m_input.MouseXY().y));
    Graph->PrintfXY(300.0f,Graph->ViewYMin(),"%6i+%i",TruncateFloatToInt32OrMin(insertZ),
        TruncateFloatToInt32OrMin(GetGroundZ(Mouse->Vid(),m_input.MouseXY(),Mouse->Direction())));
    Graph->PrintfXY(400.0f,Graph->ViewYMin(),"%i",Graph->RealZBuffer(m_input.ClientMouseXY()));
    if (spriteType==128)
    {
        int index;
        for (SPRITE* sprite=::Sprites->FirstDrawingSprite(&index,5);sprite;
             sprite=::Sprites->NextDrawingSprite(&index,5))
            if (sprite->Vid()->spriteClass==23) sprite->DrawSecondaryInfo();
        if (curRegion)
            Graph->Box(curRegion->ScreenLeftTop()-VECTOR2(1,1),curRegion->ScreenRightBottom()+VECTOR2(1,1),GRAPH::RED);
    }
    Graph->Box(ToScreen(VECTOR2(-1,-1)),ToScreen(SizeXY()),COLOR(255,255,255));
    if (options.optTacticMode)
        for (int index=0;index<selectedSprites.No();++index)
        {
            SPRITE* sprite=*selectedSprites[index];
            sprite->DrawRectangle();
            for (int lane=0;lane<4;++lane) sprite->DrawStack(lane);
            if (options.optDrawName)
                Graph->PutsXY(sprite->ScreenXY()+VECTOR2(0,-10),sprite->Vid()->name,COLOR(255,255,255));
        }
}
