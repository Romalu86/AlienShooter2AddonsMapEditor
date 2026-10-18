#include "player_arcade.h"
#include "graph.h"
#include "map.h"
#include "engine_globals.h"

namespace
{

unsigned int requestedVidChange;
}

PLAYER_ARCADE::PLAYER_ARCADE(unsigned int playerIndex)
    : PLAYER(playerIndex), m_message(4, -1, 398.0f, 388.0f, 5, 5000)
{
    m_message.m_baseX = Graph->screenWidth - 242.0f;
    m_message.m_baseY = Graph->screenHeight - 92.0f;
    m_message.m_rowShift = 1;
}

PLAYER_ARCADE::~PLAYER_ARCADE() noexcept(false) = default;

void PLAYER_ARCADE::InitForLoadMap()
{
}

void PLAYER_ARCADE::DeletePointerToSprite(SPRITE* sprite)
{
    m_message.DeletePointerToSprite(sprite);
    PLAYER::DeletePointerToSprite(sprite);
}

void PLAYER_ARCADE::PutMessage(const STRING& text, float x, float y)
{
    m_message.Put(text, x, y);
}

void PLAYER_ARCADE::Control(INPUT* input)
{
    if (m_controlPauseState || m_controlMode == 0) return;
    GroupsTact();
    m_message.Tact();
    if (!Flagman() || !Flagman()->IsSpriteClass(7)) return;

    ControlFlagman(input);
    if (input->GetAction(72) == '[') requestedVidChange = 1;
    if (input->GetAction(72) == ']') requestedVidChange = 2;
    SPRITE* const child = Flagman()->ChildChain();
    if (!child) return;
    if (child->Animation() == 8 && !(child->RuntimeFlags() & 0x10000u)) return;
    if (requestedVidChange == 1) Map->CopyVid(Map->Vid(251), Map->Vid(350));
    if (requestedVidChange == 2) Map->CopyVid(Map->Vid(251), Map->Vid(351));
    requestedVidChange = 0;
}
