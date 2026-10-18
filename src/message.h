#pragma once

#include "list.h"

class SPRITE;

struct MESSAGE_STACK
{
    STRING m_text;
    float m_x;
    float m_y;
    unsigned int m_duration;

    MESSAGE_STACK();
    MESSAGE_STACK(STRING text, float x, float y);
    MESSAGE_STACK(const MESSAGE_STACK& source);
    ~MESSAGE_STACK();
    MESSAGE_STACK& operator=(const MESSAGE_STACK& source);
};

template<> LIST<MESSAGE_STACK>::LIST();
template<> LIST<MESSAGE_STACK>::~LIST();
template<> int LIST<MESSAGE_STACK>::No() const;
template<> MESSAGE_STACK& LIST<MESSAGE_STACK>::Item(int index) const;
template<> void LIST<MESSAGE_STACK>::Insert(MESSAGE_STACK value);
template<> int LIST<MESSAGE_STACK>::DeleteNumber(int index);
template<> void LIST<MESSAGE_STACK>::Release();
template<> void LIST<MESSAGE_STACK>::Expand(int requestedCapacity);
template<> void LIST<MESSAGE_STACK>::ExpandForInsert();

class MESSAGE
{
public:

    MESSAGE(int mapIndex, int vidIndex, float baseX, float baseY,
            int lineCount, unsigned long shiftIntervalMs);
    virtual ~MESSAGE() noexcept(false);
    virtual void DeletePointerToSprite(SPRITE* sprite);

    void Release();
    void Tact();
    void Put(const STRING& text, float x, float y);
    void PutToStack(const STRING& text, float x, float y);
    void Shift();

private:
    friend class PLAYER_ARCADE;

    int           m_lineCount;
    unsigned long m_shiftIntervalMs;
    int           m_mapIndex;
    int           m_vidIndex;
    float         m_baseX;
    float         m_baseY;
    int           m_rowShift;
    unsigned long m_lastShiftTime;
    SPRITE*       m_primarySprites[45];
    SPRITE*       m_secondarySprites[45];
    float         m_screenX[45];
    float         m_screenY[45];
    LIST<MESSAGE_STACK> m_messageStackList;
};
