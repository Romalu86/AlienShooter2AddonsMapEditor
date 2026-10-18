#include "vid.h"
#include "engine_globals.h"
#include "menu.h"

#include "message.h"
#include "mylib.h"
#include "sprite.h"
#include "graph.h"
#include "map.h"
#include "basetype.h"
#include "constant.h"
#include "myerror.h"
#include <cstring>
#include <new>

MESSAGE::MESSAGE(int mapIndex, int vidIndex, float baseX, float baseY, int lineCount, unsigned long shiftIntervalMs)
    : m_lineCount(lineCount),
      m_shiftIntervalMs(shiftIntervalMs),
      m_mapIndex(mapIndex),
      m_vidIndex(vidIndex),
      m_baseX(baseX),
      m_baseY(baseY),
      m_rowShift(-1)
{
    for (int index = 0; index<m_lineCount; ++index)
    {
        m_primarySprites[index] = 0;
        m_secondarySprites[index] = 0;
    }
}

MESSAGE::~MESSAGE() noexcept(false)
{
    Release();

}

void MESSAGE::Release()
{
    for (int index = 0; index<m_lineCount; ++index)
    {
        SPRITE* secondary = m_secondarySprites[index];
        delete secondary;
        m_secondarySprites[index] = 0;

        SPRITE* primary = m_primarySprites[index];
        delete primary;
        m_primarySprites[index] = 0;
    }
}

void MESSAGE::DeletePointerToSprite(SPRITE* sprite)
{
    for (int index = 0; index<m_lineCount; ++index)
    {
        if (m_secondarySprites[index] == sprite) m_secondarySprites[index] = 0;
        if (m_primarySprites[index] == sprite) m_primarySprites[index] = 0;
    }
}

static int MessageVerticalOffsetToInt(float spriteY, float spriteZ, float mapShiftY, float viewTop)
{
    const double offset = ((static_cast<double>(spriteY)-spriteZ)-mapShiftY)-viewTop;
    return TruncateDoubleToInt32(offset);
}

void MESSAGE::Tact()
{
    MAP* game = Map;
    VID* mapObject = game->Vid(m_mapIndex);
    const int mapScale = static_cast<int>(mapObject->vidSizeY)+1;

    LIST<MESSAGE_STACK>& queued = m_messageStackList;
    int index = 0;
    while (index<queued.m_count)
    {
        MESSAGE_STACK& record = queued.m_data[index];
        const unsigned int now = ::CurrentTime;
        const unsigned int frameBase = ::PrevCurrentTime;
        const unsigned int expires = record.m_duration;
        if (now-frameBase >= expires)
        {
            Put(record.m_text, record.m_x, record.m_y);
            (void)queued.DeleteNumber(index);
            continue;
        }
        record.m_duration = expires-now+::PrevCurrentTime;
        ++index;
    }

    if (::CurrentTime-m_lastShiftTime>m_shiftIntervalMs)
    {
        Shift();
        m_lastShiftTime = ::CurrentTime;
    }

    game = Map;
    if ((game->m_menu.ControlFlags()&1u) == 0u) return;
    if (game->m_menu.NVidUnderCursor() != m_vidIndex) return;

    SPRITE* const view = game->m_menu.SelectedSprite();
    const int vertical = MessageVerticalOffsetToInt(
        view->Y(), view->Z(), game->m_shiftXY.y, Graph->viewTop);
    const int row = vertical/mapScale;
    const unsigned int sentinel = 0xC97423F0u;
    const unsigned int xBits = FloatBitsU32(m_screenX[row]);
    const unsigned int yBits = FloatBitsU32(m_screenY[row]);
    if (xBits == sentinel && yBits == sentinel) return;

    const VECTOR2 point(m_screenX[row], m_screenY[row]);
    game->SetShiftCoor(point, 2);
}

void MESSAGE::PutToStack(const STRING& text, float x, float y)
{
    m_messageStackList.Insert(MESSAGE_STACK(text, x, y));
}

void MESSAGE::Put(const STRING& text, float x, float y)
{
    MAP* game = Map;
    VID* mapObject = game->Vid(m_mapIndex);
    const int mapScale = static_cast<int>(mapObject->vidSizeY)+1;

    if (std::strcmp(text.m_value, StringGlobals::EmptyText()) == 0) return;

    m_lastShiftTime = ::CurrentTime;
    if (m_secondarySprites[m_lineCount-1]) Shift();

    SPRITE* textSprite = m_secondarySprites[m_lineCount-1];
    if (!textSprite)
    {
        const float heroY = Graph->viewTop;
        VECTOR position;
        position.x = m_baseX;
        const float baseLine = static_cast<float>(
            static_cast<double>(MultiplySigned32Wrap(m_lineCount-1, mapScale))+m_baseY+2000.0);
        position.y = static_cast<float>(static_cast<double>(heroY)+baseLine);
        position.z = 2000.0f;
        VID* const vid = Map->Vid(m_mapIndex);
        textSprite = Map->CreateSprite(vid, position, ANGLE(static_cast<unsigned char>(0u)), 0);
        m_secondarySprites[m_lineCount-1] = textSprite;
    }

    textSprite = m_secondarySprites[m_lineCount-1];
    if (textSprite)
        (void) textSprite->Action(ACT_SET_TEXT, PtrToU32(&text), 0, 0);

    m_screenX[m_lineCount-1] = x;
    m_screenY[m_lineCount-1] = y;
    if (FloatBitsU32(x) == 0xBF800000u && FloatBitsU32(y) == 0xBF800000u) return;

    const float heroY = Graph->viewTop;
    VECTOR markerPosition;
    markerPosition.x = m_baseX-10.0f;
    const float markerLine = static_cast<float>(
        static_cast<double>(mapScale/2)+m_baseY+MultiplySigned32Wrap(m_lineCount-1, mapScale)+2000.0);
    markerPosition.y = static_cast<float>(static_cast<double>(heroY)+markerLine);
    markerPosition.z = 2000.0f;
    VID* const markerVid = Map->Vid(m_vidIndex);
    SPRITE* const marker = Map->CreateSprite(markerVid, markerPosition, ANGLE(static_cast<unsigned char>(0u)), 0);
    m_primarySprites[m_lineCount-1] = marker;
}

void MESSAGE::Shift()
{
    MAP* game = Map;
    VID* mapObject = game->Vid(m_mapIndex);
    const int mapScale = static_cast<int>(mapObject->vidSizeY)+1;

    delete m_secondarySprites[0];
    m_secondarySprites[0] = 0;
    delete m_primarySprites[0];
    m_primarySprites[0] = 0;

    for (int index = 1; index<m_lineCount; ++index)
    {
        SPRITE* sprite = m_primarySprites[index];
        if (!sprite) continue;
        VECTOR position(sprite->X(),
            static_cast<float>(static_cast<double>(MultiplySigned32Wrap(m_rowShift, mapScale))+sprite->Y()),
            sprite->Z());

        sprite->ChangeCoor(position);
        m_primarySprites[index-1] = m_primarySprites[index];
        m_screenX[index-1] = m_screenX[index];
        m_screenY[index-1] = m_screenY[index];
        m_primarySprites[index] = 0;
    }

    for (int index = 1; index<m_lineCount; ++index)
    {
        SPRITE* sprite = m_secondarySprites[index];
        if (!sprite) continue;
        VECTOR position(sprite->X(),
            static_cast<float>(static_cast<double>(MultiplySigned32Wrap(m_rowShift, mapScale))+sprite->Y()),
            sprite->Z());

        sprite->ChangeCoor(position);
        m_secondarySprites[index-1] = m_secondarySprites[index];
        m_secondarySprites[index] = 0;
    }
}

MESSAGE_STACK::MESSAGE_STACK() = default;

MESSAGE_STACK::MESSAGE_STACK(STRING text, float x, float y)
    : m_text(text), m_x(x), m_y(y),
      m_duration(static_cast<unsigned int>(Const->MessageStartDelay))
{
}

MESSAGE_STACK::MESSAGE_STACK(const MESSAGE_STACK& source) : m_text(source.m_text)
{
    std::memcpy(&m_x, &source.m_x, sizeof(m_x));
    std::memcpy(&m_y, &source.m_y, sizeof(m_y));
    std::memcpy(&m_duration, &source.m_duration, sizeof(m_duration));
}

MESSAGE_STACK::~MESSAGE_STACK() = default;

MESSAGE_STACK& MESSAGE_STACK::operator=(const MESSAGE_STACK& source)
{
    m_text = source.m_text;
    std::memcpy(&m_x, &source.m_x, sizeof(m_x));
    std::memcpy(&m_y, &source.m_y, sizeof(m_y));
    std::memcpy(&m_duration, &source.m_duration, sizeof(m_duration));
    return *this;
}

template<> LIST<MESSAGE_STACK>::LIST() : m_count(0), m_capacity(0), m_data(0)
{
}

template<> LIST<MESSAGE_STACK>::~LIST()
{
    delete[] m_data;
    m_data = 0;
    m_count = 0;
}

template<> int LIST<MESSAGE_STACK>::No() const
{
    return m_count;
}

template<> MESSAGE_STACK& LIST<MESSAGE_STACK>::Item(int index) const
{
    return m_data[index];
}

template<> void LIST<MESSAGE_STACK>::Insert(MESSAGE_STACK value)
{
    ExpandForInsert();
    m_data[m_count] = value;
    ++m_count;
}

template<> int LIST<MESSAGE_STACK>::DeleteNumber(int index)
{
    if (index < 0 || index >= m_count) return 1;
    --m_count;
    for (; index < m_count; ++index) m_data[index] = m_data[index+1];
    if (m_count == 0) Release();
    return 0;
}

template<> void LIST<MESSAGE_STACK>::Release()
{
    m_capacity = 0;
    m_count = 0;
    delete[] m_data;
    m_data = 0;
}

template<> void LIST<MESSAGE_STACK>::ExpandForInsert()
{
    if (m_count >= m_capacity)
        Expand(BitsToSigned32(static_cast<unsigned int>(m_capacity)*2u+4u));
}

template<> void LIST<MESSAGE_STACK>::Expand(int requestedCapacity)
{
    if (requestedCapacity <= m_capacity) return;
    MESSAGE_STACK* const previous = m_data;
    m_data = new(std::nothrow) MESSAGE_STACK[requestedCapacity];
    if (!m_data)
        ::Error->LogExit(STRING("!!!ERROR!!!::LIST: Not enough memory %i"), requestedCapacity);
    if (previous)
    {
        for (int i = 0; i < m_capacity; ++i) m_data[i] = previous[i];
        delete[] previous;
    }
    m_capacity = requestedCapacity;
}
