#include "map.h"
#include "engine_globals.h"
#include "script_exec_dispatch.h"
#include "script.h"
#include "mylib.h"
#include "myerror.h"
#include "sprite.h"
#include "sprite_collector.h"
#include "vid.h"
#include "engine.h"
#include "railway.h"
#include "mouse.h"
#include "graph.h"
#include "sound.h"
#include "net.h"
#include "player.h"
#include "registry.h"
#include "profile.h"
#include "crc32.h"
#include "resread.h"
#include "region.h"
#include "zm/Input/zInputMap.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <windows.h>
#include <shellapi.h>

namespace {
int scriptFileSearch = 0;
int scriptSpriteIterator = 0;
int scriptUnitIterator = 0;
unsigned int scriptUnitMask = 0;

}

int __cdecl ScriptExecFunc(int command) { return Map->ExecFunc(command); }

int MAP::PopInt() { return m_script.PopInt(); }
const STRING& MAP::PopStr() { return m_script.PopStr(); }
SPRITE* MAP::PopSprite() { return static_cast<SPRITE*>(m_script.PopObject()); }

void MAP::PushInt(int value) { m_script.PushInt(value); }
void MAP::PushStr(const STRING& value) { m_script.PushStr(value); }
void MAP::PushSprite(SPRITE* sprite, const STRING& tag) { m_script.PushObject(sprite, tag); }

VID* MAP::PopVid(const char* diagnosticContext) {
    const int index = PopInt();
    VID* const resource = Vid(index);
    if (resource == ::EmptyVid && ::Error && diagnosticContext[0] != 0 && zmTemp == 0)
        ::Error->Log("!!!ERROR!!!SCRIPT: Invalid nvid %s %i", diagnosticContext, index);
    return resource;
}

unsigned char __cdecl InterpolateSignedByteChannel(int from, int to, int factor)
{
    if (from >= 128) from -= 254;
    if (to >= 128) to -= 254;
    const int difference = BitsToSigned32(static_cast<unsigned int>(to)-static_cast<unsigned int>(from));
    const int product = MultiplySigned32Wrap(difference, factor);
    return static_cast<unsigned char>(static_cast<unsigned int>(from)+static_cast<unsigned int>(product/255));
}

unsigned int __cdecl InterpolatePackedRGB(unsigned int from, unsigned int to, int factor)
{
    const unsigned int r = InterpolateSignedByteChannel(
        static_cast<int>(from & 0xFFu), static_cast<int>(to & 0xFFu), factor);
    const unsigned int g = InterpolateSignedByteChannel(
        static_cast<int>((from >> 8) & 0xFFu), static_cast<int>((to >> 8) & 0xFFu), factor);
    const unsigned int b = InterpolateSignedByteChannel(
        static_cast<int>((from >> 16) & 0xFFu), static_cast<int>((to >> 16) & 0xFFu), factor);
    return r | (g << 8) | (b << 16);
}

static int ScriptExternTrig1024(int direction, bool cosine)
{
    const ANGLE angle(static_cast<unsigned char>(direction));
    return TruncateFloatToInt32((cosine ? angle.Cos() : angle.Sin())*1024.0f);
}

static inline int ScriptExternToScreenY(const MAP* map, int y, int z)
{
    return TruncateFloatToInt32(map->ToScreenY(static_cast<float>(y), static_cast<float>(z)));
}

static inline void ScriptExternSetCursor(int type)
{
    MOUSE* cursor = Mouse;
    if (type == -1)
    {
        cursor->Disable();
        return;
    }
    if (type == 256)
    {
        cursor->HardwareOn();
        return;
    }
    if (type == 257)
    {
        cursor->HardwareOff();
        return;
    }
    if (cursor->Enabled() == 0u)
        cursor->Enable();
    cursor->ChangeAnimation(type);
}

static inline unsigned int ScriptExternGetGamma(const GAMMA& value)
{
    const unsigned int gamma = value.modulate;
    const unsigned int control = value.add;
    unsigned int result = (gamma>>1)&0x7F7F7F7Fu;
    if ((control&0x000000FFu) != 0u)
    {
        unsigned int lane = (~control)&0x000000FEu;
        lane |= 0x00000100u;
        result |= (lane>>1);
    }
    if ((control&0x0000FF00u) != 0u)
    {
        unsigned int lane = (~control)&0x0000FE00u;
        lane |= 0x00010000u;
        result |= (lane>>1);
    }
    if ((control&0x00FF0000u) != 0u)
    {
        unsigned int lane = (~control)&0x00FE0000u;
        lane |= 0x01000000u;
        result |= (lane>>1);
    }
    if ((control&0xFF000000u) != 0u)
    {
        unsigned int lane = (~control)>>1;
        lane &= 0xFF000000u;
        lane |= 0x80000000u;
        result |= lane;
    }
    return result;
}

int MAP::ExecFunc(int command)
{
    static STRING text;
    SCRIPT* const stack = &m_script;
    SPRITE_COLLECTOR* const world = ::Sprites;
    switch (command)
    {

    case SCRIPT_EXT_CreateSprite:
    {
        SPRITE* const parent = PopSprite();
        const int direction = PopInt();
        const int z = PopInt();
        const int y = PopInt();
        const int x = PopInt();
        VID* const resource = PopVid("for CreateSprite()");
        if (resource == ::EmptyVid) PushInt(0);
        else PushSprite(CreateSprite(resource, VECTOR(float(x), float(y), float(z)),
                                     ANGLE(static_cast<unsigned char>(direction)), parent),
                        STRING("return CreateSprite"));
        break;
    }

    case SCRIPT_EXT_Flagman:
        {
        const STRING tag("return Flagman");
        const int index = PopInt();
        SPRITE* const result = Flagman(static_cast<unsigned int>(index));
        stack->PushObject(result, tag);
        break;
    }

    case SCRIPT_EXT_FirstUnit:
        {
        const int lane = PopInt();
        const unsigned int requiredMask = static_cast<unsigned int>(PopInt());
        scriptUnitMask = requiredMask;
        int* const cursor = &scriptUnitIterator;
        SPRITE* candidate = ::Sprites->FirstUnit(cursor, lane);
        while (candidate)
        {
            if ((candidate->Vid()->spriteType&scriptUnitMask) != 0u)break;
            candidate = ::Sprites->NextUnit(cursor);
        }
        const STRING tag("return FirstUnit");
        stack->PushObject(candidate, tag);
        break;
    }

    case SCRIPT_EXT_NextUnit:
        {
        int* const cursor = &scriptUnitIterator;
        SPRITE* candidate;
        for (;;)
        {
            candidate = ::Sprites->NextUnit(cursor);
            if (!candidate || (candidate->Vid()->spriteType&scriptUnitMask) != 0u)break;
        }
        const STRING tag("return NextUnit");
        stack->PushObject(candidate, tag);
        break;
    }

    case SCRIPT_EXT_GetSprite:
        {
        SPRITE* const previous = PopSprite();
        const int y = PopInt();
        const int x = PopInt();
        const int type = PopInt();
        const STRING tag("return GetSprite");
        SPRITE* const result = ::Sprites->GetSprite(
            type, static_cast<float>(x), static_cast<float>(y), previous);
        stack->PushObject(result, tag);
        break;
    }

    case SCRIPT_EXT_GetSpriteScr:
        {
        const int y = PopInt();
        const int x = PopInt();
        const int type = PopInt();
        const STRING tag("return GetSpriteScr");
        SPRITE_COLLECTOR* const collector = ::Sprites;
        const VECTOR2 screenPoint = {static_cast<float>(x), static_cast<float>(y)};
        SPRITE* const result = collector->GetSpriteScr(type, screenPoint, 0);
        stack->PushObject(result, tag);
        break;
    }

    case SCRIPT_EXT_FindNearestSprite:
        {
        SPRITE* const previous = PopSprite();
        const int radius = PopInt();
        const int y = PopInt();
        const int x = PopInt();
        const int type = PopInt();
        const STRING tag("return FindNearestSprite");
        const VECTOR2 point = {static_cast<float>(x), static_cast<float>(y)};
        const float distance = static_cast<float>(radius);
        SPRITE* const result = ::Sprites->FindNearestSprite(
            type, point, distance, previous);
        stack->PushObject(result, tag);
        break;
    }

    case SCRIPT_EXT_FirstInBox:
        {
        const float maxY = static_cast<float>(PopInt());
        const float maxX = static_cast<float>(PopInt());
        const float minY = static_cast<float>(PopInt());
        const float minX = static_cast<float>(PopInt());
        const STRING tag("return FirstInBox");
        SPRITE* const result = ::Sprites->FirstHashInBox(minX, minY, maxX, maxY);
        stack->PushObject(result, tag);
        break;
    }

    case SCRIPT_EXT_NextInBox:
        {
        const STRING tag("return NextInBox");
        SPRITE* const result = ::Sprites->NextHashInBox();
        stack->PushObject(result, tag);
        break;
    }

    case SCRIPT_EXT_FirstSprite:
    {
        PushSprite(world->FirstActiveSprite(&scriptSpriteIterator), STRING("return FirstSprite"));
        break;
    }

    case SCRIPT_EXT_NextSprite:
    {
        PushSprite(world->NextActiveSprite(&scriptSpriteIterator), STRING("return NextSprite"));
        break;
    }

    case SCRIPT_EXT_Action:
        {
        const unsigned int var3 = static_cast<unsigned int>(PopInt());
        const unsigned int var2 = static_cast<unsigned int>(PopInt());
        const unsigned int var1 = static_cast<unsigned int>(PopInt());
        const unsigned int action = static_cast<unsigned int>(PopInt());
        SPRITE* const unit = PopSprite();
        void* const cursorObject = Mouse;
        if (!unit || unit == cursorObject)
            {
            stack->PushInt(0);
            break;
        }
        if (static_cast<int>(action)<17)
            {
            unit->ChangeAnimation(static_cast<int>(action));
            stack->PushInt(0);
            break;
        }
        if (action == ACT_MOVE || action == ACT_ATTACK || action == ACT_PATROL || action == ACT_MOVE_TO ||
           action == ACT_LINK_ENGINE || action == ACT_CLASH_ENGINE)
            {
            VID* const owner = unit->Vid();
            if (owner->spriteClass == 21u && unit->IsCommand(26u))
                {
                stack->PushInt(0);
                break;
            }
        }
        const unsigned int result = static_cast<unsigned int>(unit->Action(static_cast<int>(action), static_cast<int>(var1), static_cast<int>(var2), static_cast<int>(var3)));
        if (action == ACT_GET_TEXT_DESC)
            {
            stack->PushStr(
                *reinterpret_cast<const STRING*>(U32ToPtr(result)));
            break;
        }
        if (result != 0u && (action == ACT_GET_GOAL || action == ACT_NEXT_ENGINE || action == ACT_LAST_ENGINE || action == ACT_FIRST_ENGINE ||
           action == ACT_GET_LINK || action == ACT_GET_UPLINK))
            {
            const STRING tag("return Action");
            stack->PushObject(U32ToPtr(result), tag);
            break;
        }
        stack->PushInt(result);
        break;
    }

    case SCRIPT_EXT_SizeTo:
    {
        const int y = PopInt();
        const int x = PopInt();
        SPRITE* const unit = PopSprite();
        PushInt(unit ? TruncateFloatToInt32((VECTOR2(float(x), float(y))-VECTOR2(unit->Coor().x, unit->Coor().y)).NearLength()) : 60000);
        break;
    }

    case SCRIPT_EXT_AddCommand:
        {
        const unsigned int var3 = static_cast<unsigned int>(PopInt());
        const unsigned int var2 = static_cast<unsigned int>(PopInt());
        const unsigned int var1 = static_cast<unsigned int>(PopInt());
        const unsigned int action = static_cast<unsigned int>(PopInt());
        SPRITE* const unit = PopSprite();
        if (unit) unit->AddActionAfterStop(0u, action, var1, var2, var3);
        break;
    }

    case SCRIPT_EXT_GetVid:
        {
        SPRITE* unit = PopSprite();
        unsigned int nvid = 0u;
        if (unit)
            {
            VID* const owner = unit->Vid();
            nvid = static_cast<unsigned int>(owner->nVid);
        }
        stack->PushInt(nvid);
        break;
    }

    case SCRIPT_EXT_Destroy:
        {
        SPRITE* unit = PopSprite();
        void* const invalid = Mouse;
        if (unit && unit != invalid)
            delete unit;
        break;
    }

    case SCRIPT_EXT_GetX:
        {
        SPRITE* unit = PopSprite();
        const int x = unit?TruncateFloatToInt32OrMin(unit->X()):0;
        stack->PushInt(x);
        break;
    }

    case SCRIPT_EXT_GetY:
        {
        SPRITE* unit = PopSprite();
        const int y = unit?TruncateFloatToInt32OrMin(unit->Y()):0;
        stack->PushInt(y);
        break;
    }

    case SCRIPT_EXT_GetZ:
        {
        SPRITE* unit = PopSprite();
        const int z = unit?TruncateFloatToInt32OrMin(unit->Z()):0;
        stack->PushInt(z);
        break;
    }

    case SCRIPT_EXT_GetDirection:
        {
        SPRITE* unit = PopSprite();
        const unsigned int direction = unit?static_cast<unsigned int>(unit->Direction().value):0u;
        stack->PushInt(direction);
        break;
    }

    case SCRIPT_EXT_GetAnimation:
        {
        SPRITE* unit = PopSprite();
        const unsigned int animation = unit?static_cast<unsigned int>(unit->Animation()):0u;
        stack->PushInt(animation);
        break;
    }

    case SCRIPT_EXT_DirectionTo:
        {
        const int y = PopInt();
        const int x = PopInt();
        SPRITE* const unit = PopSprite();
        if (!unit)
            {
            stack->PushInt(0);
            break;
        }
        const VECTOR2 target = {static_cast<float>(x), static_cast<float>(y)};
        VECTOR2 delta;
        delta = target - VECTOR2(unit->Coor().x, unit->Coor().y);
        const ANGLE direction(delta);
        stack->PushInt(static_cast<unsigned int>(direction.value));
        break;
    }

    case SCRIPT_EXT_GetCommands:
    {
        SPRITE* const unit = PopSprite();
        if (unit) {
            const STRING actions = unit->GetTextActions(0);
            const STRING items = unit->GetTextItems();
            PushStr(items+actions);
        } else PushStr(STRING(StringGlobals::EmptyText()));
        break;
    }

    case SCRIPT_EXT_SetCommands:
    {
        text = PopStr();
        SPRITE* const unit = PopSprite();
        if (unit) {
            unit->SetTextItems(text);
            if (std::strstr(text.CharPtr(), "\002")) {
                text = text.After("\002");
            }
            unit->SetTextActions(0, text);
        }
        break;
    }

    case SCRIPT_EXT_MenuSpriteUnderCursor:
        {
        PushSprite(m_menu.SelectedSprite(), STRING("return MenuSpriteUnderCursor()"));
        break;
    }

    case SCRIPT_EXT_MenuFind:
    {
        const int y = PopInt();
        const int x = PopInt();
        const int direction = PopInt();
        VID* const resource = PopVid("for MenuFind");
        SPRITE* best = 0;
        if (resource != ::EmptyVid && resource->NoSprites()) {
            const VECTOR2 target{float(x), float(y)};
            for (int i = 0; i < m_menu.m_count; ++i) {
                SPRITE* const item = m_menu.m_data[i];
                if (item->Vid() != resource) continue;
                if (direction != 999999 && item->Vid()->RealDirection(item->Direction().value) != direction &&
                    direction != 999000+int(item->Direction().value)) continue;
                if (best) {
                    const float distance = (item->ScreenXY()-target).NearLength();
                    const float bestDistance = (best->ScreenXY()-target).NearLength();
                    if (!(bestDistance > distance)) continue;
                }
                best = item;
            }
        }
        PushSprite(best, STRING("return MenuFind"));
        break;
    }

    case SCRIPT_EXT_MenuLoad:
        {
        const int referenceWidth = PopInt();
        const STRING* fileName = &PopStr();
        (void)m_menu.Load(*fileName, VECTOR2(0, 0), referenceWidth);
        break;
    }

    case SCRIPT_EXT_MenuRelease:
        {
        const int referenceWidth = PopInt();
        STRING* const scratch = &text;
        *scratch = PopStr();
        const char* const emptyName = StringGlobals::EmptyText();
        MENU* const menu = &m_menu;
        if (*scratch == emptyName)
            menu->DeleteAll();
        else
            (void)menu->DeleteFromFile(*scratch, VECTOR2(0, 0), referenceWidth);
        break;
    }

    case SCRIPT_EXT_MenuNVidUnderCursor:
        {
        stack->PushInt(m_menu.NVidUnderCursor());
        break;
    }

    case SCRIPT_EXT_MenuNDirUnderCursor:
        {
        stack->PushInt(m_menu.NDirUnderCursor());
        break;
    }

    case SCRIPT_EXT_MenuAction:
        {
        const unsigned int var3 = static_cast<unsigned int>(PopInt());
        const unsigned int var2 = static_cast<unsigned int>(PopInt());
        const unsigned int var1 = static_cast<unsigned int>(PopInt());
        const unsigned int action = static_cast<unsigned int>(PopInt());
        const int requestedDirection = PopInt();
        VID* const resource = PopVid("for MenuAction");

        if (resource != ::EmptyVid)
            {
            for (int i = 0;i<m_menu.m_count;++i)
                {
                SPRITE* const child = m_menu.m_data[i];
                if (child->Vid() != resource)continue;
                if (requestedDirection != 999999 &&
                   child->Vid()->RealDirection(child->Direction().value) != requestedDirection)
                    {
                    if (requestedDirection != 999000+static_cast<int>(m_menu.m_data[i]->Direction().value))continue;
                }
                if (static_cast<int>(action)<17)
                    (void)m_menu.m_data[i]->ChangeAnimation(static_cast<int>(action));
                else
                    (void)m_menu.m_data[i]->Action(static_cast<int>(action), static_cast<int>(var1), static_cast<int>(var2), static_cast<int>(var3));
            }
        }
        break;
    }

    case SCRIPT_EXT_MenuCreate:
        {
        const int z = PopInt();
        const int y = PopInt();
        const int yPlusZ = BitsToSigned32(static_cast<unsigned int>(y)+static_cast<unsigned int>(z));
        const int x = PopInt();
        const int requestedDirection = PopInt();
        VID* const resource = PopVid("for MenuCreate");

        if (resource == ::EmptyVid)
            {
            stack->PushInt(0);
            break;
        }
        const STRING tag("return MenuCreate");
        const int shiftedDirection = BitsToSigned32(static_cast<unsigned int>(requestedDirection)<<8);
        const ANGLE direction(static_cast<unsigned char>(shiftedDirection/resource->noDir));
        const float positionZ = static_cast<float>(z);
        const float positionY = static_cast<float>(yPlusZ);
        const float positionX = static_cast<float>(x);
        const VECTOR position(positionX, positionY, positionZ);
        SPRITE* const created = Map->CreateSprite(resource, position, direction, 0);
        stack->PushObject(created, tag);
        break;
    }

    case SCRIPT_EXT_MenuLClick:
        {
        const STRING tag("return MenuLClick");
        const unsigned int result = (m_menu.ControlFlags()&0x01u) != 0u ? PtrToU32(m_menu.SelectedSprite()) : 0u;
        stack->PushObject(U32ToPtr(result), tag);
        break;
    }

    case SCRIPT_EXT_MenuRClick:
        {
        const STRING tag("return MenuRClick");
        const unsigned int result = (m_menu.ControlFlags()&0x02u) != 0u ? PtrToU32(m_menu.SelectedSprite()) : 0u;
        stack->PushObject(U32ToPtr(result), tag);
        break;
    }

    case SCRIPT_EXT_MenuFindNamed:
        {
        const STRING tag("return MenuFindNamed");
        const STRING* name = &PopStr();
        const unsigned int result = PtrToU32(m_menu.SpriteWithName(*name));
        stack->PushObject(U32ToPtr(result), tag);
        break;
    }

    case SCRIPT_EXT_Load:
        {
        const STRING* fileName = &PopStr();
        m_flags |= FLAG_LOAD_REQUESTED;
        m_pendingMapFileName = *fileName;
        break;
    }

    case SCRIPT_EXT_Save:
        {
        const STRING* const source = &PopStr();
        Save(*source);
        break;
    }

    case SCRIPT_EXT_SaveDemo:
        {
        if (m_resource.m_file == 0)
            {
            const STRING* fileName = &PopStr();
            (void)m_resource.OpenForWrite(*fileName, ResourceType::Demo);
        }
        break;
    }

    case SCRIPT_EXT_Exit:
        {
        (void)PopStr();
        (void)::PostMessageA(reinterpret_cast<HWND>(U32ToPtr(m_dialog)), 0x10u, 0u, 0u);
        break;
    }

    case SCRIPT_EXT_Exec:
    {
        const STRING directory(PopStr());
        const STRING parameters(PopStr());
        text = PopStr();
        ::Error->Log("    Exec '%s'", text.CharPtr());
        m_flags |= FLAG_EXEC_REQUESTED;
        (void)::ShellExecuteA(0, 0, text.CharPtr(), parameters.CharPtr(), directory.CharPtr(), SW_SHOW);
        PushInt(0);
        break;
    }

    case SCRIPT_EXT_Pause:
        {
        const int pause = PopInt();
        NET* const session = ::Net;
        NET* const network = session;
        const bool networkSession = (network->m_flags&0x10u) != 0u;
        if (pause != 0)
            {
            if (networkSession)
                m_pendingNetworkPauseCommand = (m_pendingNetworkPauseCommand & ~0xFFu) | 2u;
            else
                PauseOn();
            MOUSE* const cursor = Mouse;
            cursor->ChangeAnimation(0);
            (void)::Sound->PauseSpeech();
        }
            else
            {
            if (networkSession)
                m_pendingNetworkPauseCommand = (m_pendingNetworkPauseCommand & ~0xFFu) | 1u;
            else
                PauseOff();
            (void)::Sound->ResumeSpeech();
        }
        break;
    }

    case SCRIPT_EXT_MessageText:
        {
        const int y = PopInt();
        const int x = PopInt();
        const STRING* messageText = &PopStr();
        const unsigned int playerIndex = m_playerIndex;
        PLAYER* const player = m_players[playerIndex];
        player->PutMessage(*messageText, static_cast<float>(x), static_cast<float>(y));
        break;
    }

    case SCRIPT_EXT_SetPosition:
        {
        const int y = PopInt();
        const int x = PopInt();
        const VECTOR2 position(static_cast<float>(x), static_cast<float>(y));
        SetShiftCoor(position, 0);
        break;
    }

    case SCRIPT_EXT_SetScrollType:
        {
        const int y1 = PopInt();
        const int x1 = PopInt();
        const int y0 = PopInt();
        const int x0 = PopInt();
        const VECTOR2 second(static_cast<float>(x1), static_cast<float>(y1));
        const VECTOR2 first(static_cast<float>(x0), static_cast<float>(y0));
        const int type = PopInt();
        SetScrollType(type, first, second);
        break;
    }

    case SCRIPT_EXT_GetScrollType:
        {
        stack->PushInt(m_scrollMode);
        break;
    }

    case SCRIPT_EXT_SetScrollBox:
        {
        const int y1 = PopInt();
        const int x1 = PopInt();
        const int y0 = PopInt();
        const int x0 = PopInt();
        const float right = static_cast<float>(x1);
        const float bottom = static_cast<float>(y1);
        const float left = static_cast<float>(x0);
        const float top = static_cast<float>(y0);
        m_cameraMaxY = bottom;
        m_cameraMinX = left;
        m_cameraMinY = top;
        m_cameraMaxX = right;
        break;
    }

    case SCRIPT_EXT_SetStateBar:
        {
        const int flag = PopInt();
        const unsigned int playerIndex = m_playerIndex;
        PLAYER* const player = m_players[playerIndex];
        if (flag != 0) player->StateBarOn();
        else player->StateBarOff();
        break;
    }

    case SCRIPT_EXT_GetPreviousMapName:
        {
        stack->PushStr(
            m_previousMapFileName);
        break;
    }

    case SCRIPT_EXT_GetASProtectUserName:
        PushStr(STRING("NotAS"));
        break;

    case SCRIPT_EXT_GetMapName:
        {
        const STRING mapName = FileName();
        PushStr(mapName);
        break;
    }

    case SCRIPT_EXT_SetLocale:
        {
        const STRING* codePage = &PopStr();
        SetKeyboardLocale(*codePage);
        break;
    }

    case SCRIPT_EXT_SetInputType:
        {
        const int type = PopInt();
        m_input.SetInputType(type);
        break;
    }

    case SCRIPT_EXT_IsPaused:
        PushInt((m_flags & FLAG_PAUSED) != 0u);
        break;

    case SCRIPT_EXT_GetInputAction:
        {
        int playerIndex = PopInt();
        const int inputAction = PopInt();
        NET* const session = ::Net;
        unsigned int result = 0u;
        if ((session->m_flags&0x10u) != 0u)
            {
            if (playerIndex<0 || playerIndex >= 8)
                playerIndex = static_cast<int>(m_playerIndex);
            PLAYER* const army = m_players[playerIndex];
            result = army->m_input.GetAction(static_cast<unsigned int>(inputAction));
        }
            else
            {
            result = m_input.GetAction(static_cast<unsigned int>(inputAction));
        }
        stack->PushInt(result);
        break;
    }

    case SCRIPT_EXT_SetInputActionMode:
        {
        const int num = PopInt();
        const int inputAction = PopInt();
        zInputMap::m_bKeyWaitAct = static_cast<unsigned int>(inputAction);
        zInputMap::m_iKeyWaitNum = num;
        break;
    }

    case SCRIPT_EXT_SetCursor:
        {
        ScriptExternSetCursor(PopInt());
        break;
    }

    case SCRIPT_EXT_RemoveInputAction:
        {
        int playerIndex = PopInt();
        const int inputAction = PopInt();
        NET* session = ::Net;
        if ((session->m_flags&0x10u) != 0u)
            {
            if (playerIndex<0 || playerIndex >= 8)
                playerIndex = static_cast<int>(m_playerIndex);
            PLAYER* const army = m_players[playerIndex];
            army->m_input.RemoveEvent(static_cast<unsigned int>(inputAction), 0u);
        }
            else
            {
            m_input.RemoveEvent(static_cast<unsigned int>(inputAction), 0u);
        }
        break;
    }

    case SCRIPT_EXT_SetCursorPosition:
        {
        const int screenY = PopInt();
        const int screenX = PopInt();
        MOUSE* const cursor = Mouse;
        (void)cursor->Action(ACT_CHANGE_COOR, screenX, screenY, 0);
        break;
    }

    case SCRIPT_EXT_SaveInput:
        {
        const STRING* fileName = &PopStr();
        (void)zInputMap::Save(fileName->CharPtr());
        // Falls through.
    }

    case SCRIPT_EXT_LoadInput:
        {
        const STRING* fileName = &PopStr();
        (void)zInputMap::Load(fileName->CharPtr());
        // Falls through.
    }

    case SCRIPT_EXT_SetSpeechVolume:
        {
        ::Sound->VolumeSpeech(PopInt());
        break;
    }

    case SCRIPT_EXT_SetSoundVolume:
        {
        ::Sound->VolumeSound(PopInt());
        break;
    }

    case SCRIPT_EXT_SetMusicVolume:
        {
        ::Sound->VolumeMusic(PopInt());
        break;
    }

    case SCRIPT_EXT_PlaySFX:
        {
        ::Sound->PlaySFX(PopInt(), 0, 0, 1.0f);
        break;
    }

    case SCRIPT_EXT_StopSFX:
        {
        ::Sound->StopSFX(PopInt());
        break;
    }

    case SCRIPT_EXT_StopMusic:
        {
        ::Sound->StopMusic(static_cast<unsigned int>(PopInt()));
        break;
    }

    case SCRIPT_EXT_PlaySFXFromCoor:
    {
        const int y = PopInt();
        const int x = PopInt();
        const VECTOR2 relative = ToScreen(VECTOR2(float(x), float(y))-Graph->SizeXY()*0.5f);
        const int sfx = PopInt();
        ::Sound->PlaySFXFromCoor(sfx, relative, 1.0f);
        break;
    }

    case SCRIPT_EXT_PlayMusicFile:
        {
        const int changeTime = PopInt();
        const int loop = PopInt();
        const STRING* fileName = &PopStr();
        ::Sound->FadeAndPlayFile(*fileName, loop, static_cast<unsigned long>(changeTime));
        break;
    }

    case SCRIPT_EXT_Effect:
        {
        const int duration = PopInt();
        const int var2 = PopInt();
        const int var1 = PopInt();
        const int effect = PopInt();
        Graph->Effect(effect, var1, var2, duration);
        break;
    }

    case SCRIPT_EXT_SetEnvironment:
        {
        Graph->SetEnvironment(static_cast<unsigned int>(PopInt()));
        break;
    }

    case SCRIPT_EXT_SetGraphDetail:
        {
        stack->PushInt(Graph->SetGraphDetail(PopInt()));
        break;
    }

    case SCRIPT_EXT_SetGamma:
        {
        const unsigned int packed = static_cast<unsigned int>(PopInt());
        const GAMMA gamma(static_cast<GAMMA::GAMMA_CREATE>(0), packed);
        Graph->SetGamma(gamma);
        break;
    }

    case SCRIPT_EXT_SetWind:
        {
        const unsigned int direction = static_cast<unsigned int>(static_cast<unsigned char>(PopInt()));
        const int wind = PopInt();
        Graph->SetWind(wind, ANGLE(static_cast<unsigned char>(direction)));
        break;
    }

    case SCRIPT_EXT_PlayMovie:
        {
        Graph->PlayMovie(PopStr());
        break;
    }

    case SCRIPT_EXT_IsPlayMovie:
        {
        stack->PushInt((Graph->movie.graphBuilder != 0));
        break;
    }

    case SCRIPT_EXT_StopMovie:
        {
        Graph->movie.Release();
        break;
    }

    case SCRIPT_EXT_IsPlayMusic:
        {
        stack->PushInt(::Sound->IsPlayMusic());
        break;
    }

    case SCRIPT_EXT_CountGamma:
        {
        const int time = PopInt();
        const unsigned int gamma2 = static_cast<unsigned int>(PopInt());
        const unsigned int gamma1 = static_cast<unsigned int>(PopInt());
        stack->PushInt(InterpolatePackedRGB(gamma1, gamma2, time));
        break;
    }

    case SCRIPT_EXT_GetGamma:
        {
        stack->PushInt(ScriptExternGetGamma(Graph->gamma));
        break;
    }

    case SCRIPT_EXT_GetEffectState:
        {
        stack->PushInt(Graph->GetEffectState(PopInt()));
        break;
    }

    case SCRIPT_EXT_ScreenX:
        {
        const GRAPH* const graph = Graph;
        stack->PushInt(TruncateFloatToInt32OrMin(graph->screenWidth));
        break;
    }

    case SCRIPT_EXT_ScreenY:
        {
        const GRAPH* const graph = Graph;
        stack->PushInt(TruncateFloatToInt32OrMin(graph->screenHeight));
        break;
    }

    case SCRIPT_EXT_SetGraphScreen:
        {
        const int sizeY = PopInt();
        const int sizeX = PopInt();
        Graph->SetScreenSize(sizeX, sizeY);
        break;
    }

    case SCRIPT_EXT_GetGraphDetail:
        {
        const GRAPH* const graph = Graph;
        PushInt(graph->graphDetail);
        break;
    }

    case SCRIPT_EXT_PlaySpeech:
        {
        const STRING& source = PopStr();
        (void)::Sound->PlaySpeech(source);
        break;
    }

    case SCRIPT_EXT_StopSpeech:
        {
        ::Sound->StopSpeech();
        break;
    }

    case SCRIPT_EXT_CanSetScreenSize:
    {
        const int height = PopInt();
        const int width = PopInt();
        PushInt(Graph->CanSetScreenSize(width, height));
        break;
    }

    case SCRIPT_EXT_Log:
        {
        const STRING* const source = &PopStr();
        STRING formatted = "    "+(*source);

        MYERROR* const logger = ::Error;
        logger->Log(STRING(formatted.CharPtr()));
        break;
    }

    case SCRIPT_EXT_Random_:
        {
        stack->PushInt(Random(PopInt()));
        break;
    }

    case SCRIPT_EXT_ChangeZUnit:
        {
        const int z = PopInt();
        VID* const owner = PopVid("for ChangeZUnit");
        VID* const invalid = ::EmptyVid;
        if (owner != invalid)
            {
            SPRITE_COLLECTOR* const manager = ::Sprites;
            int cursor = 0;
            SPRITE* unit = manager->FirstActiveSprite(&cursor);
            while (unit != 0)
                {
                if (unit->Vid() == owner)
                    unit->ChangeZCoor(static_cast<float>(z));
                unit = manager->NextActiveSprite(&cursor);
            }
        }
        break;
    }

    case SCRIPT_EXT_GetTime:
        {
        PushInt(::CurrentTime);
        break;
    }

    case SCRIPT_EXT_GetGroundZ:
        {
        const int y = PopInt();
        const int x = PopInt();
        const VECTOR2 point = {static_cast<float>(x), static_cast<float>(y)};
        const float height = GetGroundZ(point);
        stack->PushInt(TruncateFloatToInt32(height));
        break;
    }

    case SCRIPT_EXT_CanPlace:
        {
        const int z = PopInt();
        const int y = PopInt();
        const int x = PopInt();
        const VID* const owner = PopVid("for CanPlace");
        const STRING tag("return CanPlace");
        const ANGLE direction(0);
        SPRITE_COLLECTOR* const collector = ::Sprites;
        SPRITE* const result = collector->CanPlace(owner, static_cast<float>(x), static_cast<float>(y),
            static_cast<float>(z), direction);
        PushSprite(result, tag);
        break;
    }

    case SCRIPT_EXT_GetVidData:
        {
        const int type = PopInt();
        const char* const diagnostic = (type == 0)
            ? StringGlobals::EmptyText()
            : "for GetVid";
        VID* const object = PopVid(diagnostic);
        VID* const invalid = ::EmptyVid;
        if (object == invalid)
            {
            stack->PushInt(0);
            break;
        }
        const VID* const vid = static_cast<const VID*>(object);

        if (type >= 160 && type<177)
            {
            VID* const child = vid->childVid[type-160];
            stack->PushInt(child ? child->nVid : 0);
            break;
        }
        if (type >= 192 && type<209)
            {
            stack->PushInt(vid->noChild[type-192]);
            break;
        }

        if (type >= 260 && type < 277)
        {
            PushInt(TruncateFloatToInt32OrMin(vid->childX[type - 260]));
            break;
        }
        if (type >= 10 && type < 18)
        {
            PushInt(ScriptExternGetGamma(vid->gammaByPlayer[type - 10]));
            break;
        }

        switch (type)
            {
        case 0:

            stack->PushInt(1);
            break;
        case 3:
                {
            WEAPON_RUNTIME* const runtime = static_cast<VID*>(object)->weapon;
            stack->PushInt(TruncateFloatToInt32OrMin(runtime->BattleRange));
            break;
        }
        case 7:
            PushStr(vid->name);
            break;
        case 29:
            stack->PushInt(static_cast<const VID*>(object)->Killed());
            break;
        case 30:
            case 31: case 32: case 33: case 34: case 35: case 36: case 37:
            stack->PushInt(
                static_cast<const VID*>(object)->Killed(static_cast<unsigned int>(type-30)));
            break;
        case 49:
            stack->PushInt(static_cast<const VID*>(object)->NoSprites());
            break;
        case 50:
            case 51: case 52: case 53: case 54: case 55: case 56: case 57:
            stack->PushInt(
                static_cast<VID*>(object)->NoSprites(type-50));
            break;
        case 69:
            stack->PushInt(vid->maxHp);
            break;
        case 70:
            case 71: case 72: case 73: case 74: case 75: case 76: case 77:
            stack->PushInt(
                static_cast<const VID*>(object)->GetMaxHp(static_cast<unsigned int>(type-70)));
            break;
        case 110:
            stack->PushInt(vid->spriteType);
            break;
        case 111:
            stack->PushInt(vid->spriteClass);
            break;
        case 112:
            case 114:
                {
            const float value = (type == 112)?vid->maxSpeed:vid->maxZSpeed;
            if (FloatEqualOrUnordered(value, 999999.0f)) stack->PushInt(999999);
            else stack->PushInt(TruncateFloatToInt32(value*1000.0f));
            break;
        }
        case 113:
            stack->PushInt(vid->lifeTime);
            break;
        case 117:
            stack->PushInt(vid->noDir);
            break;
        case 118:
            stack->PushInt(vid->moveMask);
            break;
        case 119:
                {
            WEAPON_RUNTIME* const runtime = static_cast<VID*>(object)->weapon;
            stack->PushInt(runtime->BuildTime);
            break;
        }
        case 120:
            stack->PushInt(
                static_cast<const VID*>(object)->PropHide());
            break;
        case 121:
            stack->PushInt(
                static_cast<const VID*>(object)->PropNotCreateAsChild());
            break;
        case 122:
            stack->PushInt(vid->frameSpeedDefault);
            break;
        case 123:
                {
            stack->PushInt(vid->linkVid ? vid->linkVid->nVid : 0);
            break;
        }
        case 124:
            stack->PushInt(vid->deathDamageMin);
            break;
        case 125:
            stack->PushInt(vid->deathDamageMax);
            break;
        case 126:
            case 127:
                {
            WEAPON_RUNTIME* const runtime = static_cast<VID*>(object)->weapon;
            const float parameter = (type == 126)?runtime->aimRange:runtime->DetectRange;
            stack->PushInt(TruncateFloatToInt32OrMin(parameter));
            break;
        }
        case 128:
            stack->PushInt(static_cast<const VID*>(object)->GetMaxAmmo());
            break;
        case 130:
            PushInt(vid->weapon->fullReloadTime);
            break;
        case 131:
            PushInt(vid->weapon->reloadTime);
            break;
        case 134:
            PushInt(TruncateFloatToInt32OrMin(vid->deathRange));
            break;
        case 132:
            stack->PushInt((vid->runtimeFlags>>12u)&1u);
            break;
        case 133:
            object->SetFramesToDevice();
            stack->PushInt(1);
            break;
        case 229:
            stack->PushInt(static_cast<const VID*>(object)->ReCaptured());
            break;
        case 230:
            case 231: case 232: case 233: case 234: case 235: case 236: case 237:
            stack->PushInt(
                static_cast<const VID*>(object)->ReCaptured(static_cast<unsigned int>(type-230)));
            break;
        case 238:
                {
            stack->PushInt(vid->sourceVid ? vid->sourceVid->nVid : 0);
            break;
        }
        case 239:
            case 240: case 241:
                {
            const float value = (type == 239)?vid->sizeXYZ.x:(type == 240)?vid->sizeXYZ.y:vid->sizeXYZ.z;
            stack->PushInt(TruncateFloatToInt32OrMin(value));
            break;
        }
        default:
                {
            STRING label;
            label = "GetVid type";
            Error(E_UNKNOWN, label, static_cast<unsigned int>(type));
            break;
        }
        }
        break;
    }

    case SCRIPT_EXT_SetVidData:
        {
        const int value = PopInt();
        const int type = PopInt();
        VID* const object = PopVid(
            "for SetVid");
        VID* const invalid = ::EmptyVid;
        if (object == invalid) break;
        VID* const vid = static_cast<VID*>(object);

        switch (type)
            {
        case 3:
            vid->weapon->BattleRange = static_cast<float>(value);
            break;
        case 29:
            for (unsigned int i = 0; i < VID::PlayerSlotCount; ++i) vid->killedByPlayer[i] = value;
            break;
        case 30:
            case 31: case 32: case 33: case 34: case 35: case 36: case 37:
            vid->killedByPlayer[type-30] = value;
            break;
        case 69:
            vid->SetMaxHp(value);
            break;
        case 70:
            case 71: case 72: case 73: case 74: case 75: case 76: case 77:
            static_cast<VID*>(object)->SetMaxHp(static_cast<unsigned int>(type-70), value);
            break;
        case 90:
            case 91: case 92: case 93: case 94: case 95: case 96: case 97:
            static_cast<VID*>(object)->SetHpCoeff(static_cast<unsigned int>(type-90), value);
            break;
        case 112:
                {
            const float scale = (value == 999999)
                ? (999999.0f)
                : static_cast<float>(value)/1000.0f;
            vid->maxSpeed = scale;
            vid->randomSpeed = scale;
            int cursor = 0;
            SPRITE* unit = world->FirstActiveSprite(&cursor);
            while (unit)
                    {
                if (unit->Vid() == object)
                        {
                    EX_SPRITE_DATA* const linked = unit->ExData();
                    if (linked)
                            {
                        EX_SPRITE_DATA* const linkedAgain = unit->ExData();
                        linkedAgain->maxSpeedOverride = vid->maxSpeed;
                    }
                }
                unit = world->NextActiveSprite(&cursor);
            }
            break;
        }
        case 113:
            if (vid->nVid != 0)
                {
                vid->lifeTime = value;
                vid->actionAuxStateRequired = 1;
            }
            break;
        case 114:
                {
            const float scale = (value == 999999)
                ? (999999.0f)
                : static_cast<float>(value)/1000.0f;
            vid->maxZSpeed = scale;
            break;
        }
        case 116:
                {
            if (ValidateVid(value) != 0u)
                    {
                VID* const source = Vid(value);
                ExchangeVid(static_cast<VID*>(object), source);
            }
                    else
                    {
                STRING label;
                label = "SetVid exchangevid";
                Error(E_INVALID, label, static_cast<unsigned int>(value));
            }
            break;
        }
        case 118:
            vid->moveMask = static_cast<unsigned int>(value);
            break;
        case 119:
                {
            WEAPON_RUNTIME* const runtime = static_cast<VID*>(object)->weapon;
            runtime->BuildTime = static_cast<unsigned int>(value);
            break;
        }
        case 120:
            static_cast<VID*>(object)->SetPropHideWithoutLink(value);
            break;
        case 121:
            static_cast<VID*>(object)->SetPropNotCreateAsChild(value);
            break;
        case 122:
            vid->frameSpeedDefault = static_cast<unsigned short>(value);
            for (unsigned int i = 0;i<VID::AnimationSlotCount;++i) vid->frameSpeed[i] = value;
            break;
        case 123:
            if (ValidateVid(value) != 0u)
                vid->linkVid = Vid(value);
            else vid->linkVid = 0;
            break;
        case 124:
            vid->deathDamageMin = value;
            break;
        case 125:
            vid->deathDamageMax = value;
            break;
        case 126:
                {
            WEAPON_RUNTIME* const runtime = static_cast<VID*>(object)->weapon;
            runtime->aimRange = static_cast<float>(value);
            break;
        }
        case 127:
                {
            WEAPON_RUNTIME* const runtime = static_cast<VID*>(object)->weapon;
            runtime->DetectRange = static_cast<float>(value);
            break;
        }
        case 128:
                {
            VID* const vidObject = static_cast<VID*>(object);
            WEAPON_RUNTIME* runtime = vidObject->weapon;
            VID* const linked = vidObject->linkVid;
            if (linked && linked->CanFight() != 0) runtime = linked->weapon;
            runtime->MaxAmmo = value;
            break;
        }
        case 130:
            vid->weapon->fullReloadTime = static_cast<DWORD>(TruncateFloatToInt32OrMin(static_cast<float>(value)));
            break;
        case 131:
            vid->weapon->reloadTime = static_cast<DWORD>(TruncateFloatToInt32OrMin(static_cast<float>(value)));
            break;
        case 134:
            vid->deathRange = static_cast<float>(value);
            break;
        case 229:
                {
            COLOR color;
            color.value = static_cast<unsigned int>(value);
            vid->SetReColorForArmy(color);
            break;
        }
        case 238:
            if (ValidateVid(value) != 0u)
                {
                VID* const source = Vid(value);
                CopyVid(static_cast<VID*>(object), source);
            }
                else
                {
                STRING label;
                label = "SetVid copy_vid";
                Error(E_INVALID, label, static_cast<unsigned int>(value));
            }
            break;
        case 239:
            vid->sizeXYZ.x = static_cast<float>(value);
            break;
        case 240:
            vid->sizeXYZ.y = static_cast<float>(value);
            break;
        case 241:
            vid->sizeXYZ.z = static_cast<float>(value);
            break;
        default:
                {
            if (type >= 160 && type<177)
                    {
                const unsigned int id = (value<0)?0u-static_cast<unsigned int>(value):static_cast<unsigned int>(value);
                const unsigned int slot = static_cast<unsigned int>(type-160);
                if (value == 0)
                        {
                    vid->nChildVid[slot] = 0;
                    vid->childVid[slot] = 0;
                    break;
                }
                if (ValidateVid((int)id) != 0u)
                        {
                    vid->nChildVid[slot] = value;
                    VID* const child = Vid((int)id);
                    vid->childVid[slot] = child;
                    if (child->PropBirthAsSmoke() != 0)
                        vid->actionAuxStateRequired |= 1u;
                }
                        else
                        {
                    STRING label;
                    label = "SetVid child";
                    Error(E_INVALID, label, static_cast<unsigned int>(value));
                }
                break;
            }
            if (type >= 10 && type<18)
                    {
                const GAMMA gamma(static_cast<GAMMA::GAMMA_CREATE>(0), static_cast<unsigned long>(value));
                vid->SetGamma(gamma, type-10);
                break;
            }
            if (type >= 192 && type<209)
                    {
                vid->noChild[type-192] = value;
                break;
            }
            if (type >= 260 && type < 277)
            {
                vid->childX[type - 260] = static_cast<float>(value);
                break;
            }
            STRING label;
            label = "SetVid type";
            Error(E_UNKNOWN, label, static_cast<unsigned int>(type));
            break;
        }
        }
        break;
    }

    case SCRIPT_EXT_Sin:
        {
        stack->PushInt(ScriptExternTrig1024(PopInt(), false));
        break;
    }

    case SCRIPT_EXT_Cos:
        {
        stack->PushInt(ScriptExternTrig1024(PopInt(), true));
        break;
    }

    case SCRIPT_EXT_MapSizeX:
        {
        const VECTOR2& mapSize = SizeXY();
        PushInt(TruncateFloatToInt32OrMin(mapSize.x));
        break;
    }

    case SCRIPT_EXT_MapSizeY:
        {
        const VECTOR2& mapSize = SizeXY();
        PushInt(TruncateFloatToInt32OrMin(mapSize.y));
        break;
    }

    case SCRIPT_EXT_Genocide:
        {
        VID* const owner = PopVid("for Genocide");
        VID* const invalid = ::EmptyVid;
        if (owner != invalid)
            {
            SPRITE_COLLECTOR* const manager = ::Sprites;
            int cursor = 0;
            SPRITE* unit = manager->FirstActiveSprite(&cursor);
            while (unit != 0)
                {
                if (unit->Vid() == owner)
                    delete unit;
                unit = manager->NextActiveSprite(&cursor);
            }
        }
        break;
    }

    case SCRIPT_EXT_ReplaceUnit:
        {
        VID* const replacementOwner = PopVid(
            "for Replace Unit 2");
        VID* const sourceOwner = PopVid(
            "for Replace Unit 1");
        VID* const invalid = ::EmptyVid;
        if (replacementOwner == invalid || sourceOwner == invalid)break;
        SPRITE_COLLECTOR* const manager = ::Sprites;
        int cursor = 0;
        SPRITE* unit = manager->FirstActiveSprite(&cursor);
        while (unit)
            {
            if (unit->Vid() == sourceOwner)
                {
                const unsigned char direction = unit->Direction().value;
                const VECTOR& position = unit->Coor();
                (void)this->CreateSprite(static_cast<VID*>(replacementOwner), position, ANGLE(static_cast<unsigned char>(direction)), 0);
                delete unit;
            }
            unit = manager->NextActiveSprite(&cursor);
        }
        break;
    }

    case SCRIPT_EXT_ReloadVid:
        {
        ReloadVid();
        break;
    }

    case SCRIPT_EXT_ToScreenX:
        {
        const float projected = ToScreenX(static_cast<float>(PopInt()));
        stack->PushInt(TruncateFloatToInt32(projected));
        break;
    }

    case SCRIPT_EXT_ToScreenY:
        {
        const int z = PopInt();
        const int y = PopInt();
        stack->PushInt(ScriptExternToScreenY(this, y, z));
        break;
    }

    case SCRIPT_EXT_Random2:
        {
        stack->PushInt(Random2(PopInt()));
        break;
    }

    case SCRIPT_EXT_IsLineUnderGround:
        {
        const int z1 = PopInt();
        const int y1 = PopInt();
        const int x1 = PopInt();
        const int z0 = PopInt();
        const int y0 = PopInt();
        const int x0 = PopInt();
        const VECTOR from(x0, y0, z0);
        const VECTOR to(x1, y1, z1);
        stack->PushInt(IsLineUnderGround(from, to));
        break;
    }

    case SCRIPT_EXT_GetDateTimeString:
        {
        STRING timeText = GetTimeString();
        STRING dateText = GetDateString();
        STRING withSpace = dateText+" ";
        STRING result = withSpace+timeText;
        PushStr(result);
        break;
    }

    case SCRIPT_EXT_FWrite:
        {
        const STRING* source = &PopStr();
        STRING* const scratch = &text;
        *scratch = *source;
        const unsigned int file = static_cast<unsigned int>(PopInt());
        if (file != 0u)
            {
            scratch->Write(reinterpret_cast<std::FILE*>(U32ToPtr(file)));
            (void)static_cast<unsigned int>(std::fseek(reinterpret_cast<std::FILE*>(U32ToPtr(file)), BitsToSigned32(0xFFFFFFFFu), static_cast<int>(1u)));
            (void)static_cast<unsigned int>(std::fputs("\n", reinterpret_cast<std::FILE*>(U32ToPtr(file))));
        }
        break;
    }

    case SCRIPT_EXT_FRead:
        {
        const unsigned int file = static_cast<unsigned int>(PopInt());
        STRING* const scratch = &text;
        const unsigned int flags = m_flags;
        if ((flags&FLAG_DEMO_PLAYBACK) != 0u)
            scratch->Read(static_cast<STREAM*>(&m_resource));
        else if (file != 0u)
            scratch->Read(reinterpret_cast<std::FILE*>(U32ToPtr(file)));
        if ((m_flags&FLAG_DEMO_RECORDING) != 0u)
            scratch->Write(static_cast<STREAM*>(&m_resource));
        PushStr(*scratch);
        break;
    }

    case SCRIPT_EXT_FOpen:
        {
        const STRING* source = &PopStr();
        STRING* const scratch = &text;
        *scratch = *source;
        if ((m_flags&FLAG_DEMO_PLAYBACK) != 0u)
            {
            PushInt(0);
            break;
        }
        STRING mode;
        mode = "rt+";
        const unsigned int file = PtrToU32(FOpen(*scratch, mode));
        if (file == 0u)Error(E_OPEN, *scratch, 0u);
        PushInt(file);
        break;
    }

    case SCRIPT_EXT_FClose:
        {
        const unsigned int file = static_cast<unsigned int>(PopInt());
        if (file != 0u)
            (void)static_cast<unsigned int>(std::fclose(reinterpret_cast<std::FILE*>(U32ToPtr(file))));
        break;
    }

    case SCRIPT_EXT_FCreate:
        {
        const STRING* source = &PopStr();
        STRING* const scratch = &text;
        *scratch = *source;
        if ((m_flags&FLAG_DEMO_PLAYBACK) != 0u)
            {
            PushInt(0);
            break;
        }
        STRING mode;
        mode = "wt+";
        const unsigned int file = PtrToU32(FOpen(*scratch, mode));
        PushInt(file);
        break;
    }

    case SCRIPT_EXT_FEof:
        {
        const unsigned int file = static_cast<unsigned int>(PopInt());
        const unsigned int value = file == 0u ? 1u : (std::feof(reinterpret_cast<std::FILE*>(U32ToPtr(file)))?0x10u:0u);
        PushInt(value);
        break;
    }









    case SCRIPT_EXT_FirstFile:
        {
        const STRING* fileMask = &PopStr();
        STRING result = FFirstFile(
            &scriptFileSearch, *fileMask, 0);
        PushStr(result);
        break;
    }

    case SCRIPT_EXT_NextFile:
        {
        STRING result = FNextFile(
            &scriptFileSearch, 0);
        PushStr(result);
        break;
    }

    case SCRIPT_EXT_GetWindowsUserPath:
        PushStr(WindowsUserPath);
        break;

    case SCRIPT_EXT_charat:
        {
        const int index = PopInt();
        const STRING* source = &PopStr();
        STRING* const scratch = &text;
        *scratch = *source;
        const signed char value = static_cast<signed char>((*scratch)[index]);
        stack->PushInt(static_cast<int>(value));
        break;
    }

    case SCRIPT_EXT_itoa:
        {
        const int value = PopInt();
        STRING numberText = Int2Str(value);
        stack->PushStr(numberText);
        break;
    }

    case SCRIPT_EXT_Printf:
        {
        if (stack->IsLastStackString() != 0)
            {
            STRING value;
            value = PopStr();
            const STRING* format = &PopStr();
            const STRING formatted = Printf(format->value, value.value);
            PushStr(formatted);
        }
            else
            {
            const int value = PopInt();
            const STRING* format = &PopStr();
            const STRING formatted = Printf(format->value, value);
            PushStr(formatted);
        }
        break;
    }

    case SCRIPT_EXT_strlen:
        {
        const STRING* source = &PopStr();
        stack->PushInt(source->Length());
        break;
    }

    case SCRIPT_EXT_ToLower:
        {
        const STRING& source = PopStr();
        char* const converted = ::CharLowerA(source.value);
        STRING temporary;
        temporary = converted;
        PushStr(temporary);
        break;
    }

    case SCRIPT_EXT_ToUpper:
        {
        const STRING& source = PopStr();
        char* const converted = ::CharUpperA(source.value);
        STRING temporary;
        temporary = converted;
        PushStr(temporary);
        break;
    }

    case SCRIPT_EXT_ToBase64:
        {
        const int addCode = PopInt();
        const STRING* source = &PopStr();
        STRING encoded = source->ToBase64(addCode);
        PushStr(encoded);
        break;
    }

    case SCRIPT_EXT_IsAlpha:
        {
        const unsigned int value = static_cast<unsigned int>(::IsCharAlphaA(static_cast<char>(PopInt())));
        PushInt(value);
        break;
    }

    case SCRIPT_EXT_GetString:
        {
        STRING* const keyword = &text;
        *keyword = PopStr();
        STRING fallback;
        fallback = StringGlobals::EmptyText();
        const STRING* section = &PopStr();
        PROFILE* strings = ::Strings;
        STRING result = strings->GetString(*section, *keyword, fallback);
        PushStr(result);
        break;
    }

    case SCRIPT_EXT_Crc32:
        {
        const STRING* source = &PopStr();
        STRING* const scratch = &text;
        *scratch = *source;
        const unsigned int length = static_cast<unsigned int>(scratch->Length());
        const CRC32 crc(scratch->value, length);
        PushInt(crc.value);
        break;
    }

    case SCRIPT_EXT_StrBefore:
        {
        const STRING* delimiter = &PopStr();
        STRING* const scratch = &text;
        *scratch = *delimiter;
        const STRING result = PopStr().Before(*scratch);
        PushStr(result);
        break;
    }

    case SCRIPT_EXT_StrAfter:
        {
        const STRING* delimiter = &PopStr();
        STRING* const scratch = &text;
        *scratch = *delimiter;
        const STRING result = PopStr().After(*scratch);
        PushStr(result);
        break;
    }

    case SCRIPT_EXT_StrSubStr:
        {
        const int end = PopInt();
        const int begin = PopInt();
        STRING* const scratch = &text;
        *scratch = PopStr();
        const STRING result = scratch->SubString(
            static_cast<unsigned int>(begin), static_cast<unsigned int>(end));
        PushStr(result);
        break;
    }

    case SCRIPT_EXT_GetRelation:
        {
        const int army2 = PopInt();
        const int army1 = PopInt();
        PLAYER* const army = Player(army1);
        stack->PushInt(army->RelationTo(static_cast<unsigned int>(army2)));
        break;
    }

    case SCRIPT_EXT_SetRelation:
        {
        const int relation = PopInt();
        const int army2 = PopInt();
        const int army1 = PopInt();
        PLAYER* const army = Player(army1);
        army->SetRelationTo(static_cast<unsigned int>(army2), relation);
        break;
    }

    case SCRIPT_EXT_GetMoney:
        {
        const int armyIndex = PopInt();
        PLAYER* const army = Player(armyIndex);
        stack->PushInt(army->GetMoney());
        break;
    }

    case SCRIPT_EXT_SetMoney:
        {
        const int money = PopInt();
        const int armyIndex = PopInt();
        PLAYER* const army = Player(armyIndex);
        army->SetMoney(money);
        break;
    }

    case SCRIPT_EXT_SetFlagman:
    {
        SPRITE* const sprite = PopSprite();
        const int army = PopInt();
        SetFlagman(static_cast<unsigned int>(army), sprite);
        break;
    }

    case SCRIPT_EXT_SetPlayerControl:
        {
        const int controlType = PopInt();
        const int armyIndex = PopInt();
        PLAYER* const army = m_players[armyIndex];
        army->SetPlayerControl(controlType);
        break;
    }

    case SCRIPT_EXT_NetProperty:
    {
        text = PopStr();
        (void)PopInt();
        PushInt(0);
        break;
    }

    case SCRIPT_EXT_NetSend:
        text = PopStr();
        (void)PopInt();
        break;

    case SCRIPT_EXT_NetReceive:
        (void)PopInt();
        (void)PopInt();
        text = "";
        PushStr(text);
        break;

    case SCRIPT_EXT_TrainProperty:
        {

        const int selector = PopInt();
        SPRITE* const unit = PopSprite();
        if (unit == 0 ||
           unit->IsSpriteClass(21u) == 0)
            {
            stack->PushInt(0);
            break;
        }

        TRAIN_INFO stats(static_cast<const ENGINE*>(unit));
        switch (selector)
            {
        case 1:
            stack->PushInt(stats.maxSpeedThousandths);
        break;
        case 2:
            stack->PushInt(stats.fireDamage);
        break;
        case 3:
            stack->PushInt(static_cast<unsigned int>(
                MultiplySigned32Wrap(stats.currentHp, 100)/stats.maxHp));
            break;
        case 4:
            stack->PushInt(stats.currentHp);
        break;
        case 5:
            stack->PushInt(stats.averageAmmoPercent);
        break;
        case 6:
            stack->PushInt(stats.Acceleration());
        break;
        case 7:
            stack->PushInt(stats.buildTime);
        break;
        case 8:
            stack->PushInt(0);
        break;
        case 9:
                {
            for (ENGINE* node = static_cast<ENGINE*>(unit)->FirstEngine();node;node = node->NextEngine())
                    {
                const unsigned int value = static_cast<const SPRITE*>(node)->IsCommand(0u);
                if (value == 0u)stack->PushInt(value);
            }
            stack->PushInt(1);
            break;
        }
        case 10:
            stack->PushInt(stats.ammoCount);
        break;
        case 11:
            stack->PushInt(stats.ammoCapacity);
        break;
        default:
            stack->PushInt(0);
        break;
        }
        break;
    }









    default:
        ::Error->Log("!!!ERROR!!!LOGIC: Unknown extern Function %i", command);
        break;
    }
    return 0;
}
