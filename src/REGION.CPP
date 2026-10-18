#include "mylib.h"
#include "engine_globals.h"

#include "region.h"
#include "basetype.h"
#include "graph.h"
#include "map.h"
#include "sprite.h"
#include "sprite_collector.h"
#include "stream.h"
#include "vid.h"
#include <new>

REGION::REGION(VID* vid, const VECTOR& position, ANGLE direction, SPRITE* parent)
    : SPRITE(vid, position, direction, parent),
      m_fogPhase(0), m_lastFogPhase(0), m_fogRamp(0), m_fogFlags(0),
      m_fogEnd(0), m_fogStart(0), m_084(0), m_halfWidth(0.0f), m_halfHeight(0.0f),
      m_regionVid(::EmptyVid)
{
    m_fogColor.value = 0xFF000000u;
    for (unsigned int index = 0;index<static_cast<unsigned int>(VidConversionCount);++index)
    {
        m_sourceVid[index] = 0;
        m_targetVid[index] = 0;
    }
}

REGION::~REGION()
{
    Map->DeletePointerToSprite(this);
    if (m_fogRamp)
        delete[] m_fogRamp;
}

void REGION::DrawSecondaryInfo()
{
    const VECTOR2 rightBottom = ScreenRightBottom()+VECTOR2(1, 1);
    const VECTOR2 leftTop = ScreenLeftTop()+VECTOR2(-1, -1);
    COLOR color;
    color.value = 0xFFFFFFFFu;
    Graph->Box(leftTop, rightBottom, color);
}

VECTOR2 REGION::ScreenLeftTop() const
{
    return Map->ToScreen(m_xyz.ViewCoor()-VECTOR2(m_halfWidth, m_halfHeight)/2.0f);
}

VECTOR2 REGION::ScreenRightBottom() const
{
    return Map->ToScreen(m_xyz.ViewCoor()+VECTOR2(m_halfWidth, m_halfHeight)/2.0f);
}

void REGION::Draw()
{
    const float originalX = m_xyz.x;
    const float originalY = m_xyz.y;
    const int originalFrame = m_currentFrame;
    VID* const regionVid = m_vid;
    if ((regionVid->runtimeFlags&0x800u) != 0u)
        return;
    if (regionVid != ::EmptyVid && (static_cast<unsigned int>(regionVid->type)&0x40u) != 0u && reinterpret_cast<const VID_SURFACE*>(regionVid)->frames != 0)
    {
        float tileY = static_cast<float>(originalY-static_cast<double>(m_halfHeight)/2.0f);
        int tileIndex = 0;
        while (tileY<originalY+static_cast<double>(m_halfHeight)/2.0f)
        {
            float tileX = static_cast<float>(originalX-static_cast<double>(m_halfWidth)/2.0f);
            while (tileX<originalX+static_cast<double>(m_halfWidth)/2.0f)
            {
                if ((m_vid->property&0x01000000u) == 0u)
                {
                    m_currentFrame += 2*tileIndex++;
                    m_currentFrame %= static_cast<int>(m_vid->noCadr);
                }
                m_xyz.XY() = VECTOR2(tileX, tileY)+
                    VECTOR2(m_vid->buildSizeX, m_vid->buildSizeY)/2.0f;
                m_vid->Draw(this);
                tileX += static_cast<float>(m_vid->vidSizeX);
            }
            tileY += static_cast<float>(m_vid->vidSizeY);
        }
    }
    m_xyz.x = originalX;
    m_xyz.y = originalY;
    m_currentFrame = originalFrame;
    const int fogEnd = m_fogEnd;
    const int fogStart = m_fogStart;
    if (fogStart >= fogEnd)
    {
        m_fogPhase = 0;
        return;
    }
    const unsigned int fogFlags = m_fogFlags;
    if ((fogFlags&1u) != 0u)
    {
        const unsigned int currentTickPhase = ::CurrentTime&7u;
        const unsigned int previousTickPhase = m_lastFogPhase;
        if (currentTickPhase<previousTickPhase)
        {
            const int phaseLimit = fogEnd*8;
            int fogPhase = m_fogPhase;
            if (fogPhase<phaseLimit)
                fogPhase += 2;
            else if (fogPhase>phaseLimit)
                fogPhase = 0;
            m_fogPhase = fogPhase;
        }
        m_lastFogPhase = currentTickPhase;
    }
    else
    {
        m_fogPhase = fogEnd*8;
    }
    GRAPH* const renderer = Graph;
    COLOR color;
    color.value = m_fogColor.value;
    unsigned short* const fogRamp = reinterpret_cast<unsigned short*>(m_fogRamp);
    const int fogPhase = m_fogPhase;
    const int blendMode = static_cast<int>(fogFlags&2u);
    if ((fogFlags&8u) != 0u)
    {
        renderer->DrawFog(renderer->ViewMin(), renderer->ViewMax(), fogStart, fogEnd, color, fogRamp, fogPhase, blendMode);
    }
    else
    {
        const VECTOR2 rightBottom = ScreenRightBottom();
        const VECTOR2 leftTop = ScreenLeftTop();
        renderer->DrawFog(leftTop, rightBottom, fogStart, fogEnd, color, fogRamp, fogPhase, blendMode);
    }
}

VID* REGION::ConvertVid(VID* sourceVid, const VECTOR& position)
{
    int drawingIterator;
    SPRITE_COLLECTOR* const spriteCollector = ::Sprites;
    SPRITE* candidateSprite = spriteCollector->FirstDrawingSprite(&drawingIterator, 5);
    while (candidateSprite)
    {
        if (candidateSprite->IsSpriteClass(23) && candidateSprite->Z()>position.z)
        {
            REGION* const candidateRegion = static_cast<REGION*>(candidateSprite);
            bool containsPosition = (candidateRegion->m_fogFlags&8u) != 0u;
            if (!containsPosition)
            {
                const VECTOR2 size(candidateRegion->m_halfWidth, candidateRegion->m_halfHeight);
                containsPosition = position.XY() >= candidateRegion->m_xyz.XY()-size/2.0f &&
                    position.XY() <= candidateRegion->m_xyz.XY()+size/2.0f;
            }
            if (containsPosition)
            {
                for (unsigned int conversionIndex = 0;conversionIndex<static_cast<unsigned int>(VidConversionCount);++conversionIndex)
                {
                    if (candidateRegion->m_sourceVid[conversionIndex] == sourceVid)
                        return candidateRegion->m_targetVid[conversionIndex];
                }
            }
        }
        candidateSprite = ::Sprites->NextDrawingSprite(&drawingIterator, 5);
    }
    return sourceVid;
}

void REGION::SetFogParameters(int fogStart, int fogEnd, COLOR fogColor)
{
    m_fogEnd = fogEnd;
    m_fogStart = fogStart;
    m_fogColor.value = fogColor.value;
    if (m_fogRamp)
        delete[] m_fogRamp;
    if (fogStart >= fogEnd)
        return;
    const int rampSampleCount = (fogEnd-fogStart)*8;
    m_fogRamp = new(std::nothrow) unsigned short[rampSampleCount+1];
    if (!m_fogRamp)
        ::Error->LogExit(STRING("Enough memory for DrawFog"), rampSampleCount+1);
    const int fogRange = fogEnd-fogStart;
    int scaledIntensity = rampSampleCount*255;
    for (int sampleIndex = rampSampleCount;sampleIndex >= 0;--sampleIndex, scaledIntensity -= 255)
    {
        int dividedIntensity = scaledIntensity/fogRange;
        if (dividedIntensity<0)
            dividedIntensity += 7;
        const int rampIntensity = dividedIntensity>>3;
        m_fogRamp[sampleIndex] = static_cast<unsigned short>(rampIntensity);
    }
}

int REGION::Action(int actionCode, int firstParameter, int secondParameter, int thirdParameter)
{
    if (actionCode != ACT_SAVE && actionCode != ACT_RESTORE && actionCode != ACT_LEGACY_RESTORE)
        return SPRITE::Action(actionCode, firstParameter, secondParameter, thirdParameter);
    STREAM* const stream = reinterpret_cast<STREAM*>(U32ToPtr(static_cast<unsigned int>(firstParameter)));
    (void)SPRITE::Action(actionCode, firstParameter, secondParameter, thirdParameter);
    if (actionCode == ACT_SAVE)
    {
        (void)stream->Write(&m_fogFlags, 4u);
        (void)stream->Write(&m_fogEnd, 4u);
        (void)stream->Write(&m_fogStart, 4u);
        (void)stream->Write(&m_fogColor, 4u);
        (void)stream->Write(&m_halfWidth, 4u);
        (void)stream->Write(&m_halfHeight, 4u);
        (void)stream->Write(&m_094, 4u);
        int vidId = -1;
        VID* serializedVid = m_regionVid;
        (void)stream->Write(serializedVid ? &serializedVid->nVid : &vidId, sizeof(vidId));
        for (int conversionIndex = 0;conversionIndex<VidConversionCount;++conversionIndex)
        {
            vidId = -1;
            serializedVid = m_sourceVid[conversionIndex];
            (void)stream->Write(serializedVid ? &serializedVid->nVid : &vidId, sizeof(vidId));
            vidId = -1;
            serializedVid = m_targetVid[conversionIndex];
            (void)stream->Write(serializedVid ? &serializedVid->nVid : &vidId, sizeof(vidId));
        }
        return 0;
    }
    const int saveVersion = secondParameter;
    (void)stream->Read(&m_fogFlags, 4u);
    int fogEnd = secondParameter;
    int fogStart = firstParameter;
    COLOR fogColor;
    fogColor.value = static_cast<unsigned int>(thirdParameter);
    (void)stream->Read(&fogEnd, 4u);
    (void)stream->Read(&fogStart, 4u);
    (void)stream->Read(&fogColor, 4u);
    SetFogParameters(fogStart, fogEnd, fogColor);
    if (saveVersion>9)
    {
        (void)stream->Read(&m_halfWidth, 4u);
        (void)stream->Read(&m_halfHeight, 4u);
    }
    else
    {
        (void)stream->Read(&m_halfWidth, sizeof(m_halfWidth));
        (void)stream->Read(&m_halfHeight, sizeof(m_halfHeight));
        m_halfWidth = static_cast<float>(BitsToSigned32(FloatBitsU32(m_halfWidth)));
        m_halfHeight = static_cast<float>(BitsToSigned32(FloatBitsU32(m_halfHeight)));
    }
    (void)stream->Read(&m_094, 4u);
    MAP* map = Map;
    int vidId = actionCode;
    (void)stream->Read(&vidId, 4u);
    VID* sourceVid = 0;
    if (vidId >= 0 && vidId<map->m_vidCount)
        sourceVid = map->m_vids[vidId];
    if (!sourceVid)
        sourceVid = ::EmptyVid;
    m_regionVid = sourceVid;
    int targetVidId;
    for (int conversionIndex = 0;conversionIndex<VidConversionCount;++conversionIndex)
    {
        map = Map;
        (void)stream->Read(&vidId, sizeof(vidId));
        sourceVid = 0;
        if (vidId >= 0 && vidId<map->m_vidCount)
            sourceVid = map->m_vids[vidId];
        m_sourceVid[conversionIndex] = sourceVid;
        map = Map;
        (void)stream->Read(&targetVidId, sizeof(targetVidId));
        VID* targetVid = 0;
        if (targetVidId >= 0 && targetVidId<map->m_vidCount)
            targetVid = map->m_vids[targetVidId];
        m_targetVid[conversionIndex] = targetVid;
        sourceVid = m_sourceVid[conversionIndex];
        if (sourceVid)
            sourceVid->runtimeFlags |= 0x20u;
    }
    return 0;
}
