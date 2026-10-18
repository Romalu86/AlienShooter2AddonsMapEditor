#include "map_edit.h"
#include "engine_globals.h"
#include "sprite_collector.h"

namespace {

int railDirection[64]={
    0,4,36,36,36,36,36,5, 7,2,11,36,36,36,36,36,
    36,9,1,8,36,36,36,36, 36,36,10,3,5,36,36,36,
    36,36,36,6,0,7,36,36, 36,36,36,36,4,2,9,36,
    36,36,36,36,36,11,1,10, 6,36,36,36,36,36,8,3};
int railVariant[64]={
    0,0,36,36,36,36,36,0, 1,0,1,36,36,36,36,36,
    36,0,0,0,36,36,36,36, 36,36,1,1,1,36,36,36,
    36,36,36,0,1,0,36,36, 36,36,36,36,1,1,1,36,
    36,36,36,36,36,0,1,0, 1,36,36,36,36,36,1,0};
int railStepX[8]={0,1,1,1,0,-1,-1,-1};
int railStepY[8]={-1,-1,0,1,1,1,0,-1};
int previousRailDirection=-1;
int railRealDirection[12]={32,50,41,59,33,39,24,40,30,14,58,42};
int railTransition[64]={
    0,1,1,9,9,9,7,7, 0,1,2,2,9,9,9,0,
    1,1,2,3,3,9,9,9, 9,2,2,3,4,4,9,9,
    9,9,3,3,4,5,5,9, 9,9,9,4,4,5,6,6,
    7,9,9,9,5,5,6,7, 0,0,9,9,9,6,6,7};
float previousRailZ;

int OptimizeRailDirection(SPRITE* rail,int incoming,int outgoing,int previousZ,int nextZ)
{
    if (!rail || outgoing>7 || incoming>7) return 1;
    if (outgoing==-1 || incoming==-1) {
        int index;
        for (SPRITE* sprite=Sprites->FirstDrawingSprite(&index,rail->Vid()->layer);sprite;
             sprite=Sprites->NextDrawingSprite(&index,rail->Vid()->layer)) {
            if (!sprite->IsSpriteClass(22u)
                || rail->Coor()!=sprite->Coor()
                || sprite==rail) continue;
            const int direction=sprite->RealDirection()%12;
            for (int i=0;i<64;++i) {
                if (railDirection[i]==direction && incoming==-1 &&
                    (outgoing==-1 || outgoing==(i>>3) || outgoing==(((i>>3)+1)&7) || outgoing==(((i>>3)-1)&7)))
                    incoming=i>>3;
            }
        }
    }
    if (incoming==-1) incoming=outgoing;
    if (outgoing==-1) outgoing=incoming;
    const int index=(incoming<<3)|outgoing;
    int variant=railVariant[index];
    if (variant!=36) {
        if (static_cast<double>(nextZ)>rail->Z()) variant=(variant+1)*12;
        else if (static_cast<double>(previousZ)>rail->Z()) variant=((variant^1)+1)*12;
        else variant=0;
    }
    rail->Action(60,((railDirection[index]+variant)*256)/rail->Vid()->noDir,0,0);
    return railDirection[index]==36;
}

}

void MAP_EDIT::CreateNewRail()
{
    static int heightDirections[3]={
        TruncateFloatToInt32OrMin(Mouse->Vid()->sizeXYZ.z),
        BitsToSigned32(0u-static_cast<unsigned int>(TruncateFloatToInt32OrMin(Mouse->Vid()->sizeXYZ.z))),0};
    if (selectedSprites.No())
    {
        SPRITE* rail=*selectedSprites[selectedSprites.No()-1];
        float radius1,radius2;
        int current=ANGLE(m_input.MouseXY()-rail->Coor().XY(),&radius1).value;
        current=((current+16)&255)>>5;
        if (previousRailDirection!=-1)
            current=railTransition[(previousRailDirection<<3)|current];
        int next=ANGLE(
            m_input.MouseXY().x-rail->X()-railStepX[current]*Mouse->Vid()->sizeXYZ.x,
            m_input.MouseXY().y-rail->Y()-railStepY[current]*Mouse->Vid()->sizeXYZ.y,
            &radius2).value;
        next=((next+16)&255)>>5;
        next=railTransition[(current<<3)|next];
        if (selectedSprites.No()>1)
            previousRailZ=(*selectedSprites[selectedSprites.No()-2])->Z();
        if (radius2<radius1 && next!=9)
        {
            float newZ=insertZ;
            if (options.optGround0)
                newZ+=GetGroundZ(Mouse->Vid(),VECTOR2(
                    rail->X()+railStepX[current]*Mouse->Vid()->sizeXYZ.x,
                    rail->Y()+railStepY[current]*Mouse->Vid()->sizeXYZ.y),Mouse->Direction());
            if (newZ>=0.0f)
            {
                const float numerator=static_cast<float>(static_cast<double>(newZ)+Mouse->Vid()->sizeXYZ.z-1.0);
                const float quotient=numerator/Mouse->Vid()->sizeXYZ.z;
                newZ=quotient*Mouse->Vid()->sizeXYZ.z;
            }
            else
            {
                const float numerator=static_cast<float>(static_cast<double>(newZ)-Mouse->Vid()->sizeXYZ.z+1.0);
                const float quotient=numerator/Mouse->Vid()->sizeXYZ.z;
                newZ=quotient*Mouse->Vid()->sizeXYZ.z;
            }
            Mouse->ChangeCoor(VECTOR(Mouse->Coor().XY(),newZ));
            OptimizeRailDirection(rail,previousRailDirection,current,
                TruncateFloatToInt32OrMin(previousRailZ),TruncateFloatToInt32OrMin(Mouse->Z()));
            previousRailZ=rail->Z();
            float insertHeight=previousRailZ;
            if (Mouse->Z()<previousRailZ) insertHeight-=Mouse->Vid()->sizeXYZ.z;
            else if (Mouse->Z()>previousRailZ) insertHeight+=Mouse->Vid()->sizeXYZ.z;
            rail=InsertUnit(Mouse->Vid(),VECTOR(
                rail->X()+railStepX[current]*Mouse->Vid()->sizeXYZ.x,
                rail->Y()+railStepY[current]*Mouse->Vid()->sizeXYZ.y,insertHeight),
                ANGLE(static_cast<unsigned char>(1)));
            if (!rail) return;
            selectedSprites.Insert(rail);
            previousRailDirection=current;
            current=next;
        }
        if (OptimizeRailDirection(rail,previousRailDirection,current,
                TruncateFloatToInt32OrMin(previousRailZ),TruncateFloatToInt32OrMin(Mouse->Z())))
        {
            selectedSprites.Delete(rail);
            undo.DeleteLast();
            delete rail;
            if (selectedSprites.No()==1) { previousRailDirection=-1; return; }
            rail=*selectedSprites[selectedSprites.No()-1];
            const int direction=railRealDirection[rail->RealDirection()%12];
            previousRailDirection=(previousRailDirection==(direction&7)) ? (direction>>3)^4 : (direction&7)^4;
        }
    }
    else
    {
        previousRailDirection=-1;
        previousRailZ=Mouse->Z();
        selectedSprites.Insert(InsertUnit(Mouse->Vid(),Mouse->Coor(),
            ANGLE(static_cast<unsigned char>(Mouse->Vid()->directionQuantizationOffset))));
    }
}
