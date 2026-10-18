#pragma once

#include "player.h"
#include "message.h"

class PLAYER_ARCADE : public PLAYER
{
public:
    explicit PLAYER_ARCADE(unsigned int playerIndex);
    ~PLAYER_ARCADE() noexcept(false) override;
    void InitForLoadMap() override;
    void DeletePointerToSprite(SPRITE* sprite) override;
    void PutMessage(const STRING& text, float x, float y) override;
    void Control(INPUT* input) override;

private:
    MESSAGE m_message;
};
