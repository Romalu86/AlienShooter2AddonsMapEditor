#include "myerror.h"
#include "engine_globals.h"
#include <new>
#include <cmath>
#include "menu.h"
#include "mylib.h"
#include "script.h"
#include "sprite.h"
#include "vid.h"
#include "resread.h"
#include "input.h"
#include "sprite_list.h"

namespace { SPRITE* pressedMenuSprite = 0; }

STRING SPRITE::GetName() const
{
    STRING emptyName;
    const STRING* nameSource = 0;

    EX_SPRITE_DATA* const extraData = ExtraData();
    if (extraData)
    {
        nameSource = &extraData->nameText;
    }
    else
    {
        emptyName = StringGlobals::EmptyText();
        nameSource = &emptyName;
    }

    return STRING(*nameSource);
}

#include "graph.h"
#include "region.h"
#include "map.h"

VECTOR2 MAP::ToScreen(const VECTOR& worldCoor) const
{
    return worldCoor.ViewCoor()-m_shiftXY;
}

int MENU::NVidUnderCursor() const
{
    if (!m_selectedSprite) return 0;
    VID* const vid = m_selectedSprite->Vid();
    return vid->nVid;
}

int MENU::NDirUnderCursor() const
{
    if (!m_selectedSprite) return 0;
    VID* const vid = m_selectedSprite->Vid();
    return vid->RealDirection(m_selectedSprite->Direction().value);
}

#include "basetype.h"

MENU::MENU()
{
    m_controlFlags = *reinterpret_cast<volatile unsigned int*>(&m_controlFlags)&0xFFFFFFFCu;
    m_selectedSprite = 0;
}

void MENU::Error(TYPE_ERROR type, const STRING& subject, unsigned long code)
{
    const STRING category("MENU");
        ::Error->Error(category, type, subject, code);
}

SPRITE* MENU::SpriteWithName(const STRING& name) const
{
    for (int spriteIndex = 0; spriteIndex<m_count; ++spriteIndex)
    {
        bool equal;
        {
            const STRING childName = m_data[spriteIndex]->GetName();
            equal = std::strcmp(childName.value, name.value) == 0;
        }
        if (equal) return m_data[spriteIndex];
    }
    return 0;
}

void MENU::Control(INPUT* input)
{
    const VECTOR2 cursor = Map->FromScreen(input->ClientMouseXY());

    m_selectedSprite = 0;
    m_controlFlags &= ~3u;

    int spriteIndex = 0;
    if (m_count != 0) do
        {
            SPRITE* const sprite = m_data[spriteIndex];
            if (!sprite) break;

            VID* const vid = sprite->Vid();
            if (vid->moveMask == 0u) continue;

            const int animation = sprite->Animation();
            if (animation == 14 || animation >= 15 || animation == 13 || animation == 12)
                continue;

            const double screenX = static_cast<double>(sprite->X());
            const double screenY = static_cast<double>(sprite->Y())-static_cast<double>(sprite->Z());
            const bool cursorInsideSprite =
                static_cast<double>(cursor.x) >= screenX-static_cast<double>(vid->halfSizeXY.x) &&
            screenX+static_cast<double>(vid->halfSizeXY.x) >= static_cast<double>(cursor.x) &&
            static_cast<double>(cursor.y)>screenY-static_cast<double>(vid->sizeXYZ.z)-static_cast<double>(vid->halfSizeXY.y) &&
            screenY+static_cast<double>(vid->halfSizeXY.y)>static_cast<double>(cursor.y);

            if (!cursorInsideSprite || (m_selectedSprite && !(sprite->Z()>m_selectedSprite->Z())))
            {
                sprite->ChangeAnimation(animation&1);
                continue;
            }

            if (m_selectedSprite)
                m_selectedSprite->ChangeAnimation(m_selectedSprite->Animation()&1);
            m_selectedSprite = sprite;
        }
        while (++spriteIndex<m_count);

    if (input->GetAction(IM_LBUTTONCLICK) != 0 || input->GetAction(IM_RBUTTONCLICK) != 0)
        pressedMenuSprite = m_selectedSprite;

    if (m_selectedSprite)
    {
        const int originalAnimation = m_selectedSprite->Animation();

        if (pressedMenuSprite == m_selectedSprite && originalAnimation <= 7)
        {
            if (input->GetAction(IM_LBUTTONUP) != 0)
            {
                SPRITE* const selected = m_selectedSprite;
                VID* const vid = selected->Vid();
                if (vid->noAnimCadr[1] != 0 && vid->noAnimCadr[0] != 0)
                {
                    selected->ChangeAnimation(((~selected->Animation())&1) | 2);
                    if ((originalAnimation&~1) == 4)
                        m_selectedSprite->AddRuntimeFlags(0x00008000u);
                }
            }
            else if (input->GetAction(IM_LBUTTONCLICK) != 0)
            {
                SPRITE* const selected = m_selectedSprite;
                VID* const vid = selected->Vid();
                int nextAnimation;
                if (vid->noAnimCadr[10] != 0 && vid->noAnimCadr[11] != 0)
                    nextAnimation = (selected->X()>cursor.x)?10:11;
                else
                    nextAnimation = (selected->Animation()&1) | 4;
                selected->ChangeAnimation(nextAnimation);
            }
        }

        if (input->GetAction(IM_LBUTTONDOWN) == 0 && input->GetAction(IM_LBUTTONUP) == 0)
        {
            SPRITE* const selected = m_selectedSprite;
            const int currentAnimation = selected->Animation();
            bool mayAdvance = true;
            if ((currentAnimation&~1) == 4)
            {
                if (::CurrentTime-selected->LastAnimationTick() == 0u ||
                    selected->CurrentFrame() != selected->CurrentFrameEnd())
                    mayAdvance = false;
            }

            if (mayAdvance)
            {
                VID* const vid = selected->Vid();
                if (vid->noAnimCadr[8] != 0 && vid->noAnimCadr[9] != 0)
                {
                    selected->ChangeAnimation((selected->X()>cursor.x)?8:9);
                    if ((originalAnimation&~1) == 10)
                        m_selectedSprite->AddRuntimeFlags(0x00008000u);
                }
                else
                {
                    selected->ChangeAnimation((currentAnimation&1) | 2);
                    if ((originalAnimation&~1) == 4)
                        m_selectedSprite->AddRuntimeFlags(0x00008000u);
                }
            }
        }
        input->RemoveEvent(IM_LBUTTONCLICK, 0u);
        input->RemoveEvent(IM_RBUTTONCLICK, 0u);
        if (input->GetAction(IM_LBUTTONUP) != 0 &&
            pressedMenuSprite == m_selectedSprite)
            m_controlFlags |= 1u;
        if (input->GetAction(IM_RBUTTONUP) != 0 &&
            pressedMenuSprite == m_selectedSprite)
            m_controlFlags |= 2u;
    }
}

int MENU::Load(const STRING& fileName, VECTOR2 offset, int referenceWidth)
{
    RESOURCE resource;
    if (resource.OpenForRead(fileName, ResourceType::Menu) != 0)
    {
        Error(E_OPEN, fileName, 0u);
        return 1;
    }

    if (resource.GoBegin(ResourceType::Header) != 0)
    {
        STRING section("'HEAD'in menu");
        Error(E_SECTION, section, 0u);
        return 1;
    }

    int version;
    union HEADER_VALUE
    {
        float f;
        int i;
    }
    sizeX, sizeY, shiftX, shiftY;
    (void) resource.RESOURCE::Read(&version, 4);
    if (version >= 15)
    {
        (void) resource.Read(&sizeX, 4);
        (void) resource.Read(&sizeY, 4);
        (void) resource.Read(&shiftX, 4);
        (void) resource.Read(&shiftY, 4);
    }
    else
    {
        (void) resource.Read(&sizeX, 4);
        (void) resource.Read(&sizeY, 4);
        sizeX.f = static_cast<float>(sizeX.i);
        sizeY.f = static_cast<float>(sizeY.i);
        (void) resource.Read(&shiftX, 4);
        (void) resource.Read(&shiftY, 4);
        shiftX.f = static_cast<float>(shiftX.i);
        shiftY.f = static_cast<float>(shiftY.i);
    }

    const VECTOR2 translation = VECTOR2(-shiftX.f, -shiftY.f) -
        VECTOR2(sizeX.f, sizeY.f) / 2.0f + Graph->SizeXY() / 2.0f + offset;
    const VECTOR2 referenceSize(static_cast<float>(referenceWidth),
        referenceWidth == 1280 ? 720.0f : referenceWidth == 1024 ? 768.0f : 600.0f);
    SPRITE* const endSprite = reinterpret_cast<SPRITE*>(U32ToPtr(0xFFFFFFFFu));
    if (resource.GoNext(ResourceType::Sprites) == 0)
    {
        for (; ;)
        {
            SPRITE* sprite = Map->LoadSprite(&resource, version);
            if (sprite == endSprite) break;
            if (sprite)
            {
                sprite->ChangeCoor(VECTOR(sprite->Coor().XY() + translation, sprite->Z()));
                if (referenceWidth != 0)
                {
                    const VECTOR2 halfScreen = Graph->SizeXY() / 2.0f;
                    const VECTOR2 edgeShift = halfScreen - referenceSize / 2.0f;
                    const float x = sprite->X() +
                        (halfScreen.x <= Map->ToScreen(sprite->Coor()).x ? edgeShift.x : -edgeShift.x);
                    sprite->ChangeCoor(VECTOR(x, sprite->Y(), sprite->Z()));
                    const float y = sprite->Y() +
                        (halfScreen.y <= Map->ToScreen(sprite->Coor()).y ? edgeShift.y : -edgeShift.y);
                    sprite->ChangeCoor(VECTOR(sprite->X(), y, sprite->Z()));
                }
                (void) sprite->Action(ACT_RESTORE, static_cast<int>(PtrToU32(&resource)), version, 0);
            }
            (void) resource.GoNextSub(ResourceType::Sprites);
        }
        resource.Close();
        return 0;
    }

    if (resource.GoBegin(ResourceType::SpriteIndices) == 0)
    {
        for (; ;)
        {
            SPRITE* sprite = Map->LoadSprite(&resource, version);
            if (sprite == endSprite) break;
            if (sprite)
            {
                VECTOR2 shift = translation;
                if (referenceWidth != 0)
                {
                    shift = VECTOR2(-shiftX.f, -shiftY.f) + offset;
                    if (Graph->SizeX() / 2.0f < sprite->X() - shiftX.f)
                        shift.x += Graph->SizeX() - sizeX.f;
                    if (Graph->SizeY() / 2.0f < sprite->Y() - shiftY.f - sprite->Z())
                        shift.y += Graph->SizeY() - sizeY.f;
                }
                sprite->ChangeCoor(VECTOR(sprite->Coor().XY() + shift, sprite->Z()));
            }
        }
        resource.Close();
        return 0;
    }

    STRING section("'SPR ' or 'SPRI' in menu");
    Error(E_SECTION, section, 0u);
    return 1;
}

int MENU::Save(const STRING& fileName)
{
    RESOURCE resource;
    const int version = 20;
    if (resource.OpenForWrite(fileName, ResourceType::Menu))
    {
        Error(E_CREATE, fileName, 0);
        return 1;
    }

    resource.PreAppend(ResourceType::Header, 0);
    resource.Write(&version, sizeof(version));
    const VECTOR2 size = Graph->SizeXY();
    resource.Write(&size.x, sizeof(size.x));
    resource.Write(&size.y, sizeof(size.y));
    const VECTOR2 shift = Map->FromScreen(VECTOR2(0, 0));
    resource.Write(&shift.x, sizeof(shift.x));
    resource.Write(&shift.y, sizeof(shift.y));
    resource.PostAppend();

    for (int index = 0; index < m_count; ++index)
    {
        SPRITE* const sprite = m_data[index];
        if (sprite->ChildBacklink() || sprite == Mouse ||
            (sprite->RuntimeFlags() & 0x800u))
            continue;
        resource.PreAppend(ResourceType::Sprites, 0);
        sprite->Save(&resource);
        sprite->Action(ACT_SAVE, static_cast<int>(PtrToU32(&resource)), 0, 0);
        resource.PostAppend();
    }
    resource.PreAppend(ResourceType::Sprites, 0);
    const int end = -1;
    resource.Write(&end, sizeof(end));
    resource.PostAppend();
    resource.Close();
    return 0;
}

int MENU::DeleteFromFile(const STRING& fileName, VECTOR2 offset, int referenceWidth)
{
    RESOURCE resource;
    if (resource.OpenForRead(fileName, ResourceType::Menu) != 0)
    {
        Error(E_OPEN, fileName, 0u);
        return 1;
    }

    if (resource.GoBegin(ResourceType::Header) != 0)
    {
        STRING section("'HEAD'in menu");
        Error(E_SECTION, section, 0u);
        return 1;
    }

    int version;
    union HEADER_VALUE
    {
        float f;
        int i;
    }
    sizeX, sizeY, shiftX, shiftY;
    (void) resource.RESOURCE::Read(&version, 4);
    if (version >= 15)
    {
        (void) resource.Read(&sizeX, 4);
        (void) resource.Read(&sizeY, 4);
        (void) resource.Read(&shiftX, 4);
        (void) resource.Read(&shiftY, 4);
    }
    else
    {
        (void) resource.Read(&sizeX, 4);
        (void) resource.Read(&sizeY, 4);
        sizeX.f = static_cast<float>(sizeX.i);
        sizeY.f = static_cast<float>(sizeY.i);
        (void) resource.Read(&shiftX, 4);
        (void) resource.Read(&shiftY, 4);
        shiftX.f = static_cast<float>(shiftX.i);
        shiftY.f = static_cast<float>(shiftY.i);
    }

    if (resource.GoNext(ResourceType::Sprites) != 0)
    {
        STRING section("'SPR ' in MENU::DeleteFromFile");
        Error(E_SECTION, section, 0u);
        return 1;
    }

    const VECTOR translation(VECTOR2(-shiftX.f, -shiftY.f) -
        VECTOR2(sizeX.f, sizeY.f) / 2.0f + Graph->SizeXY() / 2.0f + offset, 0.0f);
    const VECTOR2 referenceSize(static_cast<float>(referenceWidth),
        referenceWidth == 1280 ? 720.0f : referenceWidth == 1024 ? 768.0f : 600.0f);
    const float epsilon = 3.6f;

    int persistentId, nvid;
    VECTOR authored;
    for (; ;)
    {
        (void) resource.RESOURCE::Read(&persistentId, 4);
        if (persistentId == -1) break;

        (void) resource.RESOURCE::Read(&nvid, 4);
        (void) resource.Read(&authored.x, 4);
        (void) resource.Read(&authored.y, 4);
        (void) resource.Read(&authored.z, 4);
        authored = VECTOR(Map->FromScreen(authored.XY()),authored.z);
        if (referenceWidth != 0)
        {
            const VECTOR2 halfScreen = Graph->SizeXY() / 2.0f;
            const VECTOR2 edgeShift = halfScreen - referenceSize / 2.0f;
            const VECTOR2 screen = Map->ToScreen(authored + translation);
            authored.x += halfScreen.x <= screen.x ? edgeShift.x : -edgeShift.x;
            authored.y += halfScreen.y <= screen.y ? edgeShift.y : -edgeShift.y;
        }

        SPRITE_LIST* const spriteList = static_cast<SPRITE_LIST*>(this);
        for (int spriteIndex = 0; spriteIndex<spriteList->m_count; ++spriteIndex)
        {
            SPRITE* sprite = spriteList->m_data[spriteIndex];
            VID* vid = sprite->Vid();
            if (vid->nVid != nvid) continue;

            const VECTOR delta = (sprite->Coor() - authored) - translation;
            if (std::fabs(delta.x) < epsilon && std::fabs(delta.y) < epsilon &&
                std::fabs(delta.z) < epsilon)
            {
                (void) spriteList->DeleteSpriteNumber(spriteIndex);
                --spriteIndex;
            }
        }

        (void) resource.GoNextSub(ResourceType::Sprites);
    }

    resource.Close();
    return 0;
}
