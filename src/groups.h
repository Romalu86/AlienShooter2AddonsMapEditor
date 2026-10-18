#pragma once
#include "sprite_list.h"

class SPRITE;
class GROUPS;

class GROUP : public SPRITE_LIST
{
public:
    GROUP(GROUP* owner, SPRITE* sprite);
    ~GROUP() override;

    void Draw() const;
    void DrawNumber(int number) const;
    float DistanceTo(const SPRITE* sprite) const;
    void Insert(SPRITE* sprite);

    float m_centerX;
    float m_centerY;
    unsigned int m_018;
    unsigned int m_01C;
    GROUP* m_next;
};

class RESOURCE;

class GROUPS : public GROUP
{
public:
    GROUPS();
    ~GROUPS() override;

    void Save(RESOURCE* resource) const;
    void Load(RESOURCE* resource);
    void DeletePointerToSprite(SPRITE* sprite);
    void DrawNumber() const;
    GROUP* First();
    const GROUP* First() const;
    GROUP* Next(const GROUP* group);
    void InsertToNearGroup(SPRITE* sprite);
    void ShiftFirstLeft();
    void ShiftFirstRight();
};
