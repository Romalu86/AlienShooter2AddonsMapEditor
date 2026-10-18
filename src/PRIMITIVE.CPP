#include "sprite.h"
#include "vid.h"

PRIMITIVE::PRIMITIVE(VID* vid, const VECTOR& position, ANGLE direction, SPRITE* parent)
    : SPRITE(vid, position, direction, parent)
{
}

PRIMITIVE::~PRIMITIVE()
{
}

void PRIMITIVE::Tact()
{
    PrimitiveTact();
}

void PRIMITIVE::MoveTact()
{
}

void PRIMITIVE::DeletePointerToSprite(SPRITE*)
{
}

void PRIMITIVE::Draw()
{
    m_vid->Draw(this);
}

void PRIMITIVE::DetectTact()
{
}

void PRIMITIVE::DrawSecondaryInfo()
{
}

void PRIMITIVE::DrawGoalLine()
{
}
