#pragma once
#include "basetype.h"
#include "mylib.h"

#include "myerror.h"
#include "sprite_list.h"

class INPUT;
class STRING;
class SPRITE;

class MENU : public SPRITE_LIST
{
    friend class FRAME;
public:
    MENU();
    void Control(INPUT* input);
    int Load(const STRING& fileName, VECTOR2 offset = VECTOR2(0, 0), int referenceWidth = 0);
    int Save(const STRING& fileName);
    int DeleteFromFile(const STRING& fileName, VECTOR2 offset = VECTOR2(0, 0), int referenceWidth = 0);
    int NVidUnderCursor() const;
    int NDirUnderCursor() const;
    SPRITE* SpriteWithName(const STRING& name) const;
    void Error(TYPE_ERROR type, const STRING& subject, unsigned long code);

    unsigned ControlFlags() const
    {
        return m_controlFlags;
    }
    SPRITE* SelectedSprite() const
    {
        return m_selectedSprite;
    }

private:
    unsigned int m_controlFlags;
    SPRITE* m_selectedSprite;
};


VECTOR2*  MenuComputeViewportOffsetTo(const void* self, VECTOR2* out, const VECTOR* value);
