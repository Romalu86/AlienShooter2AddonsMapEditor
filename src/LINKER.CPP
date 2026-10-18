#include "sprite.h"
#include "vid.h"

LINKER::LINKER(VID* vid, const VECTOR& position, ANGLE direction, SPRITE* linked)
    : SPRITE(vid, position, direction, linked), m_linkedSprite(linked)
{
    if (linked)
    {
        linked->AddLink(this);
        m_linkOffset.x = Coor().x-linked->Coor().x;
        m_linkOffset.y = Coor().y-linked->Coor().y;
        m_linkOffset.y /= 0.7070602178573608f;
    }
    else
    {
        m_linkOffset = VECTOR2(0, 0);
    }
    m_linkDirection = direction;
}

LINKER::~LINKER()
{
    if (m_vid->linkVid)
    {
        for (SPRITE* linked=Link(); linked; linked=linked->Link())
        {
            if (linked->Animation()<15 && linked->Vid()==m_vid->linkVid)
            {
                linked->ChangeAnimation(15);
                break;
            }
        }
    }
    BreakLink();
}

void LINKER::LinkRotate(ANGLE direction)
{
    const VECTOR2 offset = (direction-m_linkDirection).RotateY(m_linkOffset);
    SPRITE* const anchor = m_linkedSprite ? m_linkedSprite : m_childBacklink;
    const VECTOR2 position = VECTOR2(anchor->X(), anchor->Y())+offset;
    ChangeCoorForLinkRotate(position);
}
