#include "mousetips.h"
#include "engine_globals.h"
#include "input.h"
#include "map.h"
#include "graph.h"
#include "sprite.h"
#include "vid.h"
#include "mylib.h"
#include "basetype.h"
#include "constant.h"
#include "frame.h"
#include <new>
#include <string.h>

namespace {

unsigned int lastMouseMoveTime = 0;
float lastMouseX = 0.0f;
float lastMouseY = 0.0f;

}

MOUSETIPS::MOUSETIPS() : tip(nullptr) {}
MOUSETIPS::~MOUSETIPS() { Clear(); }

void MOUSETIPS::Tact(const INPUT* input)
{

    static STRING cachedMouseTipText;

    const unsigned int now = ::RealCurrentTime;
    const float mouseX = input->ClientMouseXY().x;
    const float mouseY = input->ClientMouseXY().y;

    if ((lastMouseX<mouseX || lastMouseX>mouseX) ||
       (lastMouseY<mouseY || lastMouseY>mouseY))
    {
        lastMouseMoveTime = now;
        lastMouseX = mouseX;
        lastMouseY = mouseY;
    }

    const unsigned int lastTick = lastMouseMoveTime;
    const unsigned int delay = static_cast<unsigned int>(Const->MouseTipsTime);
    const unsigned int elapsed = now-lastTick;
    if (elapsed <= delay)
    {
        Clear();
        return;
    }

    if (input->GetAction(IM_LBUTTONCLICK) != 0 || input->GetAction(IM_KEYDOWN) != 0)
    {
        Clear();
        return;
    }

    MAP* const map = ::Map;
    if ((map->MenuState().ControlFlags()&0x01u) != 0u || (map->Flags()&MAP::FLAG_MOUSE_TIPS) == 0u)
    {
        Clear();
        return;
    }

    SPRITE* sprite = tip;
    if (!sprite)
    {
        VID* const tooltipVid = map->Vid(15);

        STRING* const cached = &cachedMouseTipText;
        {
            const STRING queried = map->GetMouseTipsString();
            *cached = queried;
        }

        if (tooltipVid == ::EmptyVid)
            return;
        if (strcmp(cached->m_value, StringGlobals::EmptyText()) == 0)
            return;

        float x = input->ClientMouseXY().x+20.0f;
        float y = static_cast<float>(static_cast<double>(input->ClientMouseXY().y)+4000.0+8.0);
        sprite = Map->CreateSprite(tooltipVid, VECTOR(x,y,4000.0f), ANGLE(static_cast<unsigned char>(0u)), 0);
        tip = sprite;
        if (!sprite)
            return;

        const STRING ownedText(*cached);
        (void)sprite->Action(ACT_SET_TEXT, static_cast<int>(PtrToU32(&ownedText)), 0, 0);

        sprite = tip;
        const VECTOR2 textSize = static_cast<STEXT*>(sprite)->TextSizeXY();
        const float rightEdge = x+textSize.x;
        if (!(Graph->ViewXMax()>rightEdge))
        {
            x = Graph->ViewXMax()-textSize.x;
            y+=10.0f;
        }

        const float topEdge = static_cast<float>(static_cast<double>(tooltipVid->sizeXYZ.y)/2.0+y-4000.0);
        if (Graph->ViewYMin() >= topEdge)
            y = static_cast<float>(static_cast<double>(tooltipVid->sizeXYZ.y)/2.0+
                input->ClientMouseXY().y+4000.0+10.0);

        tip->ChangeCoor(VECTOR(Map->FromScreen(VECTOR2(x,y)),4000.0f));
        return;
    }

    const unsigned int currentDelay = static_cast<unsigned int>(Const->MouseTipsTime);
    const unsigned int currentLastTick = lastMouseMoveTime;
    if (::RealCurrentTime-currentLastTick <= currentDelay+500u)
        return;

    lastMouseMoveTime = currentLastTick+500u;
    bool changed;
    {
        const STRING current = map->GetMouseTipsString();
        const char* const cachedText = cachedMouseTipText.value;
        changed = strcmp(current.value, cachedText) != 0;
    }
    if (changed)
        Clear();
}

void MOUSETIPS::Clear()
{
    delete tip;
    tip = 0;
}

void MOUSETIPS::DeletePointerToSprite(SPRITE* sprite)
{
    if (tip == sprite)
        tip = 0;
}
