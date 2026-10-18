#include "sprite.h"
#include "vid.h"
#include "map.h"
#include "graph.h"
#include "region.h"
#include "engine_globals.h"
#include <algorithm>
#include <cstdint>

void GRAPH::PutAlphaPixel(float x,float y,COLOR color,unsigned int alpha) const
{
    struct Vertex { float x,y,z,rhw; unsigned int diffuse; };
    const Vertex point={x,y,0.99999988079071044921875f,1.0f,
        (color.value&0x00FFFFFFu)|(static_cast<unsigned int>(alpha)<<24)};
    device->SetTexture(0,0);
    SetAlphaBlend(D3DBLEND_SRCALPHA,D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_ZWRITEENABLE,0);
    device->SetRenderState(D3DRS_ZFUNC,D3DCMP_ALWAYS);
    device->SetRenderState(D3DRS_ALPHATESTENABLE,0);
    DrawPrimitive(D3DPT_POINTLIST,D3DFVF_XYZRHW|D3DFVF_DIFFUSE,&point,sizeof(point),1);
}

void GRAPH::WuLine(VECTOR2 first,VECTOR2 second,COLOR color) const
{
    if (first.y>second.y) std::swap(first,second);
    PutPixel(first,color);
    int dx=TruncateDoubleToInt32(static_cast<double>(second.x)-first.x);
    const int step=dx>=0 ? 1 : -1;
    if (dx<0) dx=static_cast<int>(0u-static_cast<unsigned int>(dx));
    int dy=TruncateDoubleToInt32(static_cast<double>(second.y)-first.y);
    if (!dx || !dy) { Line(first,second,color); return; }
    if (dx==dy) {
        do {
            const float oldY=first.y;
            first.x+=static_cast<float>(step); first.y+=1.0f;
            PutPixel(VECTOR2(first.x,oldY),color);
        } while (--dy);
        return;
    }
    unsigned short accumulator=0;
    if (dy>dx) {
        const unsigned short increment=static_cast<unsigned short>(
            static_cast<std::int32_t>(static_cast<std::uint32_t>(dx)<<16)/dy);
        while (--dy) {
            const unsigned short previous=accumulator;
            accumulator=static_cast<unsigned short>(accumulator+increment);
            if (accumulator<=previous) first.x+=static_cast<float>(step);
            first.y+=1.0f;
            const int coverage=accumulator>>8;
            PutAlphaPixel(first.x,first.y,color,255-coverage);
            PutAlphaPixel(first.x+static_cast<float>(step),first.y,color,coverage);
        }
    } else {
        const unsigned short increment=static_cast<unsigned short>(
            static_cast<std::int32_t>(static_cast<std::uint32_t>(dy)<<16)/dx);
        while (--dx) {
            const unsigned short previous=accumulator;
            accumulator=static_cast<unsigned short>(accumulator+increment);
            if (accumulator<=previous) first.y+=1.0f;
            first.x+=static_cast<float>(step);
            const int coverage=accumulator>>8;
            PutAlphaPixel(first.x,first.y,color,255-coverage);
            PutAlphaPixel(first.x,first.y+1.0f,color,coverage);
        }
    }
    PutPixel(second,color);
}

void GRAPH::Circle(const VECTOR2& centre,float radius,COLOR color) const
{
    const int cx=TruncateFloatToInt32(centre.x),cy=TruncateFloatToInt32(centre.y);
    int x=0,y=TruncateFloatToInt32(radius),error=1-y,insideStep=3,outsideStep=5-2*y;
    do {
        PutPixel(cx+x,cy+y,color);
        if (x) PutPixel(cx-x,cy+y,color);
        if (y) PutPixel(cx+x,cy-y,color);
        if (x && y) PutPixel(cx-x,cy-y,color);
        if (x!=y) {
            PutPixel(cx+y,cy+x,color);
            if (x) PutPixel(cx+y,cy-x,color);
            if (y) PutPixel(cx-y,cy+x,color);
            if (x && y) PutPixel(cx-y,cy-x,color);
        }
        if (error<0) { error+=insideStep; insideStep+=2; outsideStep+=2; }
        else { error+=outsideStep; insideStep+=2; outsideStep+=4; --y; }
        ++x;
    } while (x<=y);
}

void SPRITE::DrawStack(int lane)
{
    if (!m_extraData || !m_extraData->actionLane[lane].No()) return;
    bool buildAreaDrawn=false;
    VECTOR2 origin=ScreenXY();
    const LIST<ACT>& stack=m_extraData->actionLane[lane];
    for (int i=stack.No()-1;i>=0;--i) {
        const ACT& action=*stack[i];
        const VECTOR coordinate(action.arg1,action.arg2,action.arg3);
        if (action.command==ACT_SET_UPLINK && action.arg1) {
            const SPRITE* const linked=reinterpret_cast<const SPRITE*>(static_cast<std::intptr_t>(action.arg1));
            Graph->WuLine(origin,Map->ToScreen(linked->Coor()),GRAPH::YELLOW);
        } else if (!buildAreaDrawn && action.command==ACT_BUILD_UNIT && action.arg2<0) {
            buildAreaDrawn=true;
            const VECTOR2 half(static_cast<int>(0u-static_cast<unsigned int>(action.arg2)),
                               static_cast<int>(0u-static_cast<unsigned int>(action.arg3)));
            Graph->Box(ScreenXY()-half,ScreenXY()+half,GRAPH::LIGHTRED);
        } else if (action.command==ACT_SET_GOAL_COOR) {
            Graph->WuLine(origin,Map->ToScreen(coordinate),GRAPH::GRAY);
        } else if (action.command==ACT_FLAGMAN_TRIGGER) {
            const VECTOR2 point(action.arg1,action.arg2);
            Graph->Circle(Map->ToScreen(VECTOR(point,Map->GetGroundZ(point))),
                static_cast<float>(action.arg3),GRAPH::WHITE);
        } else if (action.command==ACT_COOR_ATTACK) {
            Graph->WuLine(origin,Map->ToScreen(coordinate),GRAPH::BLUE);
        } else if (action.command==ACT_MOVE || action.command==ACT_MOVE_ACCURATELY || action.command==ACT_STAND_FIRM) {
            const VECTOR2 target=Map->ToScreen(coordinate);
            Graph->WuLine(origin,target,GRAPH::GREEN);
            origin=target;
            Graph->PutsXY(origin+VECTOR2(3,8),Int2Str(i),GRAPH::GREEN);
        } else if (action.command==ACT_CHANGE_COOR) {
            const VECTOR2 target=Map->ToScreen(coordinate);
            Graph->WuLine(origin,target,GRAPH::LIGHTGREEN);
            origin=target;
        } else if (action.command==ACT_ATTACK && action.arg1) {
            const SPRITE* const target=reinterpret_cast<const SPRITE*>(static_cast<std::intptr_t>(action.arg1));
            Graph->WuLine(origin,target->ScreenXY(),GRAPH::RED);
            origin=target->ScreenXY();
        } else if (action.command==12 || action.command==ACT_RANDOM || action.command==ACT_PAUSE) {
            const char letter=action.command==12 ? 'L' : action.command==ACT_RANDOM ? 'R' : 'P';
            Graph->PutcXY(origin+Random2XY(-5.0f,5.0f),letter,GRAPH::WHITE);
        }
    }
}
