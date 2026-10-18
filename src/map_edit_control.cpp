#include "map_edit.h"
#include "editor_dialogs.h"
#include "polygon.h"
#include "graph.h"
#include "region.h"
#include "player.h"
#include "sprite_collector.h"
#include "engine_globals.h"

void MAP_EDIT::Control(INPUT* input)
{
    static VECTOR dragBegin(-999,-999,0);
    static POLYGON selectionPolygon;
    static int railFinished=0;
    static unsigned long lastAirBrushTime=0;

    selectionPolygon.Draw(GRAPH::GREEN);
    const VECTOR2& point=input->MouseXY();

    if (options.optTacticMode)
    {
        SPRITE* hovered=::Sprites->GetSpriteScr(spriteType<<20,point,selectedSprites.No() ? *selectedSprites[0] : 0);
        if (!hovered || input->GetAction(0x21)) hovered=::Sprites->GetSpriteScr(spriteType<<20,point,0);
        if (selectedSprites.No() && !input->GetAction(0x21))
        {
            if (hovered && input->GetAction(0x20)) Mouse->ChangeAnimation(10);
            else if ((hovered && Map->Player((*selectedSprites[0])->Army())->RelationTo(hovered->Army())==1) || input->GetAction(0x22))
                Mouse->ChangeAnimation(5);
            else if (::Sprites->CanPlace((*selectedSprites[0])->Vid(),Mouse->Coor(),(*selectedSprites[0])->Direction()) ||
                (*selectedSprites[0])->CantMoveBulletTo(Mouse->Coor()))
                Mouse->ChangeAnimation(8);
            else if (Mouse->Animation()!=2) Mouse->ChangeAnimation(2);
            if ((Mouse->Animation()==2 || Mouse->Animation()==5) &&
                (*selectedSprites[0])->AskCycleAction(0,33,TruncateFloatToInt32OrMin(point.x),TruncateFloatToInt32OrMin(point.y),0))
                Mouse->ChangeAnimation(9);

            if ((input->GetAction(0x48)>>8)==0x2E)
            {
                undo.Begin();
                for (int index=0;index<selectedSprites.No();++index) undo.AddRemove(*selectedSprites[index]);
                undo.End();
                selectedSprites.Release();
            }
            else if (input->GetAction(0x48)=='r')
                for (int index=0;index<selectedSprites.No();++index)
                    (*selectedSprites[index])->AddAction(0,38,0,0,0);
            else if (input->GetAction(0x48)=='l')
                for (int index=0;index<selectedSprites.No();++index)
                    (*selectedSprites[index])->AddAction(0,12,0,0,0);
        }
        else Mouse->ChangeAnimation(0);

        if (input->GetAction(0x38))
        {
            selectionPolygon.Release();
            selectionPolygon.AddLinedPoint(point);
            dragBegin=VECTOR(point,Mouse->Z());
        }
        else if (input->GetAction(0x23))
        {
            if (input->GetAction(0x22)) selectionPolygon.AddLinedPoint(point);
            else selectionPolygon.CreateBox(dragBegin.XY(),point);
        }
        else if (dragBegin.x!=-999.0f)
        {
            if (!input->GetAction(0x21) && !input->GetAction(0x20)) selectedSprites.Release();
            if (selectionPolygon.NoPoint() && (input->GetAction(0x22) || AbsFloat(point.x-dragBegin.x)>10.0f || AbsFloat(point.y-dragBegin.y)>10.0f))
            {
                selectionPolygon.Closed();
                const int type=(spriteType<<20)+(input->GetAction(0x20) && selectedSprites.No() ? (*selectedSprites[0])->Vid()->nVid : 0);
                ::Sprites->FindSpritesInsideBox(type,selectionPolygon.minPoint,selectionPolygon.maxPoint,&selectedSprites);
                if (selectionPolygon.flags!=2) selectionPolygon.FilterSprites(&selectedSprites);
            }
            else if (input->GetAction(0x20) && selectedSprites.No())
            {
                if (hovered && hovered->Vid()->nVid==(*selectedSprites[0])->Vid()->nVid)
                    selectedSprites.InsertUnique(hovered);
            }
            else if (selectedSprites.InsertUnique(hovered)) selectedSprites.Delete(hovered);
            selectionPolygon.Release();
            dragBegin.x=dragBegin.y=-999.0f;
        }

        if (input->GetAction(0x3D))
            for (int index=0;index<selectedSprites.No();++index)
            {
                SPRITE* sprite=*selectedSprites[index];
                if (hovered && input->GetAction(0x20)) sprite->AddAction(0,104,static_cast<int>(PtrToU32(hovered)),0,0);
                else if (input->GetAction(0x22)) sprite->AddCycleAction(0,37,TruncateFloatToInt32OrMin(Mouse->X()),TruncateFloatToInt32OrMin(Mouse->Y()),0);
                else if (hovered && hovered!=*selectedSprites[0]) sprite->AddAction(0,32,static_cast<int>(PtrToU32(hovered)),0,0);
                else sprite->AddCycleAction(0,33,TruncateFloatToInt32OrMin(Mouse->X()),TruncateFloatToInt32OrMin(Mouse->Y()),TruncateFloatToInt32OrMin(Mouse->Z()));
            }
        if (spriteType==128 && selectedSprites.No() && (*selectedSprites[0])->Vid()->spriteClass==23)
            curRegion=static_cast<REGION*>(*selectedSprites[0]);
        return;
    }

    if (spriteType==64)
    {
        if (input->GetAction(0x23)) Mouse->HardwareOn();
        else Mouse->HardwareOff();
        if (options.optDelete)
        {
            if (input->GetAction(0x38))
            {
                undo.Begin();
                DeleteUnit(Mouse->Coor().XY(),spriteType);
                undo.End();
            }
        }
        else if (input->GetAction(0x38))
        {
            selectedSprites.Release();
            undo.Begin();
            railFinished=0;
            dragBegin=VECTOR(point,Mouse->Z());
        }
        else if (input->GetAction(0x3D))
        {
            for (int index=selectedSprites.No()-1;index>=0;--index) undo.DeleteLast();
            selectedSprites.DeleteAll();
            undo.End();
            railFinished=1;
        }
        else if (input->GetAction(0x23) && !railFinished && dragBegin.XY()!=point) CreateNewRail();
        else if (input->GetAction(0x37))
        {
            if (!(dragBegin.XY()!=point)) InsertUnit(Mouse->Vid(),Mouse->Coor(),Mouse->Direction());
            undo.End();
        }
        return;
    }

    if (spriteType==128)
    {
        if (options.optDelete)
        {
            if (input->GetAction(0x38))
            {
                undo.Begin();
                DeleteUnit(Mouse->Coor().XY(),spriteType);
                undo.End();
            }
            return;
        }
        if (input->GetAction(0x38)) dragBegin=VECTOR(point,Mouse->Z());
        else if (input->GetAction(0x23) && dragBegin.x!=-999.0f)
        {
            if (input->GetAction(0x3D)) { dragBegin.x=dragBegin.y=-999.0f; selectionPolygon.Release(); }
            else selectionPolygon.CreateBox(dragBegin.XY(),point);
        }
        else if (dragBegin.x!=-999.0f)
        {
            float x,width,y,height;
            if (dragBegin.x<point.x) { x=dragBegin.x; width=point.x-dragBegin.x; }
            else { x=point.x; width=dragBegin.x-point.x; }
            if (dragBegin.y<point.y) { y=dragBegin.y; height=point.y-dragBegin.y; }
            else { y=point.y; height=dragBegin.y-point.y; }
            if (Mouse->Vid()->spriteClass==23 && width!=0.0f && height!=0.0f)
            {
                undo.Begin();
                curRegion=static_cast<REGION*>(CreateSprite(Mouse->Vid(),VECTOR(
                    width/2.0f+x,
                    height/2.0f+y+dragBegin.z,dragBegin.z),ANGLE(0),0));
                if (curRegion)
                {
                    curRegion->SetSize(width,height);
                    if (!CallDialogBox(STRING("REGION_PROPERTY"),AppRegionProperty))
                    {
                        delete curRegion;
                        curRegion=0;
                    }
                    else undo.AddInsert(curRegion);
                }
                undo.End();
            }
            selectionPolygon.Release();
            dragBegin.x=dragBegin.y=-999.0f;
        }
        return;
    }

    if (input->GetAction(0x38)) { dragBegin=Mouse->Coor(); undo.Begin(); }
    if (input->GetAction(0x23) && !options.optAirBrush && dragBegin.x!=-999.0f &&
        (AbsFloat(dragBegin.x-Mouse->X())>20.0f || AbsFloat(dragBegin.y-Mouse->Y())>20.0f))
    {
        if (input->GetAction(0x3D))
        {
            dragBegin.x=dragBegin.y=-999.0f;
            undo.End(); undo.Undo(); undo.Reset();
        }
        else
        {
            undo.End(); undo.Undo(); undo.Reset(); undo.Begin();
            if (input->GetAction(0x22)) FillRomb(dragBegin.XY(),point);
            else FillBox(dragBegin.XY(),point);
        }
    }
    else if (input->GetAction(0x38) || (input->GetAction(0x23) && options.optAirBrush && CurrentTime-lastAirBrushTime>1000u/optAirBrushDensity))
    {
        lastAirBrushTime=CurrentTime;
        if (options.optDelete) DeleteUnit(Mouse->Coor().XY(),spriteType);
        else InsertUnit(Mouse->Vid(),Mouse->Coor(),Mouse->Direction());
    }
    if (input->GetAction(0x37)) { undo.End(); dragBegin.x=dragBegin.y=-999.0f; }
    if (Mouse->Vid()->noDir>1 || (Mouse->Vid()->property&0x20000000u))
    {
        static VECTOR2 rotateBegin;
        if (input->GetAction(0x3D)) rotateBegin=Mouse->Coor().XY();
        if (input->GetAction(0x25))
            Mouse->ChangeDirection(ANGLE(rotateBegin-Mouse->Coor().XY())+ANGLE(static_cast<unsigned char>(Mouse->Vid()->directionQuantizationOffset)));
    }
}
