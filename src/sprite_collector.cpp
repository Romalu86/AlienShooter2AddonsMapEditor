#include <new>
#include <exception>
#define INPUT WIN32_INPUT
#include <windows.h>
#include <d3d9.h>
#undef INPUT

#include "basetype.h"
#include "sprite_collector.h"

#include "sprite.h"
#include "vid.h"
#include "map.h"
#include "mouse.h"
#include "list.h"
#include "graph.h"
#include "region.h"
#include "texture.h"
#include "mylib.h"
#include "myerror.h"

VECTOR SPRITE::GetScale() const
{
    return m_extraData ? m_extraData->scale : VECTOR(1.0f, 1.0f, 1.0f);
}
SPRITE_COLLECTOR* Sprites = 0;

namespace {
int allSpriteItemCursor = 0;
int allSpriteCellCursor = 0;
int allSpriteLayerCursor = 0;
int drawingSpriteItemCursor = 0;
int drawingSpriteCellCursor = 0;
int noFreeBuildedGround = 0;
unsigned int drawnGroundCellCount = 0;
unsigned int groundCellBuildAttempts = 0;
}

int SPRITE_COLLECTOR::GetNoFreeBuildedGround() const
{
    return 0;
}

static SPRITE_LIST* SpriteLayerBucket(const SPRITE_COLLECTOR* collector, int layer, int cell)
{
    SPRITE_COLLECTOR::CELL& record = collector->spatialCells[cell];
    return &record.layerSprites[layer];
}
static SPRITE* LastSpriteInLayer(SPRITE_LIST* bucket)
{
    const int count = bucket->m_count;
    if (count == 0) return 0;
    const int item = count-1;
    allSpriteItemCursor = item;
    return bucket->m_data[item];
}
static SPRITE* DepthSortedSpriteAt(const SPRITE_COLLECTOR* collector, int item)
{
    SPRITE** list = collector->depthSortedSprites.m_data;
    return list[item];
}

namespace {
static int ClampHashCoordinate(double value, float scale, int limit)
{
    int cell = TruncateDoubleToInt32(value*static_cast<double>(scale));
    if (cell<0) return 0;
    if (cell >= limit) return limit-1;
    return cell;
}

}

void SPRITE_COLLECTOR::GetHashSpritesInBox(LIST<SPRITE*>* result,
    float minX, float minY, float maxX, float maxY) const
{
    const float firstX = static_cast<float>(static_cast<double>(minX) - 1.0 / gridScaleX);
    const int minCellX = ClampHashCoordinate(firstX, gridScaleX, gridWidth);
    const float firstY = static_cast<float>(static_cast<double>(minY) - 1.0 / gridScaleY);
    const int minCellY = ClampHashCoordinate(firstY, gridScaleY, gridHeight);
    const float lastX = static_cast<float>(static_cast<double>(maxX) + 1.0 / gridScaleX);
    const int maxCellX = ClampHashCoordinate(lastX, gridScaleX, gridWidth);
    const float lastY = static_cast<float>(static_cast<double>(maxY) + 1.0 / gridScaleY);
    const int maxCellY = ClampHashCoordinate(lastY, gridScaleY, gridHeight);

    result->m_count = 0;
    if (result->m_count > result->m_capacity)
        result->Expand(result->m_count);
    for (int y = minCellY; y <= maxCellY; ++y)
    {
        for (int x = minCellX; x <= maxCellX; ++x)
        {
            const unsigned int cellIndex = (static_cast<unsigned int>(y) <<
                (static_cast<unsigned int>(gridShift) & 31u)) + static_cast<unsigned int>(x);
            const SPRITE_LIST& list = spatialCells[cellIndex].sprites;
            if (list.m_count > 0)
                result->Insert(list.m_data[0]);
        }
    }
}

SPRITE* SPRITE_COLLECTOR::FirstHashInBox(float minX, float minY, float maxX, float maxY)
{
    const float firstX = static_cast<float>(static_cast<double>(minX) - 1.0 / gridScaleX);
    scanMinX = ClampHashCoordinate(firstX, gridScaleX, gridWidth);
    const float firstY = static_cast<float>(static_cast<double>(minY) - 1.0 / gridScaleY);
    scanCurrentY = ClampHashCoordinate(firstY, gridScaleY, gridHeight);
    const float lastX = static_cast<float>(static_cast<double>(maxX) + 1.0 / gridScaleX);
    scanMaxX = ClampHashCoordinate(lastX, gridScaleX, gridWidth);
    const float lastY = static_cast<float>(static_cast<double>(maxY) + 1.0 / gridScaleY);
    scanMaxY = ClampHashCoordinate(lastY, gridScaleY, gridHeight);
    scanCurrentX = scanMinX;
    scanItem = 0;
    return NextHashInBox();
}

SPRITE* SPRITE_COLLECTOR::NextHashInBox()
{
    while (scanCurrentY <= scanMaxY)
    {
        while (scanCurrentX <= scanMaxX)
        {
            const unsigned int cellIndex = (static_cast<unsigned int>(scanCurrentY)<<(static_cast<unsigned int>(gridShift)&31u))+static_cast<unsigned int>(scanCurrentX);
            SPRITE_LIST& list = spatialCells[cellIndex].sprites;
            if (scanItem<list.m_count)
            {
                const int item = scanItem;
                ++scanItem;
                return list.m_data[item];
            }
            ++scanCurrentX;
            scanItem = 0;
        }
        ++scanCurrentY;
        scanCurrentX = scanMinX;
        scanItem = 0;
    }
    return 0;
}

SPRITE* SPRITE_COLLECTOR::CanPlace(const VID* vid, const VECTOR& position, ANGLE direction)
{
    if (vid->moveMask == 0u) return 0;

    if (Map->CanPlaceInGridZ(vid, position, direction))
        return Mouse;

    SPRITE* candidate = FirstHashInBox(
        position.x-vid->halfSizeXY.x,
        position.y-vid->halfSizeXY.y,
        position.x+vid->halfSizeXY.x,
        position.y+vid->halfSizeXY.y);

    while (candidate)
    {
        if ((candidate->Behave()&0x20u) == 0u &&
            candidate->IsCross(vid, position, direction) &&
            (candidate->Vid()->moveMask&vid->moveMask) != 0u)
            return candidate;
        candidate = NextHashInBox();
    }
    return 0;
}

SPRITE* SPRITE_COLLECTOR::CanPlace(const VID* vid, float x, float y, float z, ANGLE direction)
{
    const VECTOR position(x, y, z);
    return CanPlace(vid, position, direction);
}

SPRITE* SPRITE_COLLECTOR::FirstActiveSprite(int* index) const
{
    return allSprites.BeginIterateNon0(index);
}

SPRITE* SPRITE_COLLECTOR::NextActiveSprite(int* index) const
{
    return allSprites.NextIterateNon0(index);
}

SPRITE* SPRITE_COLLECTOR::FirstAllSprite() const
{
    int layer = 0;
    allSpriteLayerCursor = layer;
    while (layer <= 14)
    {
        int cell = gridHeight*gridWidth;
        for (; ;)
        {
            --cell;
            allSpriteCellCursor = cell;
            if (cell<0) break;
            SPRITE* sprite = LastSpriteInLayer(SpriteLayerBucket(this, layer, cell));
            if (sprite) return sprite;
        }
        ++layer;
        allSpriteLayerCursor = layer;
    }
    const int count = depthSortedSprites.m_count;
    if (count == 0) return 0;
    const int item = count-1;
    allSpriteItemCursor = item;
    return DepthSortedSpriteAt(this, item);
}

SPRITE* SPRITE_COLLECTOR::NextAllSprite() const
{
    int layer = allSpriteLayerCursor;
    if (layer>14)
    {
        const int count = depthSortedSprites.m_count;
        int item = allSpriteItemCursor;
        if (item>count) item = count;
        --item;
        allSpriteItemCursor = item;
        return item<0 ? 0 : DepthSortedSpriteAt(this, item);
    }

    int cell = allSpriteCellCursor;
    SPRITE_LIST* bucket = SpriteLayerBucket(this, layer, cell);
    int item = allSpriteItemCursor;
    const int count = bucket->m_count;
    if (item>count) item = count;
    --item;
    allSpriteItemCursor = item;
    SPRITE* sprite = 0;
    if (item >= 0)
    {
        sprite = bucket->m_data[item];
        if (sprite) return sprite;
    }

    for (; ;)
    {
        --cell;
        allSpriteCellCursor = cell;
        while (cell >= 0)
        {
            sprite = LastSpriteInLayer(SpriteLayerBucket(this, layer, cell));
            if (sprite) return sprite;
            --cell;
            allSpriteCellCursor = cell;
        }

        ++layer;
        allSpriteLayerCursor = layer;
        if (layer>14)
        {
            const int fallbackCount = depthSortedSprites.m_count;
            if (fallbackCount == 0) return 0;
            const int fallbackItem = fallbackCount-1;
            allSpriteItemCursor = fallbackItem;
            return DepthSortedSpriteAt(this, fallbackItem);
        }
        cell = gridHeight*gridWidth;
        allSpriteCellCursor = cell;

    }
}

SPRITE* SPRITE_COLLECTOR::FirstAllSpriteFrom0() const
{
    int layer = 0;
    allSpriteLayerCursor = layer;
    while (layer <= 14)
    {
        int cell = gridHeight*gridWidth;
        for (; ;)
        {
            --cell;
            allSpriteCellCursor = cell;
            if (cell<0) break;
            SPRITE_LIST* bucket = SpriteLayerBucket(this, layer, cell);
            if (bucket->m_count == 0) continue;
            allSpriteItemCursor = 0;
            SPRITE* sprite = bucket->m_data[0];
            if (sprite) return sprite;
        }
        ++layer;
        allSpriteLayerCursor = layer;
    }
    if (depthSortedSprites.m_count == 0) return 0;
    allSpriteItemCursor = 0;
    return depthSortedSprites.m_data[0];
}

SPRITE* SPRITE_COLLECTOR::NextAllSpriteFrom0() const
{
    int layer = allSpriteLayerCursor;
    if (layer>14)
    {
        const int item = allSpriteItemCursor+1;
        allSpriteItemCursor = item;
        const int count = depthSortedSprites.m_count;
        if (item >= count) return 0;
        SPRITE** list = depthSortedSprites.m_data;
        return list[item];
    }

    int cell = allSpriteCellCursor;
    SPRITE_LIST* bucket = SpriteLayerBucket(this, layer, cell);
    int item = allSpriteItemCursor+1;
    allSpriteItemCursor = item;
    const int count = bucket->m_count;
    if (item<count)
    {
        SPRITE* sprite = bucket->m_data[item];
        if (sprite) return sprite;
    }

    const int totalCells = gridHeight*gridWidth;
    for (; ;)
    {
        --cell;
        allSpriteCellCursor = cell;
        while (cell >= 0)
        {
            bucket = SpriteLayerBucket(this, layer, cell);
            if (bucket->m_count != 0)
            {
                allSpriteItemCursor = 0;
                SPRITE* sprite = bucket->m_data[0];
                if (sprite) return sprite;
            }
            --cell;
            allSpriteCellCursor = cell;
        }

        ++layer;
        allSpriteLayerCursor = layer;
        if (layer>14) break;
        cell = totalCells;
        allSpriteCellCursor = cell;

    }

    if (depthSortedSprites.m_count == 0) return 0;
    allSpriteItemCursor = 0;
    SPRITE** list = depthSortedSprites.m_data;
    return list[0];
}

void SPRITE_COLLECTOR::InvalidateDeviceObjects()
{
    if (renderTargetGroundCount <= 0)
    {
        renderTargetGroundCount = 0;
        return;
    }

    for (int i = 0; i < renderTargetGroundCount; ++i)
    {
        CELL* cell = renderTargetGroundCells[i];
        if (cell->renderTargetGround != 0)
        {
            {
                TEXTURE* const releasedTexture = cell->renderTargetGround;
                delete releasedTexture;
            }
            renderTargetGroundCells[i]->renderTargetGround = 0;
        }
        renderTargetGroundCells[i] = 0;
    }
    renderTargetGroundCount = 0;
}

SPRITE* SPRITE_COLLECTOR::FirstDrawingSprite(int* outIndex, int category)
{
    if (category == 11)
    {
        int count = depthSortedSprites.m_count;
        if (count == 0) return 0;
        --count;
        *outIndex = count;
        return depthSortedSprites.m_data[count];
    }

    int row = gridHeight*gridWidth-1;
    int selected = category;
    SPRITE* result = 0;
    for (; ;)
    {
        SPRITE_LIST& list = spatialCells[row].layerSprites[category];
        const int count = list.m_count;
        if (count != 0)
        {
            selected = count-1;
            result = list.m_data[selected];
            if (result) break;
        }
        --row;
        if (row<0) break;
    }
    drawingSpriteItemCursor = selected;
    drawingSpriteCellCursor = row;
    return result;
}

SPRITE* SPRITE_COLLECTOR::NextDrawingSprite(int* cursor, int type)
{
    if (type == 11)
    {
        const int count = depthSortedSprites.m_count;
        if (*cursor>count) *cursor = count;
        --*cursor;
        if (*cursor<0) return 0;
        return depthSortedSprites.m_data[*cursor];
    }

    int cellIndex = drawingSpriteCellCursor;
    int spriteIndex = drawingSpriteItemCursor;
    SPRITE_LIST& initial = spatialCells[cellIndex].layerSprites[type];
    if (spriteIndex>initial.m_count) spriteIndex = initial.m_count;
    --spriteIndex;
    SPRITE* value = spriteIndex >= 0 ? initial.m_data[spriteIndex] : 0;
    if (!value)
    {
        while (--cellIndex >= 0)
        {
            SPRITE_LIST& list = spatialCells[cellIndex].layerSprites[type];
            const int count = list.m_count;
            if (count == 0) continue;
            spriteIndex = count-1;
            value = list.m_data[spriteIndex];
            if (value) break;
        }
    }
    drawingSpriteCellCursor = cellIndex;
    drawingSpriteItemCursor = spriteIndex;
    return value;
}

namespace {
static int SpriteCollectorClampCell(int value, int limit)
{
    if (value<0) return 0;
    if (value >= limit) return BitsToSigned32(static_cast<unsigned int>(limit)-1u);
    return value;
}

static SPRITE_LIST* SpriteCollectorLayerBucket(const SPRITE_COLLECTOR* self, int layer, int x, int y)
{
    const unsigned int cellIndex = (static_cast<unsigned int>(y)<<(self->gridShift&31))+static_cast<unsigned int>(x);
    return &self->spatialCells[cellIndex].layerSprites[layer];
}

}

int SPRITE_COLLECTOR::GetFarGroundCell(const VECTOR2& minPoint, const VECTOR2& maxPoint) const
{
    const int count = renderTargetGroundCount;
    const float middleYSum = minPoint.y+maxPoint.y;
    const float middleX = (minPoint.x+maxPoint.x)*0.5f;
    const VECTOR2 middle(middleX, middleYSum*0.5f);
    float farDistance = -1.0f;
    int farIndex = -1;

    for (int index = 0; index<count; ++index)
    {
        CELL* const cell = renderTargetGroundCells[index];
        const float cellX = cell->originX;
        const float cellY = cell->originY;

        if (cellX+128.0f>minPoint.x &&
            cellY+128.0f>minPoint.y &&
            maxPoint.x+128.0f>cellX &&
            !(cellY >= maxPoint.y+128.0f))
            continue;

        const float distance = VECTOR2(cellX-middle.x, cellY-middle.y).NearLength();
        if (distance>farDistance)
        {
            farDistance = distance;
            farIndex = index;
        }
    }
    return farIndex;
}

void SPRITE_COLLECTOR::DrawShadowInLayer(int layer, const VECTOR& position, const VECTOR2& halfExtents) const
{
    const float groundY = position.y-position.z;
    const VECTOR2 minPoint(position.x-halfExtents.x, groundY-halfExtents.y);
    const VECTOR2 maxPoint(position.x+halfExtents.x, groundY+halfExtents.y);
    const float marginX = 0.0f/gridScaleX;
    const int minX = SpriteCollectorClampCell(TruncateFloatToInt32((minPoint.x-marginX)*gridScaleX), gridWidth);
    const float marginY = 0.0f/gridScaleY;
    const int minY = SpriteCollectorClampCell(TruncateFloatToInt32((minPoint.y-marginY)*gridScaleY), gridHeight);
    const int maxX = SpriteCollectorClampCell(TruncateFloatToInt32((maxPoint.x+marginX)*gridScaleX), gridWidth);
    const int maxY = SpriteCollectorClampCell(TruncateFloatToInt32((maxPoint.y+marginY)*gridScaleY), gridHeight);

    const VECTOR2 oldViewMin = VID::viewMin;
    const VECTOR2 oldViewMax = VID::viewMax;

    VID::viewMin.x = minPoint.x;
    VID::viewMin.y = minPoint.y;
    VID::viewMax.x = maxPoint.x;
    VID::viewMax.y = maxPoint.y;

    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            int item = 0;
            if (SpriteCollectorLayerBucket(this, layer, x, y)->m_count != 0) do
                {
                    SPRITE* const sprite = SpriteCollectorLayerBucket(this, layer, x, y)->m_data[item];
                    if (!sprite) break;
                    if ((sprite->RuntimeFlags()&0x04000000u) == 0u &&
                        (sprite->Coor()-position).NearLength()<halfExtents.x)
                        sprite->Vid()->DrawShadow(sprite, position);
                }
                while (++item<SpriteCollectorLayerBucket(this, layer, x, y)->m_count);
        }
    }

    SPRITE* sprite = Mouse;
    if (Mouse->HardwareCursorEnabled() == 0u)
    {
        VID* const headOwner = sprite->Vid();
        if ((headOwner->property&0x00008000u) == 0u)
        {
            for (; sprite; sprite = sprite->ChildChain())
            {
                VID* const owner = sprite->Vid();
                if ((owner->property&0x00001000u) != 0u &&
                    (sprite->RuntimeFlags()&0x04000000u) == 0u &&
                    (static_cast<unsigned char>(owner->type)&0x80u) == 0u)
                    owner->DrawShadow(sprite, position);
            }
        }
    }

    VID::viewMin = oldViewMin;
    VID::viewMax = oldViewMax;
}

void SPRITE_COLLECTOR::DrawLayerOnlyZ(const VECTOR2& minPoint, const VECTOR2& maxPoint) const
{
    const float marginX = 1.0f/gridScaleX;
    const int minX = SpriteCollectorClampCell(TruncateFloatToInt32((minPoint.x-marginX)*gridScaleX), gridWidth);
    const float marginY = 1.0f/gridScaleY;
    const int minY = SpriteCollectorClampCell(TruncateFloatToInt32((minPoint.y-marginY)*gridScaleY), gridHeight);
    const int maxX = SpriteCollectorClampCell(TruncateFloatToInt32((maxPoint.x+marginX)*gridScaleX), gridWidth);
    const int maxY = SpriteCollectorClampCell(TruncateFloatToInt32((maxPoint.y+marginY)*gridScaleY), gridHeight);

    {
        (void) Graph->device->SetSamplerState(0u, D3DSAMP_MINFILTER, static_cast<unsigned int>(D3DTEXF_POINT));
        (void) Graph->device->SetSamplerState(0u, D3DSAMP_MAGFILTER, static_cast<unsigned int>(D3DTEXF_POINT));
        (void) Graph->device->SetTextureStageState(0u, D3DTSS_COLOROP, static_cast<unsigned int>(D3DTOP_MODULATE));
        (void) Graph->device->SetTextureStageState(0u, D3DTSS_COLORARG2, static_cast<unsigned int>(D3DTA_DIFFUSE));
        (void) Graph->device->SetTextureStageState(0u, D3DTSS_ALPHAOP, static_cast<unsigned int>(D3DTOP_MODULATE));
        (void) Graph->device->SetTextureStageState(0u, D3DTSS_ALPHAARG1, static_cast<unsigned int>(D3DTA_TEXTURE));
        (void) Graph->device->SetTextureStageState(0u, D3DTSS_ALPHAARG2, static_cast<unsigned int>(D3DTA_DIFFUSE));
        (void) Graph->device->SetRenderState(D3DRS_SPECULARENABLE, 0u);
        (void) Graph->device->SetRenderState(D3DRS_ALPHATESTENABLE, 1u);
        (void) Graph->device->SetRenderState(D3DRS_ALPHAREF, 0u);
        (void) Graph->device->SetRenderState(D3DRS_ALPHAFUNC, static_cast<unsigned int>(D3DCMP_GREATER));
        (void) Graph->device->SetRenderState(D3DRS_ALPHABLENDENABLE, 0u);
        (void) Graph->device->SetRenderState(D3DRS_ZWRITEENABLE, 1u);
        (void) Graph->device->SetRenderState(D3DRS_ZFUNC, static_cast<unsigned int>(D3DCMP_GREATEREQUAL));
        (void) Graph->device->SetRenderState(D3DRS_COLORWRITEENABLE, static_cast<int>(Graph->graphDetail)<8 ? 0u : 8u);
    }

    for (int y = maxY; y >= minY; --y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            SPRITE_LIST* const bucket = SpriteCollectorLayerBucket(this, 0, x, y);
            int item = 0;
            if (bucket->m_count != 0) do
                {
                    SPRITE* const sprite = bucket->m_data[item];
                    if (!sprite) break;
                    if ((sprite->RuntimeFlags()&0x04000000u) != 0u) continue;
                    sprite->Vid()->DrawOpaque(sprite);
                }
                while (++item<bucket->m_count);
        }
    }
}

void SPRITE_COLLECTOR::DrawLayer(int layer, const VECTOR2& minPoint, const VECTOR2& maxPoint) const
{
        ::Sleep(0u);

    if (layer == 11)
    {
        int item = depthSortedSprites.m_count-1;
        while (item >= 0)
        {
            SPRITE* const sprite = depthSortedSprites.m_data[item];
            if (!sprite) break;
            if ((sprite->RuntimeFlags()&0x04000000u) == 0u) sprite->Draw();

            const int count = depthSortedSprites.m_count;
            if (item>count) item = count;
            --item;
        }
        goto linkedSprites;
    }
    {
        VID::viewMin.x = minPoint.x;
        VID::viewMin.y = minPoint.y;
        VID::viewMax.x = maxPoint.x;
        VID::viewMax.y = maxPoint.y;

        if (Map->Flagman(Map->PlayerIndex()) &&
            Map->Flagman(Map->PlayerIndex())->Vid()->layer == layer &&
            Map->Flagman(Map->PlayerIndex())->Vid()->nVid/100 == 2)
        {
            for (SPRITE* sprite = Map->Flagman(Map->PlayerIndex()); sprite; sprite = sprite->ChildChain())
            {
                if ((sprite->RuntimeFlags()&0x04000000u) == 0u &&
                    sprite->Vid()->layer == layer)
                    sprite->Vid()->DrawOccluded(sprite);
            }
        }

        GRAPH* const graph = Graph;
        const float marginX = 2.0f/(graph->screenScaleX*gridScaleX);
        const int minX = SpriteCollectorClampCell(TruncateFloatToInt32((minPoint.x-marginX)*gridScaleX), gridWidth);
        const float marginY = 2.0f/(graph->screenScaleY*gridScaleY);
        const int minY = SpriteCollectorClampCell(TruncateFloatToInt32((minPoint.y-marginY)*gridScaleY), gridHeight);
        const int maxX = SpriteCollectorClampCell(TruncateFloatToInt32((maxPoint.x+marginX)*gridScaleX), gridWidth);
        const int maxY = SpriteCollectorClampCell(TruncateFloatToInt32((maxPoint.y+marginY)*gridScaleY), gridHeight);

        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                int item = 0;
                if (SpriteCollectorLayerBucket(this, layer, x, y)->m_count != 0) do
                    {
                        SPRITE* const sprite = SpriteCollectorLayerBucket(this, layer, x, y)->m_data[item];
                        if (!sprite) break;
                        if ((sprite->RuntimeFlags()&0x04000000u) != 0u) continue;
                        if ((Graph->drawFlags&2u) != 0u)
                            sprite->Vid()->DrawMesh(sprite);
                        else
                            sprite->Draw();
                    }
                    while (++item<SpriteCollectorLayerBucket(this, layer, x, y)->m_count);
            }
        }
    }
    linkedSprites:
        SPRITE* sprite = Mouse;
    if (!sprite) return;
    if (static_cast<MOUSE*>(sprite)->HardwareCursorEnabled() != 0u) return;
    if ((sprite->Vid()->property&0x00008000u) != 0u) return;
    for (; sprite; sprite = sprite->ChildChain())
    {
        if (sprite->Vid()->layer == layer && (sprite->RuntimeFlags()&0x04000000u) == 0u)
            sprite->Draw();
    }
}

namespace {
struct SpriteCollectorGroundVertex
{
    float x, y, z, rhw, u, v;
};

}

SPRITE_COLLECTOR::CELL::CELL()
{
    renderTargetGround = 0;
    sourceGroundTexture = 0;
}

void SPRITE_COLLECTOR::CELL::Insert(SPRITE* sprite)
{
    layerSprites[sprite->Vid()->layer].Insert(sprite);
    const VID* const vid = sprite->Vid();
    if ((vid->property&0x00001000u) != 0u &&
        (vid->type&0x40u) != 0u && (vid->type&0x80u) == 0u)
        layerSprites[15].Insert(sprite);
}

int SPRITE_COLLECTOR::CELL::Delete(SPRITE* sprite)
{
    int result = layerSprites[sprite->Vid()->layer].Delete(sprite);
    const VID* const vid = sprite->Vid();
    if ((vid->property&0x00001000u) != 0u &&
        (vid->type&0x40u) != 0u && (vid->type&0x80u) == 0u)
        result |= layerSprites[15].Delete(sprite);
    return result;
}

SPRITE_COLLECTOR::CELL::~CELL() noexcept(false)
{
    if (renderTargetGround)
    {
        TEXTURE* const releasedTexture = renderTargetGround;
        delete releasedTexture;
        renderTargetGround = 0;
    }
    if (sourceGroundTexture)
    {
        TEXTURE* const releasedTexture = sourceGroundTexture;
        delete releasedTexture;
        sourceGroundTexture = 0;
    }

    buildedSprites.Release();
    alphaBuildedSprites.Release();
    sprites.Release();
    for (unsigned int i = 0u; i<15u; ++i) layerSprites[i].Release();
}

void SPRITE_COLLECTOR::DrawNamed() const
{
    for (int index = 0; index < namedSprites.No(); ++index)
    {
        const STRING name = (*namedSprites[index])->GetName();
        Graph->PutsXY((*namedSprites[index])->ScreenXY(), name, COLOR(255, 255, 255));
    }
}

int SPRITE_COLLECTOR::GetNoDrawedBuildedGround() const
{
    return 0;
}

void SPRITE_COLLECTOR::CELL::CreateRenderTargetGround()
{
    const D3DFORMAT format = sourceGroundTexture
        ? sourceGroundTexture->m_format
        : static_cast<D3DFORMAT>(Graph->builtPixelFormat);

    if (renderTargetGround && renderTargetGround->m_format == format) return;

    if (renderTargetGround)
    {
        TEXTURE* const releasedTexture = renderTargetGround;
        delete releasedTexture;
    }

    TEXTURE* const texture = new(std::nothrow) TEXTURE(256, 256, format,
        format == D3DFMT_R5G6B5 ? 2u : 3u);

    renderTargetGround = texture;
    if (texture->m_texture == 0 && texture->m_cpuData == 0)
    {
        if (texture)
        {
            delete texture;
        }
        renderTargetGround = 0;
    }
}

void SPRITE_COLLECTOR::RestoreDeviceObjects()
{
    GRAPH* const graph = Graph;
    const float viewWidth = graph->screenWidth;
    const int visibleCellsY = BitsToSigned32(static_cast<unsigned int>(TruncateFloatToInt32(graph->screenHeight/graph->screenScaleY)/256)+2u);
    const int visibleCellsX = BitsToSigned32(static_cast<unsigned int>(TruncateFloatToInt32(viewWidth/graph->screenScaleX)/256)+2u);
    int wanted = BitsToSigned32(static_cast<unsigned int>(visibleCellsY)*static_cast<unsigned int>(visibleCellsX));
    if (wanted>130) wanted = 130;
    const int totalCells = BitsToSigned32(static_cast<unsigned int>(gridHeight)*static_cast<unsigned int>(gridWidth));
    if (wanted>totalCells) wanted = totalCells;

    if (renderTargetGroundCount>wanted)
    {
        do
        {
            if (renderTargetGroundCells[renderTargetGroundCount-1]->renderTargetGround)
            {
                {
                    TEXTURE* const releasedTexture = renderTargetGroundCells[renderTargetGroundCount-1]->renderTargetGround;
                    delete releasedTexture;
                }
                renderTargetGroundCells[renderTargetGroundCount-1]->renderTargetGround = 0;
            }
            renderTargetGroundCells[renderTargetGroundCount-1] = 0;
            --renderTargetGroundCount;
        }
        while (renderTargetGroundCount>wanted);
        return;
    }
    if (renderTargetGroundCount >= wanted) return;
    do
    {
        const int count = renderTargetGroundCount;
        int freeCellIndex = 0;
        for (; freeCellIndex <= count; ++freeCellIndex)
        {
            bool used = false;
            CELL* const base = spatialCells;
            for (int i = 0; i<count; ++i)
            {
                const int index = BitsToSigned32(PtrToU32(renderTargetGroundCells[i])-PtrToU32(base))/320;
                if (index == freeCellIndex)
                {
                    used = true;
                    break;
                }
            }
            if (!used) break;
        }

        renderTargetGroundCells[count] = &spatialCells[freeCellIndex];
        spatialCells[freeCellIndex].CreateRenderTargetGround();
        if (!spatialCells[freeCellIndex].renderTargetGround)
        {
            const STRING subject("video memory for create renderTargetGround for restoredeviceobjects");
            if (::Error)
            {
                const STRING object("SPRITE_COLLECTOR");
                    ::Error->Error(object, E_MEMORY, subject, 0u);
            }
        }
        else if (spatialCells[freeCellIndex].sourceGroundTexture)
        {
            IDirect3DDevice9* const device = Graph->device;
            IDirect3DSurface9* const target = spatialCells[freeCellIndex].renderTargetGround->GetSurface();
            IDirect3DSurface9* const source = spatialCells[freeCellIndex].sourceGroundTexture->GetSurface();
            const long hr = device->UpdateSurface(source, 0, target, 0);
            if (hr<0)
            {
                const STRING subject("updateTexture");
                if (::Error)
                {
                    const STRING object("SPRITE_COLLECTOR");
                        ::Error->Error(object, E_COPY, subject, static_cast<unsigned long>(hr));
                }
            }
        }
        ++renderTargetGroundCount;
    }
    while (renderTargetGroundCount<wanted);
}

#include "unit.h"
#include "script.h"

static bool SpriteCollectorNearestCandidateMatches(SPRITE* candidate, unsigned int type, unsigned int categoryMask, unsigned int directionMask, unsigned int lowTypeId)
{
    SPRITE* const sprite = candidate;
    if (!sprite || sprite->ChildBacklink() != 0) return false;

    VID* const owner = sprite->Vid();
    if (!owner) return false;
    if ((owner->spriteType&categoryMask) == 0u) return false;

    const unsigned int spriteFlags = sprite->RuntimeFlags();
    const unsigned int directionBit = 0x1000u<<((spriteFlags>>17)&7u);
    if ((directionMask&directionBit) == 0u) return false;
    if ((spriteFlags&0x00000800u) != 0u) return false;
    if ((type&0x80000000u) != 0u && (spriteFlags&0x7Cu) != 0u) return false;

    if ((type&0x20000000u) != 0u)
    {
        if (owner->spriteClass != lowTypeId) return false;
    }
    else if (lowTypeId != 0u)
    {
        if (static_cast<unsigned int>(owner->nVid) != lowTypeId) return false;
    }
    return true;
}

static float SpriteCollectorNearestMetric(const VECTOR2& point, const SPRITE* candidate)
{
    const SPRITE* const sprite = candidate;
    const VECTOR2 sprite2(sprite->Coor().x, sprite->Coor().y);
    const VECTOR2 delta = point-sprite2;
    return delta.NearLength();
}

static void SpriteCollectorConsiderNearest(SPRITE* candidate, unsigned int type, unsigned int categoryMask, unsigned int directionMask, unsigned int lowTypeId, const VECTOR2& point, float previousDistance, float& bestDistance, SPRITE*& best)
{
    if (!SpriteCollectorNearestCandidateMatches(candidate, type, categoryMask, directionMask, lowTypeId)) return;
    const float metric = SpriteCollectorNearestMetric(point, candidate);
    if (metric<bestDistance && metric>previousDistance)
    {
        bestDistance = metric;
        best = candidate;
    }
}

static bool SpriteCollectorTypeOwnerHasBuckets(const VID* owner)
{
    unsigned int total = 0u;
    for (unsigned int i = 0u; i<VID::PlayerSlotCount; ++i) total += owner->spriteCountsByPlayer[i];
    return total != 0u;
}

SPRITE* SPRITE_COLLECTOR::FindNearestSprite(int typeValue, const VECTOR2& pointValue, float radius, SPRITE* previousSprite)
{
    unsigned int type = static_cast<unsigned int>(typeValue);
    const VECTOR2& point = pointValue;
    SPRITE* const previous = previousSprite;
    if (type == 0u) return 0;

    SPRITE* best = 0;
    float bestDistance = radius;
    float previousDistance = -1.0f;
    if (previous) previousDistance = SpriteCollectorNearestMetric(point, previous);

    unsigned int directionMask = type&0x000FF000u;
    if (directionMask == 0u) directionMask = 0x000FF000u;

    const unsigned int lowTypeId = type&0x00000FFFu;
    unsigned int categoryMask = 0u;
    VID* resolvedTypeOwner = 0;

    if (lowTypeId != 0u && (type&0x20000000u) == 0u)
    {
        resolvedTypeOwner = Map->Vid(static_cast<int>(lowTypeId));
        if (!SpriteCollectorTypeOwnerHasBuckets(resolvedTypeOwner)) return 0;
        if ((resolvedTypeOwner->property&0x40u) != 0u) type |= 0x10000000u;
        categoryMask = resolvedTypeOwner->spriteType;
    }
    else
    {
        categoryMask = (static_cast<unsigned int>(static_cast<int>(type)>>20))&0x000006FFu;
        if (categoryMask == 0u) categoryMask = 0x000006FFu;
    }

    if ((type&0x10000000u) != 0u)
    {
        SPRITE* candidate = this->FirstHashInBox(point.x-radius, point.y-radius, point.x+radius, point.y+radius);
        while (candidate)
        {
            SpriteCollectorConsiderNearest(candidate, type, categoryMask, directionMask, lowTypeId, point, previousDistance, bestDistance, best);
            candidate = this->NextHashInBox();
        }
        return best;
    }

    if ((categoryMask&0x18u) != 0u && (categoryMask&0x06E7u) == 0u)
    {
        for (int lane = 0; lane<8; ++lane)
        {
            int cursor = 0;
            SPRITE* candidate = this->FirstUnit(&cursor, lane);
            while (candidate)
            {
                SpriteCollectorConsiderNearest(candidate, type, categoryMask, directionMask, lowTypeId, point, previousDistance, bestDistance, best);
                candidate = this->NextUnit(&cursor);
            }
        }
        return best;
    }

    if (categoryMask == 0x20u)
    {
        const MENU& menu = Map->MenuState();
        int index = menu.m_count-1;
        while (index >= 0)
        {
            SPRITE* candidate = menu.m_data[index];
            if (candidate) SpriteCollectorConsiderNearest(candidate, type, categoryMask, directionMask, lowTypeId, point, previousDistance, bestDistance, best);
            const int count = menu.m_count;
            if (index>count) index = count;
            --index;
        }
        return best;
    }

    if (lowTypeId != 0u && (type&0x20000000u) == 0u)
    {
        if (!resolvedTypeOwner) resolvedTypeOwner = Map->Vid(static_cast<int>(lowTypeId));
        if ((resolvedTypeOwner->runtimeFlags&0x80u) != 0u)
        {
            SPRITE_COLLECTOR* const collector = this;
            int cursor = collector->allSprites.m_count;
            LIST<SPRITE*>* vectorOwner = &collector->allSprites;
            SPRITE* candidate = vectorOwner->NextIterateNon0(&cursor);
            while (candidate)
            {
                SpriteCollectorConsiderNearest(candidate, type, categoryMask, directionMask, lowTypeId, point, previousDistance, bestDistance, best);
                candidate = vectorOwner->NextIterateNon0(&cursor);
            }
            return best;
        }
    }

    SPRITE_COLLECTOR* const collector = this;
    SPRITE* candidate = collector->FirstAllSprite();
    while (candidate)
    {
        SpriteCollectorConsiderNearest(candidate, type, categoryMask, directionMask, lowTypeId, point, previousDistance, bestDistance, best);
        candidate = collector->NextAllSprite();
    }
    return best;
}

static bool SpriteCollectorScreenCandidateFilter(SPRITE* candidate, unsigned int type, unsigned int categoryMask, unsigned int directionMask, unsigned int lowTypeId, SPRITE* excluded)
{
    if (candidate == excluded) return false;
    SPRITE* const sprite = candidate;
    VID* const owner = sprite->Vid();
    if ((owner->spriteType&categoryMask) == 0u) return false;

    const unsigned int spriteFlags = sprite->RuntimeFlags();
    const unsigned int directionBit = 0x1000u<<((spriteFlags>>17)&7u);
    if ((directionMask&directionBit) == 0u || (spriteFlags&0x800u) != 0u) return false;
    if ((type&0x80000000u) != 0u && (spriteFlags&0x7Cu) != 0u) return false;

    if ((type&0x20000000u) != 0u)
    {
        if (owner->spriteClass != lowTypeId) return false;
    }
    else if (lowTypeId != 0u && static_cast<unsigned int>(owner->nVid) != lowTypeId)
    {
        return false;
    }
    return true;
}

static bool SpriteCollectorScreenCandidateMatches(SPRITE* candidate, unsigned int type, unsigned int categoryMask, unsigned int directionMask, unsigned int lowTypeId, const VECTOR2& screenPoint, SPRITE* excluded)
{
    if (!SpriteCollectorScreenCandidateFilter(candidate, type, categoryMask, directionMask, lowTypeId, excluded)) return false;
    SPRITE* const sprite = candidate;
    VID* const owner = sprite->Vid();
    const float extentX = owner->halfSizeXY.x;
    const float x = sprite->Coor().x;
    if (screenPoint.x < x-extentX || !(x+extentX >= screenPoint.x)) return false;

    const float baseline = sprite->Coor().y-sprite->Coor().z;
    const float extentY = owner->halfSizeXY.y;
    const float lower = baseline-owner->sizeXYZ.z-extentY;
    const float upper = baseline+extentY;
    return !(lower >= screenPoint.y) && screenPoint.y < upper;
}

static bool SpriteCollectorType17ScreenGeometryMatches(SPRITE* candidate, VID* owner, const VECTOR2& screenPoint)
{
    const VECTOR2 projectedBase = candidate->Coor().ViewCoor();

    const VECTOR2 halfSize = VECTOR2(static_cast<const REGION*>(candidate)->HalfWidth(), static_cast<const REGION*>(candidate)->HalfHeight())/2.0f;

    VECTOR2 lower(projectedBase.x-halfSize.x, projectedBase.y-halfSize.y);
    lower.y -= owner->sizeXYZ.z;
    if (!(screenPoint >= lower)) return false;

    const VECTOR2 candidateXY(candidate->X(), candidate->Y());
    const VECTOR2 upper = candidateXY+halfSize;
    return (screenPoint <= upper) != 0;
}

static bool SpriteCollectorScreenCandidateRanksBefore(SPRITE* candidate, SPRITE* current)
{
    if (!current) return true;
    VID* const candidateOwner = candidate->Vid();
    VID* const currentOwner = current->Vid();
    return !(candidateOwner->sizeXYZ.XY() >= currentOwner->sizeXYZ.XY());
}

static void SpriteCollectorConsiderScreenCandidate(SPRITE* candidate, unsigned int type, unsigned int categoryMask, unsigned int directionMask, unsigned int lowTypeId, const VECTOR2& screenPoint, SPRITE* excluded, SPRITE*& best)
{
    if (candidate->ChildBacklink() != 0) return;
    if (!SpriteCollectorScreenCandidateMatches(candidate, type, categoryMask, directionMask, lowTypeId, screenPoint, excluded)) return;
    if (SpriteCollectorScreenCandidateRanksBefore(candidate, best)) best = candidate;
}

void SPRITE_COLLECTOR::BuildAllTerrain()
{
    GRAPH* const entryGraph = Graph;
    const VECTOR2 savedViewMin(entryGraph->viewLeft, entryGraph->viewTop);
    const VECTOR2 savedViewMax(entryGraph->viewRight, entryGraph->viewBottom);
    const VECTOR2 savedMapPoint(Map->ShiftXY());
    const VECTOR savedScale(entryGraph->screenScaleX, entryGraph->screenScaleY, entryGraph->screenScaleZ);

    TEXTURE* renderTexture = new(std::nothrow) TEXTURE(256, 256, D3DFMT_A8R8G8B8, 2u);
    TEXTURE* systemTexture = new(std::nothrow) TEXTURE(256, 256, D3DFMT_A8R8G8B8, 4u);
    IDirect3DSurface9* oldRenderTarget;
    IDirect3DSurface9* oldDepthSurface;
    IDirect3DSurface9* buildDepthSurface;
    (void) Graph->device->GetRenderTarget(0u, &oldRenderTarget);
    (void) Graph->device->GetDepthStencilSurface(&oldDepthSurface);

    unsigned int depthFormat;
    GRAPH* formatGraph = Graph;
    if (formatGraph->direct3D->CheckDepthStencilMatch(formatGraph->selectedAdapter, D3DDEVTYPE_HAL,
        static_cast<D3DFORMAT>(formatGraph->desktopFormat), static_cast<D3DFORMAT>(renderTexture->m_format), static_cast<D3DFORMAT>(static_cast<unsigned int>(D3DFMT_D16))) >= 0)
        depthFormat = static_cast<unsigned int>(D3DFMT_D16);
    else
    {
        formatGraph = Graph;
        if (formatGraph->direct3D->CheckDepthStencilMatch(formatGraph->selectedAdapter, D3DDEVTYPE_HAL,
            static_cast<D3DFORMAT>(formatGraph->desktopFormat), static_cast<D3DFORMAT>(renderTexture->m_format), static_cast<D3DFORMAT>(static_cast<unsigned int>(D3DFMT_D24X8))) >= 0)
            depthFormat = static_cast<unsigned int>(D3DFMT_D24X8);
        else
        {
            formatGraph = Graph;
            if (formatGraph->direct3D->CheckDepthStencilMatch(formatGraph->selectedAdapter, D3DDEVTYPE_HAL,
                static_cast<D3DFORMAT>(formatGraph->desktopFormat), static_cast<D3DFORMAT>(renderTexture->m_format), static_cast<D3DFORMAT>(static_cast<unsigned int>(D3DFMT_D24S8))) >= 0)
                depthFormat = static_cast<unsigned int>(D3DFMT_D24S8);
        }
    }
    const long depthResult = Graph->device->CreateDepthStencilSurface(256u, 256u,
        static_cast<D3DFORMAT>(depthFormat), D3DMULTISAMPLE_NONE, 0u, TRUE, &buildDepthSurface, 0);
    const unsigned int systemFormat = systemTexture->m_format;
    const unsigned int renderFormat = renderTexture->m_format;
    if (depthResult<0)
    {
        const STRING subject("depth256x256 for BuildAll()");
        if (::Error)
        {
            const STRING object("SPRITE_COLLECTOR");
                ::Error->Error(object, E_CREATE, subject, static_cast<unsigned long>(depthResult));
        }
    }

    for (int y = 0; y<gridHeight; ++y)
    {
        for (int x = 0; x<gridWidth; ++x)
        {
            CELL& cell = spatialCells[(static_cast<unsigned int>(y)<<(gridShift&31))+static_cast<unsigned int>(x)];
            if (cell.buildedSprites.m_count == 0 && cell.alphaBuildedSprites.m_count == 0) continue;

            const VECTOR2 targetMin(0.0f, 0.0f);
            const VECTOR2 targetMax(256.0f, 256.0f);
            IDirect3DSurface9* const renderSurface = renderTexture->GetSurface();
            const long targetResult = Graph->SetRenderTarget(
                renderSurface,
                targetMin, targetMax,
                buildDepthSurface);
            const VECTOR unitScale(1.0f, 1.0f, 1.0f);
            Graph->SetScreenScale(unitScale);
            if (targetResult<0)
            {
                const STRING subject("rt and depth256x256 for BuildAll()");
                if (::Error)
                {
                    const STRING object("SPRITE_COLLECTOR");
                        ::Error->Error(object, E_SET, subject, static_cast<unsigned long>(targetResult));
                }
            }

            const VECTOR2 projected = VECTOR2(x, y)*256.0f;
            Map->SetShiftCoor(projected+Graph->SizeXY()/2.0f, 0x10000000);

            {
                COLOR clearColor;
                clearColor.value = 0xFF000000u;
                Graph->ClearScreen(clearColor);
            }
            VID::viewMin = VECTOR2(x, y)*256.0f;
            VID::viewMax = VECTOR2(x+1, y+1)*256.0f;

            int item = 0;
            if (cell.buildedSprites.m_count != 0) do
                {
                    SPRITE* const sprite = cell.buildedSprites.m_data[item];
                    if (!sprite) break;
                    if ((sprite->RuntimeFlags()&0x04000000u) == 0u) sprite->Draw();
                }
                while (++item<cell.buildedSprites.m_count);
            item = 0;
            if (cell.alphaBuildedSprites.m_count != 0) do
                {
                    SPRITE* const sprite = cell.alphaBuildedSprites.m_data[item];
                    if (!sprite) break;
                    if ((sprite->RuntimeFlags()&0x04000000u) == 0u) sprite->Draw();
                }
                while (++item<cell.alphaBuildedSprites.m_count);

            cell.sourceGroundTexture = new(std::nothrow) TEXTURE(256, 256, D3DFMT_DXT1, 4u);
            if (cell.sourceGroundTexture->m_texture == 0 && cell.sourceGroundTexture->m_cpuData == 0)
            {
                if (cell.sourceGroundTexture)
                {
                    {
                        TEXTURE* const releasedTexture = cell.sourceGroundTexture;
                        delete releasedTexture;
                    }
                    cell.sourceGroundTexture = 0;
                }
                const STRING subject("system memory texture for BuildAll");
                if (::Error)
                {
                    const STRING object("SPRITE_COLLECTOR");
                        ::Error->Error(object, E_CREATE, subject, 0u);
                }
                continue;
            }
            else
            {
                if (cell.sourceGroundTexture->m_format != static_cast<unsigned int>(D3DFMT_DXT1))
                {
                    const STRING subject("system memory texture not DXT1");
                    if (::Error)
                    {
                        const STRING object("SPRITE_COLLECTOR");
                            ::Error->Error(object, E_CREATE, subject, 0u);
                    }
                }

                IDirect3DDevice9* const device = Graph->device;
                IDirect3DSurface9* const targetSurface = systemTexture->GetSurface();
                IDirect3DSurface9* const sourceSurface = renderTexture->GetSurface();
                const long readbackResult = device->GetRenderTargetData(sourceSurface, targetSurface);
                if (readbackResult<0)
                {
                    const STRING subject("rendertargetdata for BuildAll");
                    if (::Error)
                    {
                        const STRING object("SPRITE_COLLECTOR");
                            ::Error->Error(object, E_COPY, subject, static_cast<unsigned long>(readbackResult));
                    }
                }

                IDirect3DSurface9* const systemSurface = systemTexture->GetSurface();
                IDirect3DSurface9* const destinationSurface = cell.sourceGroundTexture->GetSurface();
                const long copyResult = D3DXLoadSurfaceFromSurface(
                    destinationSurface, 0, 0, systemSurface, 0, 0, 1u, 0u);
                if (copyResult<0)
                {
                    const STRING subject("D3DXLoadSurfaceFromSurface for BuildAll");
                    if (::Error)
                    {
                        const STRING object("SPRITE_COLLECTOR");
                            ::Error->Error(object, E_COPY, subject, static_cast<unsigned long>(copyResult));
                    }
                }
                else
                    cell.buildedSprites.Release();
            }

            (void) Graph->SetRenderTarget(
                oldRenderTarget,
                savedViewMin, savedViewMax,
                oldDepthSurface);
            Graph->SetScreenScale(savedScale);
            DispatchMapPresentation(0u);
        }
    }

    int totalCells = BitsToSigned32(static_cast<unsigned int>(gridHeight)*static_cast<unsigned int>(gridWidth));
    for (int index = BitsToSigned32(static_cast<unsigned int>(totalCells)-1u); index >= 0; --index)
        spatialCells[index].layerSprites[2].DeleteAll();

    totalCells = BitsToSigned32(static_cast<unsigned int>(gridHeight)*static_cast<unsigned int>(gridWidth));
    for (int index = BitsToSigned32(static_cast<unsigned int>(totalCells)-1u); index >= 0; --index)
    {
        if (spatialCells[index].buildedSprites.m_count != 0)
        {
            const STRING subject("exist builded sprite after delete");
            const int id = spatialCells[index].buildedSprites.m_data[0]->Vid()->nVid;
            if (::Error)
            {
                const STRING object("SPRITE_COLLECTOR");
                    ::Error->Error(object, E_ERROR, subject, static_cast<unsigned long>(id));
            }
        }
        if (spatialCells[index].alphaBuildedSprites.m_count != 0)
        {
            const STRING subject("exist alpha builded sprite after delete");
            const int id = spatialCells[index].alphaBuildedSprites.m_data[0]->Vid()->nVid;
            if (::Error)
            {
                const STRING object("SPRITE_COLLECTOR");
                    ::Error->Error(object, E_ERROR, subject, static_cast<unsigned long>(id));
            }
        }
    }

    Map->SetShiftCoor(savedMapPoint+Graph->SizeXY()/2.0f, 0x10000000);

    if (buildDepthSurface)
    {
        (void) buildDepthSurface->Release();
        buildDepthSurface = 0;
    }
    if (oldDepthSurface)
    {
        (void) oldDepthSurface->Release();
        oldDepthSurface = 0;
    }
    if (oldRenderTarget)
    {
        (void) oldRenderTarget->Release();
        oldRenderTarget = 0;
    }
    if (renderTexture)
    {
        delete renderTexture;
    }
    if (systemTexture)
    {
        delete systemTexture;
    }

    Graph->InvalidateDeviceObjects();
    Graph->RestoreDeviceObjects();

    const unsigned int freeVideoMemory = Graph->device->GetAvailableTextureMem();
    STRING depthName = GetPixelFormat(static_cast<_D3DFORMAT>(depthFormat));
    STRING systemName = GetPixelFormat(static_cast<_D3DFORMAT>(systemFormat));
    STRING renderName = GetPixelFormat(static_cast<_D3DFORMAT>(renderFormat));
    (void) ::Error->Log("Build all terrain %s %s %s free video memory = %i", renderName.value, systemName.value, depthName.value, freeVideoMemory);
}

void SPRITE_COLLECTOR::DrawBuilded(const VECTOR2 &minPoint, const VECTOR2 &maxPoint,
    IDirect3DSurface9 *sourceSurface)
{
    if (renderTargetGroundCount == 0)
    {
        {
            const STRING subject("not created drawGroundCells for DrawBuilded");
            if (::Error)
            {
                const STRING object("SPRITE_COLLECTOR");
                    ::Error->Error(object, E_ERROR, subject, 0ul);
            }
        }
        return;
    }

    const float marginX = 1.0f / gridScaleX;
    GRAPH *const entryGraph = Graph;
    const VECTOR2 savedViewMin(entryGraph->viewLeft, entryGraph->viewTop);
    const VECTOR2 savedViewMax(entryGraph->viewRight, entryGraph->viewBottom);
    const VECTOR2 savedMapPoint(Map->ShiftXY());
    IDirect3DSurface9 *oldDepthSurface = 0;

    const int expandedMinX =
        SpriteCollectorClampCell(TruncateFloatToInt32((minPoint.x - marginX) * gridScaleX), gridWidth);
    const float marginY = 1.0f / gridScaleY;
    const int expandedMinY =
        SpriteCollectorClampCell(TruncateFloatToInt32((minPoint.y - marginY) * gridScaleY), gridHeight);
    const int expandedMaxX =
        SpriteCollectorClampCell(TruncateFloatToInt32((maxPoint.x + marginX) * gridScaleX), gridWidth);
    const int expandedMaxY =
        SpriteCollectorClampCell(TruncateFloatToInt32((maxPoint.y + marginY) * gridScaleY), gridHeight);
    const int viewMinX = SpriteCollectorClampCell(TruncateFloatToInt32(minPoint.x * gridScaleX), gridWidth);
    const int viewMinY = SpriteCollectorClampCell(TruncateFloatToInt32(minPoint.y * gridScaleY), gridHeight);
    const int viewMaxX = SpriteCollectorClampCell(TruncateFloatToInt32(maxPoint.x * gridScaleX), gridWidth);
    const int viewMaxY = SpriteCollectorClampCell(TruncateFloatToInt32(maxPoint.y * gridScaleY), gridHeight);

    noFreeBuildedGround = 0;

    for (int y = viewMinY; y <= viewMaxY; ++y)
    {
        for (int x = viewMinX; x <= viewMaxX; ++x)
        {
            CELL &cell =
                spatialCells[(static_cast<unsigned int>(y) << (static_cast<unsigned int>(gridShift) & 31u)) +
            static_cast<unsigned int>(x)];
            if (!((VECTOR2(cell.originX, cell.originY) + VECTOR2(128.0f, 128.0f)) > minPoint &&
                VECTOR2(cell.originX, cell.originY) < (maxPoint + VECTOR2(128.0f, 128.0f))) ||
                cell.renderTargetGround)
                continue;

            const int farIndex = GetFarGroundCell(minPoint, maxPoint);
            if (farIndex < 0)
            {
                {
                    const STRING subject("can't find cell for building for DrawBuilded()");
                    if (::Error)
                    {
                        const STRING object("SPRITE_COLLECTOR");
                            ::Error->Error(object, E_ERROR, subject,
                            0ul);
                    }
                }
                continue;
            }

            noFreeBuildedGround = AddSigned32Wrap(noFreeBuildedGround, 1);
            cell.renderTargetGround = renderTargetGroundCells[farIndex]->renderTargetGround;
            renderTargetGroundCells[farIndex]->renderTargetGround = 0;
            renderTargetGroundCells[farIndex] = &cell;
            cell.CreateRenderTargetGround();
            if (!cell.renderTargetGround)
            {
                const STRING subject("renderTargetGround0 for DrawBuilded()");
                if (::Error)
                {
                    const STRING object("SPRITE_COLLECTOR");
                        ::Error->Error(object, E_CREATE, subject,
                        0ul);
                }
            }
            if (cell.sourceGroundTexture)
            {
                IDirect3DDevice9 *const updateDevice = Graph->device;
                IDirect3DSurface9 *const targetSurface = cell.renderTargetGround->GetSurface();
                IDirect3DSurface9 *const sourceGroundSurface = cell.sourceGroundTexture->GetSurface();
                const int hr =
                    static_cast<int>(updateDevice->UpdateSurface(sourceGroundSurface, 0, targetSurface, 0));
                if (hr < 0)
                {
                    const STRING subject("updateTexture0 for DrawBuilded()");
                    if (::Error)
                    {
                        const STRING object("SPRITE_COLLECTOR");
                            ::Error->Error(object, E_COPY, subject,
                            static_cast<unsigned long>(hr));
                    }
                }
            }
        }
    }

    if (noFreeBuildedGround < 2)
    {
        const int farIndex = GetFarGroundCell(minPoint, maxPoint);
        if (farIndex >= 0)
        {
            const float middleXSum = minPoint.x + maxPoint.x;
            const float middleY = (minPoint.y + maxPoint.y) * 0.5f;
            const VECTOR2 middle(middleXSum * 0.5f, middleY);
            CELL *candidate = 0;
            float best = 100000000.0f;
            for (int y = expandedMinY; y <= expandedMaxY; ++y)
            {
                const int xs[2] = {expandedMinX, expandedMaxX};
                for (int side = 0; side < 2; ++side)
                {
                    CELL &cell = spatialCells[(static_cast<unsigned int>(y)
                        << (static_cast<unsigned int>(gridShift) & 31u)) +
                    static_cast<unsigned int>(xs[side])];
                    if (cell.renderTargetGround)
                        continue;
                    const float d = VECTOR2(middle.x - cell.originX, middle.y - cell.originY).NearLength();
                    if (d < best)
                    {
                        best = d;
                        candidate = &cell;
                    }
                }
            }
            for (int x = expandedMinX; x <= expandedMaxX; ++x)
            {
                const int ys[2] = {expandedMinY, expandedMaxY};
                for (int side = 0; side < 2; ++side)
                {
                    CELL &cell = spatialCells[(static_cast<unsigned int>(ys[side])
                        << (static_cast<unsigned int>(gridShift) & 31u)) +
                    static_cast<unsigned int>(x)];
                    if (cell.renderTargetGround)
                        continue;
                    const float d = VECTOR2(middle.x - cell.originX, middle.y - cell.originY).NearLength();
                    if (d < best)
                    {
                        best = d;
                        candidate = &cell;
                    }
                }
            }
            CELL *const oldCell = renderTargetGroundCells[farIndex];
            if (candidate && candidate != oldCell &&
                best < VECTOR2(middle.x - oldCell->originX, middle.y - oldCell->originY).NearLength())
            {
                noFreeBuildedGround = AddSigned32Wrap(noFreeBuildedGround, 1);
                candidate->renderTargetGround = renderTargetGroundCells[farIndex]->renderTargetGround;
                renderTargetGroundCells[farIndex]->renderTargetGround = 0;
                renderTargetGroundCells[farIndex] = candidate;
                candidate->CreateRenderTargetGround();
                if (!candidate->renderTargetGround)
                {
                    const STRING subject("renderTargetGround for drawbuilded");
                    if (::Error)
                    {
                        const STRING object("SPRITE_COLLECTOR");
                            ::Error->Error(object, E_CREATE, subject,
                            0ul);
                    }
                }
                if (candidate->sourceGroundTexture)
                {
                    IDirect3DDevice9 *const updateDevice = Graph->device;
                    IDirect3DSurface9 *const targetSurface = candidate->renderTargetGround->GetSurface();
                    IDirect3DSurface9 *const sourceGroundSurface =
                        candidate->sourceGroundTexture->GetSurface();
                    const int hr = static_cast<int>(
                        updateDevice->UpdateSurface(sourceGroundSurface, 0, targetSurface, 0));
                    if (hr < 0)
                    {
                        const STRING subject("updateTexture for drawbuilded");
                        if (::Error)
                        {
                            const STRING object(
                                "SPRITE_COLLECTOR");
                                ::Error->Error(object, E_COPY, subject,
                                static_cast<unsigned long>(hr));
                        }
                    }
                }
            }
        }
    }

    for (int index = 0; index < renderTargetGroundCount; ++index)
    {
        CELL *const cell = renderTargetGroundCells[index];
        if (cell->buildedSprites.m_count == 0 && cell->alphaBuildedSprites.m_count == 0)
            continue;
        const unsigned int previous = groundCellBuildAttempts;
        groundCellBuildAttempts = previous + 1u;
        if (previous <= 30u && cell->alphaBuildedSprites.m_count <= 100)
            continue;

        const float cx = cell->originX, cy = cell->originY;
        const VECTOR2 cellMin(cx - 128.0f, cy - 128.0f);
        const VECTOR2 cellMax(cx + 128.0f, cy + 128.0f);
        VID::viewMin.x = cellMin.x;
        VID::viewMin.y = cellMin.y;
        VID::viewMax.x = cellMax.x;
        VID::viewMax.y = cellMax.y;

        bool bakeEdge = (Graph->sceneState & 2u) != 0u || (Graph->capabilityFlags & 0x2000u) == 0u;
        if (!bakeEdge)
        {
            const float sx0 = cellMin.x - minPoint.x, sy0 = cellMin.y - minPoint.y;
            const float sx1 = cellMax.x - minPoint.x, sy1 = cellMax.y - minPoint.y;
            bakeEdge = !(sx0 >= savedViewMin.x) || !(sy0 >= savedViewMin.y) || sx1 >= savedViewMax.x ||
            sy1 >= savedViewMax.y;
        }
        if (!bakeEdge)
            continue;

        const VECTOR savedScale(Graph->screenScaleX, Graph->screenScaleY, Graph->screenScaleZ);
        const VECTOR unitScale(1.0f, 1.0f, 1.0f);
        Graph->SetScreenScale(unitScale);

        (void) Graph->device->GetDepthStencilSurface(&oldDepthSurface);

        TEXTURE *compressedTarget = 0;
        if (cell->renderTargetGround->m_format == static_cast<unsigned int>(D3DFMT_DXT1))
        {
            compressedTarget = cell->renderTargetGround;
            cell->renderTargetGround = new(std::nothrow) TEXTURE(256, 256, static_cast<D3DFORMAT>(Graph->builtPixelFormat), 2u);
        }

        const VECTOR2 targetMin(0.0f, 0.0f), targetMax(256.0f, 256.0f);
        IDirect3DSurface9 *const renderSurface = cell->renderTargetGround->GetSurface();
        const long setResult =
            Graph->SetRenderTarget(renderSurface, targetMin, targetMax,
            reinterpret_cast<IDirect3DSurface9 *>(Graph->lightDepthSurface));
        if (setResult < 0)
        {
            const STRING subject("rendet target for DrawBuilded");
            if (::Error)
            {
                const STRING object("SPRITE_COLLECTOR");
                    ::Error->Error(object, E_SET, subject,
                    static_cast<unsigned long>(static_cast<int>(setResult)));
            }
        }

        const unsigned int clearFlags = cell->sourceGroundTexture ? 2u : 3u;
        (void) Graph->device->Clear(0u, 0, clearFlags, 0u, 0.0f, 0u);

        if (!cell->sourceGroundTexture || cell->sourceGroundTexture->m_format == static_cast<unsigned int>(D3DFMT_DXT1))
        {
            if (cell->sourceGroundTexture)
            {
                {
                    TEXTURE* const releasedTexture = cell->sourceGroundTexture;
                    delete releasedTexture;
                }
                cell->sourceGroundTexture = 0;
            }
            cell->sourceGroundTexture = new(std::nothrow) TEXTURE(256, 256, static_cast<D3DFORMAT>(Graph->builtPixelFormat), 4u);
        }

        if (compressedTarget)
        {
            SpriteCollectorGroundVertex v[4];
            const float nearOne = 1.0f;
            const float uv0 = 0.0019529999699443579f;
            const float uv1 = 1.0019530f;
            v[0].x = 0.0f;
            v[0].y = 0.0f;
            v[0].z = 0.99999988f;
            v[0].rhw = nearOne;
            v[0].u = uv0;
            v[0].v = uv0;
            v[1].x = 256.0f;
            v[1].y = 0.0f;
            v[1].z = 0.99999988f;
            v[1].rhw = nearOne;
            v[1].u = uv1;
            v[1].v = uv0;
            v[2].x = 256.0f;
            v[2].y = 256.0f;
            v[2].z = 0.99999988f;
            v[2].rhw = nearOne;
            v[2].u = uv1;
            v[2].v = uv1;
            v[3].x = 0.0f;
            v[3].y = 256.0f;
            v[3].z = 0.99999988f;
            v[3].rhw = nearOne;
            v[3].u = uv0;
            v[3].v = uv1;

            (void) Graph->device->SetTextureStageState(0u, D3DTSS_COLORARG1, static_cast<unsigned int>(D3DTA_TEXTURE));
            (void) Graph->device->SetTextureStageState(0u, D3DTSS_ALPHAARG1, static_cast<unsigned int>(D3DTA_TEXTURE));
            (void) Graph->device->SetTextureStageState(0u, D3DTSS_COLORARG2, static_cast<unsigned int>(D3DTA_DIFFUSE));
            (void) Graph->device->SetTextureStageState(0u, D3DTSS_ALPHAARG2, static_cast<unsigned int>(D3DTA_DIFFUSE));
            (void) Graph->device->SetTextureStageState(0u, D3DTSS_COLOROP, static_cast<unsigned int>(D3DTOP_MODULATE));
            (void) Graph->device->SetTextureStageState(0u, D3DTSS_ALPHAOP, static_cast<unsigned int>(D3DTOP_MODULATE));
            (void) Graph->device->SetSamplerState(0u, D3DSAMP_MINFILTER, static_cast<unsigned int>(D3DTEXF_POINT));
            (void) Graph->device->SetSamplerState(0u, D3DSAMP_MAGFILTER, static_cast<unsigned int>(D3DTEXF_POINT));
            (void) Graph->device->SetRenderState(D3DRS_COLORWRITEENABLE, 7u);
            (void) Graph->device->SetRenderState(D3DRS_SPECULARENABLE, 0u);
            (void) Graph->device->SetRenderState(D3DRS_ALPHATESTENABLE, 0u);
            (void) Graph->device->SetRenderState(D3DRS_ALPHABLENDENABLE, 0u);
            (void) Graph->device->SetRenderState(D3DRS_ZFUNC, static_cast<unsigned int>(D3DCMP_ALWAYS));
            (void) Graph->device->SetRenderState(D3DRS_ZWRITEENABLE, 0u);
            (void) Graph->device->SetTexture(
                0u, reinterpret_cast<IDirect3DBaseTexture9 *>(compressedTarget->m_texture));
            Graph->DrawPrimitive(D3DPT_TRIANGLEFAN, 0x104u, v, 24u, 4);
            {
                TEXTURE* const releasedTexture = compressedTarget;
                delete releasedTexture;
            }
        }

        GRAPH *const projectedGraph = Graph;
        const float halfWidth = projectedGraph->screenWidth * 0.5f;
        const float projectedY = (cell->originY - 128.0f) + projectedGraph->screenHeight * 0.5f;
        const float projectedX = (cell->originX - 128.0f) + halfWidth;
        VECTOR2 projectedCell(projectedX, projectedY);
        Map->SetShiftCoor(projectedCell, 0x10000000);
        DrawLayerOnlyZ(VID::viewMin,
            VID::viewMax);
        {
            SPRITE_LIST *list = &cell->buildedSprites;
            if (list->m_count != 0)
            {
                int item = 0;
                SPRITE *sprite = list->m_data[0];
                while (sprite)
                {
                    if ((sprite->RuntimeFlags() & 0x04000000u) == 0u)
                        sprite->Draw();
                    ++item;
                    if (item >= list->m_count)
                        break;
                    sprite = list->m_data[item];
                }
            }
        }
        {
            SPRITE_LIST *list = &cell->alphaBuildedSprites;
            if (list->m_count != 0)
            {
                int item = 0;
                SPRITE *sprite = list->m_data[0];
                while (sprite)
                {
                    if ((sprite->RuntimeFlags() & 0x04000000u) == 0u)
                        sprite->Draw();
                    ++item;
                    if (item >= list->m_count)
                        break;
                    sprite = list->m_data[item];
                }
            }
        }

        IDirect3DDevice9 *const readbackDevice = Graph->device;
        IDirect3DSurface9 *const targetSurface = cell->sourceGroundTexture->GetSurface();
        IDirect3DSurface9 *const sourceGroundSurface = cell->renderTargetGround->GetSurface();
        const int readback =
            static_cast<int>(readbackDevice->GetRenderTargetData(sourceGroundSurface, targetSurface));
        if (readback < 0)
        {
            const STRING subject("GetRenderTargetData0 for DrawBuilded");
            if (::Error)
            {
                const STRING object("SPRITE_COLLECTOR");
                    ::Error->Error(object, E_COPY, subject,
                    static_cast<unsigned long>(readback));
            }
        }
        else
        {
            cell->buildedSprites.Release();
            cell->alphaBuildedSprites.Release();
        }

        groundCellBuildAttempts = 0u;
        Graph->SetScreenScale(savedScale);
        break;
    }

    const long restoreResult = Graph->SetRenderTarget(sourceSurface, savedViewMin, savedViewMax,
        oldDepthSurface);
    if (restoreResult < 0)
    {
        const STRING subject("rendet target for DrawBuilded");
        if (::Error)
        {
            const STRING object("SPRITE_COLLECTOR");
                ::Error->Error(object, E_SET, subject,
                static_cast<unsigned long>(static_cast<int>(restoreResult)));
        }
    }
    if (oldDepthSurface)
    {
        (void) oldDepthSurface->Release();
        oldDepthSurface = 0;
        GRAPH *const restoreGraph = Graph;
        const float halfWidth = restoreGraph->screenWidth * 0.5f;
        const float restoredY = savedMapPoint.y + restoreGraph->screenHeight * 0.5f;
        VECTOR2 projectedSaved(savedMapPoint.x + halfWidth, restoredY);
        Map->SetShiftCoor(projectedSaved, 0x10000000);
    }

    {
        COLOR clearColor;
        clearColor.value = 0xFF000000u;
        Graph->ClearScreen(clearColor);
    }
    VID::viewMin.x = minPoint.x;
    VID::viewMin.y = minPoint.y;
    VID::viewMax.x = maxPoint.x;
    VID::viewMax.y = maxPoint.y;
    DrawLayerOnlyZ(minPoint, maxPoint);

    (void) Graph->device->SetTextureStageState(0u, D3DTSS_COLORARG1, static_cast<unsigned int>(D3DTA_TEXTURE));
    (void) Graph->device->SetTextureStageState(0u, D3DTSS_ALPHAARG1, static_cast<unsigned int>(D3DTA_TEXTURE));
    (void) Graph->device->SetTextureStageState(0u, D3DTSS_COLORARG2, static_cast<unsigned int>(D3DTA_DIFFUSE));
    (void) Graph->device->SetTextureStageState(0u, D3DTSS_ALPHAARG2, static_cast<unsigned int>(D3DTA_DIFFUSE));
    (void) Graph->device->SetTextureStageState(0u, D3DTSS_COLOROP, static_cast<unsigned int>(D3DTOP_MODULATE));
    (void) Graph->device->SetTextureStageState(0u, D3DTSS_ALPHAOP, static_cast<unsigned int>(D3DTOP_MODULATE));
    (void) Graph->device->SetSamplerState(0u, D3DSAMP_MINFILTER, static_cast<unsigned int>(D3DTEXF_POINT));
    (void) Graph->device->SetSamplerState(0u, D3DSAMP_MAGFILTER, static_cast<unsigned int>(D3DTEXF_POINT));
    (void) Graph->device->SetRenderState(D3DRS_COLORWRITEENABLE, 7u);
    (void) Graph->device->SetRenderState(D3DRS_SPECULARENABLE, 0u);
    (void) Graph->device->SetRenderState(D3DRS_ALPHATESTENABLE, 0u);
    (void) Graph->device->SetRenderState(D3DRS_ALPHABLENDENABLE, 0u);
    (void) Graph->device->SetRenderState(D3DRS_ZFUNC, static_cast<unsigned int>(D3DCMP_ALWAYS));
    (void) Graph->device->SetRenderState(D3DRS_ZWRITEENABLE, 0u);
    drawnGroundCellCount = 0u;
    for (int index = 0; index < renderTargetGroundCount; ++index)
    {
        CELL *const cell = renderTargetGroundCells[index];
        if (!cell->sourceGroundTexture)
            continue;
        if (!((VECTOR2(cell->originX, cell->originY) + VECTOR2(128.0f, 128.0f)) > minPoint &&
            VECTOR2(cell->originX, cell->originY) < (maxPoint + VECTOR2(128.0f, 128.0f))))
            continue;
        const VECTOR2 worldMin(renderTargetGroundCells[index]->originX - 128.0f,
            renderTargetGroundCells[index]->originY - 128.0f);
        const VECTOR2 screenMin = Map->ToScreenScaled(worldMin);
        const VECTOR2 worldMax(renderTargetGroundCells[index]->originX + 128.0f,
            renderTargetGroundCells[index]->originY + 128.0f);
        const VECTOR2 screenMax = Map->ToScreenScaled(worldMax);
        TEXTURE *const texture = renderTargetGroundCells[index]->renderTargetGround;
        SpriteCollectorGroundVertex v[4];
        const float nearOne = 1.0f;
        const float uv0 = 0.0019529999699443579f;
        const float uv1 = 1.0019530f;
        v[0].x = screenMin.x;
        v[0].y = screenMin.y;
        v[0].z = 0.99999988f;
        v[0].rhw = nearOne;
        v[0].u = uv0;
        v[0].v = uv0;
        v[1].x = screenMax.x;
        v[1].y = screenMin.y;
        v[1].z = 0.99999988f;
        v[1].rhw = nearOne;
        v[1].u = uv1;
        v[1].v = uv0;
        v[2].x = screenMax.x;
        v[2].y = screenMax.y;
        v[2].z = 0.99999988f;
        v[2].rhw = nearOne;
        v[2].u = uv1;
        v[2].v = uv1;
        v[3].x = screenMin.x;
        v[3].y = screenMax.y;
        v[3].z = 0.99999988f;
        v[3].rhw = nearOne;
        v[3].u = uv0;
        v[3].v = uv1;
        (void) Graph->device->SetTexture(0u, reinterpret_cast<IDirect3DBaseTexture9 *>(texture->m_texture));
        Graph->DrawPrimitive(D3DPT_TRIANGLEFAN, 0x104u, v, 24u, 4);
        drawnGroundCellCount = drawnGroundCellCount + 1u;
    }

    (void) Graph->device->SetRenderState(D3DRS_SCISSORTESTENABLE, 1u);
    for (int index = 0; index < renderTargetGroundCount; ++index)
    {
        CELL *const cell = renderTargetGroundCells[index];
        if (!((VECTOR2(cell->originX, cell->originY) + VECTOR2(128.0f, 128.0f)) > minPoint &&
            VECTOR2(cell->originX, cell->originY) < (maxPoint + VECTOR2(128.0f, 128.0f))))
            continue;
        const float minCellX = renderTargetGroundCells[index]->originX - 128.0f;
        const float minCellY = renderTargetGroundCells[index]->originY - 128.0f;
        const VECTOR2 clipMin(minPoint.x > minCellX ? minPoint.x : minCellX,
            minCellY < minPoint.y ? minPoint.y : minCellY);
        VID::viewMin.x = clipMin.x;
        VID::viewMin.y = clipMin.y;
        const float maxCellX = renderTargetGroundCells[index]->originX + 128.0f;
        const float maxCellY = renderTargetGroundCells[index]->originY + 128.0f;
        const VECTOR2 clipMax(maxPoint.x < maxCellX ? maxPoint.x : maxCellX,
            maxCellY > maxPoint.y ? maxPoint.y : maxCellY);
        VID::viewMax.x = clipMax.x;
        VID::viewMax.y = clipMax.y;
        GRAPH *const scissorGraph = Graph;
        const float maxDeltaY = clipMax.y - minPoint.y;
        const float maxScreenY = maxDeltaY * scissorGraph->screenScaleY;
        const float maxScreenX = (clipMax.x - minPoint.x) * scissorGraph->screenScaleX;
        const float minDeltaY = clipMin.y - minPoint.y;
        const float minScreenY = minDeltaY * scissorGraph->screenScaleY;
        RECT scissorRect;
        scissorRect.left = TruncateFloatToInt32((clipMin.x - minPoint.x) * scissorGraph->screenScaleX);
        scissorRect.right = TruncateFloatToInt32(maxScreenX);
        scissorRect.top = TruncateFloatToInt32(minScreenY);
        scissorRect.bottom = TruncateFloatToInt32(maxScreenY);
        (void) scissorGraph->device->SetScissorRect(&scissorRect);
        {
            SPRITE_LIST *list = &renderTargetGroundCells[index]->buildedSprites;
            if (list->m_count != 0)
            {
                int item = 0;
                SPRITE *sprite = list->m_data[0];
                while (sprite)
                {
                    if ((sprite->RuntimeFlags() & 0x04000000u) == 0u)
                        sprite->Draw();
                    list = &renderTargetGroundCells[index]->buildedSprites;
                    ++item;
                    if (item >= list->m_count)
                        break;
                    sprite = list->m_data[item];
                }
            }
        }
        {
            SPRITE_LIST *list = &renderTargetGroundCells[index]->alphaBuildedSprites;
            if (list->m_count != 0)
            {
                int item = 0;
                SPRITE *sprite = list->m_data[0];
                while (sprite)
                {
                    if ((sprite->RuntimeFlags() & 0x04000000u) == 0u)
                        sprite->Draw();
                    list = &renderTargetGroundCells[index]->alphaBuildedSprites;
                    ++item;
                    if (item >= list->m_count)
                        break;
                    sprite = list->m_data[item];
                }
            }
        }
    }
    (void) Graph->device->SetRenderState(D3DRS_SCISSORTESTENABLE, 0u);

    if ((Graph->sceneState & 2u) == 0u && (Graph->capabilityFlags & 0x2000u) != 0u)
    {
        bool done = false;
        for (int y = viewMinY; y <= viewMaxY && !done; ++y)
        {
            for (int x = viewMinX; x <= viewMaxX && !done; ++x)
            {
                const float gridY = static_cast<float>(y) * 256.0f;
                const float top = gridY - minPoint.y;
                const float left = static_cast<float>(x) * 256.0f - minPoint.x;
                if (!(left >= savedViewMin.x) || !(top >= savedViewMin.y))
                    continue;
                const int nextX = BitsToSigned32(static_cast<unsigned int>(x) + 1u);
                const int nextY = BitsToSigned32(static_cast<unsigned int>(y) + 1u);
                const float nextGridY = static_cast<float>(nextY) * 256.0f;
                const float bottom = nextGridY - minPoint.y;
                const float right = static_cast<float>(nextX) * 256.0f - minPoint.x;
                if (!(right < savedViewMax.x) || !(bottom < savedViewMax.y))
                    continue;

                CELL &cell = spatialCells[(static_cast<unsigned int>(y)
                    << (static_cast<unsigned int>(gridShift) & 31u)) +
                static_cast<unsigned int>(x)];
                if (cell.buildedSprites.m_count == 0 && cell.alphaBuildedSprites.m_count == 0)
                    continue;
                const unsigned int previous = groundCellBuildAttempts;
                groundCellBuildAttempts = previous + 1u;
                if (previous <= 30u && cell.alphaBuildedSprites.m_count <= 100)
                    continue;

                RECT rect;
                const int offsetX = TruncateFloatToInt32(minPoint.x);
                rect.left =
                    BitsToSigned32((static_cast<unsigned int>(x) << 8) - static_cast<unsigned int>(offsetX));
                const int offsetY = TruncateFloatToInt32(minPoint.y);
                rect.top =
                    BitsToSigned32((static_cast<unsigned int>(y) << 8) - static_cast<unsigned int>(offsetY));
                rect.right = BitsToSigned32((static_cast<unsigned int>(nextX) << 8) -
                    static_cast<unsigned int>(offsetX));
                rect.bottom = BitsToSigned32((static_cast<unsigned int>(nextY) << 8) -
                    static_cast<unsigned int>(offsetY));

                if (!cell.sourceGroundTexture || cell.sourceGroundTexture->m_format == static_cast<unsigned int>(D3DFMT_DXT1))
                {
                    if (cell.sourceGroundTexture)
                    {
                        {
                            TEXTURE* const releasedTexture = cell.sourceGroundTexture;
                            delete releasedTexture;
                        }
                        cell.sourceGroundTexture = 0;
                    }
                    cell.sourceGroundTexture = new(std::nothrow) TEXTURE(256, 256, static_cast<D3DFORMAT>(Graph->builtPixelFormat), 4u);
                    if (cell.sourceGroundTexture->m_texture == 0 &&
                        cell.sourceGroundTexture->m_cpuData == 0)
                    {
                        if (cell.sourceGroundTexture)
                        {
                            {
                                TEXTURE* const releasedTexture = cell.sourceGroundTexture;
                                delete releasedTexture;
                            }
                            cell.sourceGroundTexture = 0;
                        }
                        {
                            const STRING subject(
                                "system memory texture for DrawBuilded");
                            if (::Error)
                            {
                                const STRING object(
                                    "SPRITE_COLLECTOR");
                                    ::Error->Error(object, E_CREATE, subject,
                                    0ul);
                            }
                        }
                    }
                }

                cell.CreateRenderTargetGround();
                if (!cell.renderTargetGround)
                {
                    const STRING subject("renderTargetGround texture for DrawBuilded");
                    if (::Error)
                    {
                        const STRING object("SPRITE_COLLECTOR");
                            ::Error->Error(object, E_CREATE, subject,
                            0ul);
                    }
                }

                IDirect3DDevice9 *const stretchDevice = Graph->device;
                IDirect3DSurface9 *const stretchTarget = cell.renderTargetGround->GetSurface();
                const int stretch = static_cast<int>(stretchDevice->StretchRect(
                    sourceSurface, &rect, stretchTarget, 0, D3DTEXF_NONE));
                if (stretch < 0)
                {
                    const STRING subject("StretchRect for DrawBuilded");
                    if (::Error)
                    {
                        const STRING object("SPRITE_COLLECTOR");
                            ::Error->Error(object, E_COPY, subject,
                            static_cast<unsigned long>(stretch));
                    }
                }
                IDirect3DDevice9 *const readbackDevice = Graph->device;
                IDirect3DSurface9 *const targetSurface = cell.sourceGroundTexture->GetSurface();
                IDirect3DSurface9 *const sourceGroundSurface = cell.renderTargetGround->GetSurface();
                const int readback =
                    static_cast<int>(readbackDevice->GetRenderTargetData(sourceGroundSurface, targetSurface));
                if (readback < 0)
                {
                    const STRING subject("GetRenderTargetData for DrawBuilded");
                    if (::Error)
                    {
                        const STRING object("SPRITE_COLLECTOR");
                            ::Error->Error(object, E_COPY, subject,
                            static_cast<unsigned long>(readback));
                    }
                }
                else
                {
                    cell.buildedSprites.Release();
                    cell.alphaBuildedSprites.Release();
                }
                groundCellBuildAttempts = 0u;
                done = true;
            }
        }
    }
}

SPRITE* SPRITE_COLLECTOR::FirstUnit()
{
    return FirstUnit(&reserved2EC, -1);
}

SPRITE* SPRITE_COLLECTOR::NextUnit()
{
    return NextUnit(&reserved2EC);
}

namespace {

int CompareDrawingSprites(const void* first, const void* second)
{
    const SPRITE* lhs = *static_cast<SPRITE* const*>(first);
    const SPRITE* rhs = *static_cast<SPRITE* const*>(second);
    const float right = Map->ToScreenY(rhs->Y(), rhs->Z());
    const float left = Map->ToScreenY(lhs->Y(), lhs->Z());
    return TruncateDoubleToInt32(static_cast<double>(right) - left);
}
}

void SPRITE_COLLECTOR::SortDrawingSprites()
{
    for (int layer = 0; layer < 13; ++layer)
        for (int cell = gridWidth * gridHeight; --cell >= 0; )
        {
            SPRITE_LIST& list = spatialCells[cell].layerSprites[layer];
            std::qsort(list.m_data, static_cast<std::size_t>(list.m_count),
                       sizeof(SPRITE*), CompareDrawingSprites);
        }
}

SPRITE* SPRITE_COLLECTOR::FirstUnit(int* index, int type) const
{
    const bool scanAll = (type == -1);
    int lane = scanAll?0:type;
    unsigned int handle = scanAll?0x80000000u:0u;
    SPRITE* result = 0;
    int item = 0;

    for (; ;)
    {
        const SPRITE_LIST& list = armySprites[lane];
        item = list.m_count-1;
        if (item >= 0 && list.m_data) result = list.m_data[item];
        if (result || !scanAll || lane >= 7) break;
        ++lane;
    }

    handle |= (static_cast<unsigned int>(lane)&0x1Fu)<<24;
    handle |= static_cast<unsigned int>(item)&0x00FFFFFFu;
    *reinterpret_cast<unsigned int*>(index) = handle;
    return result;
}

SPRITE* SPRITE_COLLECTOR::NextUnit(int* index) const
{
    const unsigned int oldHandle = *reinterpret_cast<const unsigned int*>(index);
    const bool scanAll = (oldHandle&0x80000000u) != 0u;
    int lane = static_cast<int>((oldHandle>>24)&0x1Fu);
    int item = static_cast<int>(oldHandle&0x00FFFFFFu);
    if (lane<0 || lane >= 8) lane = 0;

    SPRITE* result = 0;
    const SPRITE_LIST* list = &armySprites[lane];
    if (item>list->m_count) item = list->m_count;
    --item;
    if (item >= 0 && list->m_data) result = list->m_data[item];

    while (!result && scanAll && lane<7)
    {
        ++lane;
        list = &armySprites[lane];
        item = list->m_count-1;
        if (item >= 0 && list->m_data) result = list->m_data[item];
    }

    unsigned int handle = scanAll?0x80000000u:0u;
    handle |= (static_cast<unsigned int>(lane)&0x1Fu)<<24;
    handle |= static_cast<unsigned int>(item)&0x00FFFFFFu;
    *reinterpret_cast<unsigned int*>(index) = handle;
    return result;
}

SPRITE* SPRITE_COLLECTOR::GetSprite(int type, float x, float y, SPRITE* previous)
{
    if (type == 0) return 0;
    const VECTOR2 point(x, y);
    SPRITE* candidate = FindNearestSprite(type, point, 300.0f, previous);
    if (!candidate) return 0;
    VID* const owner = candidate->Vid();
    const float minX = candidate->X()-owner->halfSizeXY.x;
    const float minY = candidate->Y()-owner->halfSizeXY.y;
    const float maxX = candidate->X()+owner->halfSizeXY.x;
    const float maxY = candidate->Y()+owner->halfSizeXY.y;
    if (x<minX || y<minY || x>maxX || y>maxY) return 0;
    return candidate;
}

SPRITE* SPRITE_COLLECTOR::GetSpriteScr(int typeValue, const VECTOR2& screenPoint, SPRITE* previous)
{
    unsigned int type = static_cast<unsigned int>(typeValue);
    if (type == 0u) return 0;
    SPRITE* const excluded = previous;
    SPRITE* best = 0;

    unsigned int directionMask = type&0x000FF000u;
    if (directionMask == 0u) directionMask = 0x000FF000u;
    const unsigned int lowTypeId = type&0x00000FFFu;
    unsigned int categoryMask = 0u;
    VID* resolvedTypeOwner = 0;

    if (lowTypeId != 0u && (type&0x20000000u) == 0u)
    {
        resolvedTypeOwner = Map->Vid(static_cast<int>(lowTypeId));
        if (!SpriteCollectorTypeOwnerHasBuckets(resolvedTypeOwner)) return 0;
        if ((resolvedTypeOwner->property&0x40u) != 0u) type |= 0x10000000u;
        categoryMask = resolvedTypeOwner->spriteType;
    }
    else
    {
        categoryMask = (static_cast<unsigned int>(static_cast<int>(type)>>20))&0x000006FFu;
        if (categoryMask == 0u) categoryMask = 0x000006FFu;
    }

    if ((type&0x10000000u) != 0u)
    {
        MAP* const world = Map;
        const float maxY = screenPoint.y+300.0f;
        const float sampledHeight = world->GetGroundZ(screenPoint);
        SPRITE* candidate = this->FirstHashInBox(
            screenPoint.x-300.0f, screenPoint.y-300.0f,
            screenPoint.x+300.0f, maxY+sampledHeight);
        while (candidate)
        {
            SpriteCollectorConsiderScreenCandidate(candidate, type, categoryMask, directionMask, lowTypeId, screenPoint, excluded, best);
            candidate = this->NextHashInBox();
        }
        return best;
    }

    if ((categoryMask&0x18u) != 0u && (categoryMask&0x06E7u) == 0u)
    {
        for (int lane = 0; lane<8; ++lane)
        {
            if ((directionMask&(0x1000u<<lane)) == 0u) continue;
            int cursor = 0;
            SPRITE* candidate = this->FirstUnit(&cursor, lane);
            while (candidate)
            {
                if (SpriteCollectorScreenCandidateMatches(candidate, type, categoryMask, 0x000FF000u, lowTypeId, screenPoint, excluded) &&
                    SpriteCollectorScreenCandidateRanksBefore(candidate, best)) best = candidate;
                candidate = this->NextUnit(&cursor);
            }
        }
        return best;
    }

    if (categoryMask == 0x20u)
    {
        const MENU& menu = Map->MenuState();
        int index = menu.m_count-1;
        while (index >= 0)
        {
            SPRITE* candidate = menu.m_data[index];
            if (!candidate) break;
            SpriteCollectorConsiderScreenCandidate(candidate, type, 0x20u, directionMask, lowTypeId, screenPoint, excluded, best);
            const int count = menu.m_count;
            if (index>count) index = count;
            --index;
        }
        return best;
    }

    if ((categoryMask&0x80u) != 0u)
    {
        int firstType = 0;
        int lastType = 14;
        if (lowTypeId != 0u && (type&0x20000000u) == 0u)
        {
            firstType = resolvedTypeOwner->layer;
            lastType = firstType+1;
        }
        for (int typeLane = firstType; typeLane<lastType; ++typeLane)
        {
            int cursor = 0;
            SPRITE* candidate = this->FirstDrawingSprite(&cursor, typeLane);
            while (candidate)
            {
                VID* owner = candidate->Vid();
                if (owner->spriteClass == 23u)
                {
                    if (candidate->ChildBacklink() == 0 &&
                        SpriteCollectorScreenCandidateFilter(candidate, type, categoryMask, directionMask, lowTypeId, excluded) &&
                        SpriteCollectorType17ScreenGeometryMatches(candidate, owner, screenPoint) &&
                        SpriteCollectorScreenCandidateRanksBefore(candidate, best)) best = candidate;
                }
                else
                {
                    SpriteCollectorConsiderScreenCandidate(candidate, type, categoryMask, directionMask, lowTypeId, screenPoint, excluded, best);
                }
                candidate = this->NextDrawingSprite(&cursor, typeLane);
            }
        }
        return best;
    }

    if (lowTypeId != 0u && (type&0x20000000u) == 0u)
    {
        if ((static_cast<unsigned char>(resolvedTypeOwner->runtimeFlags)&0x80u) != 0u)
        {
            int cursor;
            SPRITE* candidate = FirstActiveSprite(&cursor);
            while (candidate)
            {
                SpriteCollectorConsiderScreenCandidate(candidate, type, categoryMask, directionMask, lowTypeId, screenPoint, excluded, best);
                candidate = NextActiveSprite(&cursor);
            }
            return best;
        }
    }

    SPRITE_COLLECTOR* const collector = this;
    const int gridColumns = collector->gridWidth, gridRows = collector->gridHeight;
    const float cellScaleX = collector->gridScaleX, cellScaleY = collector->gridScaleY;
    int minCellX = TruncateFloatToInt32((screenPoint.x-(2.0f/cellScaleX))*cellScaleX);
    int minCellY = TruncateFloatToInt32((screenPoint.y-(2.0f/cellScaleY))*cellScaleY);
    int maxCellX = TruncateFloatToInt32((screenPoint.x+(2.0f/cellScaleX))*cellScaleX);
    int maxCellY = TruncateFloatToInt32((screenPoint.y+(2.0f/cellScaleY))*cellScaleY);
    if (minCellX<0) minCellX = 0;
    else if (minCellX >= gridColumns) minCellX = gridColumns-1;
    if (minCellY<0) minCellY = 0;
    else if (minCellY >= gridRows) minCellY = gridRows-1;
    if (maxCellX<0) maxCellX = 0;
    else if (maxCellX >= gridColumns) maxCellX = gridColumns-1;
    if (maxCellY<0) maxCellY = 0;
    else if (maxCellY >= gridRows) maxCellY = gridRows-1;
    if (minCellX>maxCellX || minCellY>maxCellY) return best;
    for (int y = minCellY; y <= maxCellY; ++y)
    {
        for (int x = minCellX; x <= maxCellX; ++x)
        {
            const unsigned int cellIndex = (static_cast<unsigned int>(y)<<static_cast<unsigned int>(collector->gridShift))+static_cast<unsigned int>(x);
            for (int lane = 0; lane<15; ++lane)
            {
                SPRITE_LIST& bucket = collector->spatialCells[cellIndex].layerSprites[lane];
                for (int spriteIndex = bucket.m_count-1; spriteIndex >= 0; --spriteIndex)
                {
                    SPRITE* candidate = bucket.m_data[spriteIndex];
                    if (!candidate) break;
                    SpriteCollectorConsiderScreenCandidate(candidate, type, categoryMask, directionMask, lowTypeId, screenPoint, excluded, best);
                }
            }
        }
    }
    return best;
}

namespace
{
static int ClampSpatialIndex(double value, float scale, int limit)
{
    int index = TruncateDoubleToInt32(value*static_cast<double>(scale));
    if (index<0) return 0;
    if (index >= limit) return limit-1;
    return index;
}

static int SpatialCellIndex(const SPRITE_COLLECTOR& collector, double x, double y)
{
    const int cellX = ClampSpatialIndex(x, collector.gridScaleX, collector.gridWidth);
    const int cellY = ClampSpatialIndex(y, collector.gridScaleY, collector.gridHeight);
    return BitsToSigned32((static_cast<unsigned int>(cellY)<<(static_cast<unsigned int>(collector.gridShift)&31u))+static_cast<unsigned int>(cellX));
}

}

int SPRITE_COLLECTOR::DeleteFromActive(SPRITE* sprite)
{
    VID* objectOwner = sprite->Vid();
    if ((objectOwner->runtimeFlags&0x80u) == 0u)
        return 0u;

    for (int index = allSprites.m_count-1; index >= 0; --index)
    {
        if (allSprites.m_data[index] == sprite)
        {
            allSprites.m_data[index] = 0;
            return 0u;
        }
    }
    return 1u;
}

void SPRITE_COLLECTOR::ChangeCoor(SPRITE* sprite, const VECTOR& position)
{
    VID* vid = sprite->Vid();

    const VECTOR& oldPosition = sprite->Coor();

    if (vid->layer == 11)
    {
        if (oldPosition.z != position.z)
        {
            if (depthSortedSprites.DeleteSorted(sprite) == 0)
                depthSortedSprites.InsertSorted(sprite, position.z);
        }
    }
    else
    {
        const int oldIsoCell = SpatialCellIndex(*this, oldPosition.x, static_cast<float>(static_cast<double>(oldPosition.y)-oldPosition.z));
        const int newIsoCell = SpatialCellIndex(*this, position.x, static_cast<float>(static_cast<double>(position.y)-position.z));
        if (oldIsoCell != newIsoCell)
        {
            if (spatialCells[oldIsoCell].Delete(sprite) == 0)
                spatialCells[newIsoCell].Insert(sprite);
        }
    }

    if ((sprite->Vid()->property&0x40u) != 0u && (sprite->RuntimeFlags()&0x08000000u) == 0u)
    {
        const int oldXYCell = SpatialCellIndex(*this, oldPosition.x, oldPosition.y);
        const int newXYCell = SpatialCellIndex(*this, position.x, position.y);
        if (oldXYCell != newXYCell)
        {
            SPRITE_LIST& oldXY = spatialCells[oldXYCell].sprites;
            if (oldXY.Delete(sprite) == 0)
                spatialCells[newXYCell].sprites.Insert(sprite);
        }
    }
}

void SPRITE_COLLECTOR::ChangeArmy(SPRITE* sprite, int army)
{
    VID* const vid = sprite->Vid();
    if ((vid->spriteType&0x18u) == 0u) return;

    const int previousArmy = static_cast<int>((sprite->RuntimeFlags()>>17)&7u);
    armySprites[previousArmy].Delete(sprite);
    armySprites[army].Insert(sprite);
}

void SPRITE_COLLECTOR::DeleteFromNamed(SPRITE* sprite)
{
    const int count = namedSprites.m_count;
    if (count == 0) return;
    for (int index = count-1; index >= 0; --index)
    {
        if (namedSprites.m_data[index] != sprite) continue;
        const int last = count-1;
        namedSprites.m_count = last;
        namedSprites.m_data[index] = namedSprites.m_data[last];
        return;
    }
}

void SPRITE_COLLECTOR::InsertToNamed(SPRITE* sprite)
{
    EX_SPRITE_DATA* const extra = sprite->ExData();
    if (!extra) return;
    if (extra->nameText.value[0] == '\0') return;

    if (namedSprites.m_count >= namedSprites.m_capacity)
        namedSprites.Expand(AddSigned32Wrap(MultiplySigned32Wrap(namedSprites.m_capacity, 2), 4));
    namedSprites.m_data[namedSprites.m_count] = sprite;
    ++namedSprites.m_count;
}

SPRITE_COLLECTOR::SPRITE_COLLECTOR(const VECTOR2& dimensions)
{
    namedSprites.m_count = 0;
    namedSprites.m_capacity = 0;
    namedSprites.m_data = 0;
    reserved2EC = 0;
    scanItem = 0;
    gridScaleX = 0.00390625f;
    gridScaleY = 0.00390625f;
    renderTargetGroundCount = 0;
    deletingAll = 0;

    const float origin = (1.0f);
    const float scale = (0.00390625f);
    const float heightBias = (3.0f);
    const int rows = TruncateDoubleToInt32((static_cast<double>(dimensions.y)-origin)*scale+heightBias);
    gridHeight = rows;
    gridShift = 0;

    const double width = (static_cast<double>(dimensions.x)-origin)*scale+origin;
    int exponent = 0;
    if (width>origin)
    {
        do
        {
            ++exponent;
        }
        while (width>static_cast<double>(BitsToSigned32(1u<<(static_cast<unsigned int>(exponent)&31u))));
    }
    gridShift = exponent;
    const int columns = BitsToSigned32(1u<<(static_cast<unsigned int>(exponent)&31u));
    gridWidth = columns;

    const unsigned int total = static_cast<unsigned int>(gridWidth)*static_cast<unsigned int>(gridHeight);
    spatialCells = new(std::nothrow) CELL[total];

    const float coordScale = (256.0f);
    const float coordBias = (128.0f);
    for (int row = 0; row<gridHeight; ++row)
    {
        if (gridWidth <= 0) continue;
        const float y = static_cast<float>(static_cast<double>(row)*coordScale+coordBias);
        for (int col = 0; col<gridWidth; ++col)
        {
            const unsigned int index = (static_cast<unsigned int>(row)<<(static_cast<unsigned int>(gridShift)&31u))+static_cast<unsigned int>(col);
            CELL& cell = spatialCells[index];
            cell.originX = static_cast<float>(static_cast<double>(col)*coordScale+coordBias);
            cell.originY = y;
        }
    }
    for (unsigned int i = 0; i<130u; ++i) renderTargetGroundCells[i] = 0;
}

SPRITE_COLLECTOR::~SPRITE_COLLECTOR() noexcept(false)
{
    DeleteAll();
    delete[] spatialCells;
    spatialCells = 0;
    allSprites.Release();
    depthSortedSprites.Release();
    for (unsigned int i = 0; i<8u; ++i) armySprites[i].Release();

}

void SPRITE_COLLECTOR::DeleteAll()
{
    deletingAll = 1;
    const int cellCount = gridWidth*gridHeight;

    for (int cellIndex = cellCount-1; cellIndex >= 0; --cellIndex)
    {
        for (int lane = 0; lane<15; ++lane)
        {
            SPRITE_LIST* list = &spatialCells[cellIndex].layerSprites[lane];
            int index = list->m_count-1;
            SPRITE* sprite = list->m_count != 0 ? list->m_data[index] : 0;
            while (sprite)
            {
                if ((sprite->Vid()->spriteType&1u) != 0u)
                {
                    if (sprite->ActionStackHaveCommand(0, ACT_WHILE_NOT_QUEST))
                    {
                        const STRING subject("В стеке осталась невыполненной команда ACT_WHILE_NOT_QUEST");
                        sprite->Error(E_ERROR, subject, 0u);
                    }
                    else if (sprite->ActionStackHaveCommand(0, ACT_WHILE_NOT_SCRIPT_VAR))
                    {
                        const STRING subject("В стеке осталась невыполненной команда ACT_WHILE_NOT_SCRIPT_VAR");
                        sprite->Error(E_ERROR, subject, 0u);
                    }
                    else if (sprite->ActionStackHaveCommand(0, ACT_BUILD_UNIT))
                    {
                        const STRING subject("В стеке осталась невыполненной команда ACT_BUILD_UNIT");
                        sprite->Error(E_ERROR, subject, 0u);
                    }
                }

                delete sprite;
                list = &spatialCells[cellIndex].layerSprites[lane];
                if (index>list->m_count) index = list->m_count;
                --index;
                if (index<0) break;
                sprite = list->m_data[index];
            }
        }
    }

    deletingAll = 0;
    for (int cellIndex = gridWidth*gridHeight-1; cellIndex >= 0; --cellIndex)
    {
        for (int lane = 0; lane<15; ++lane)
        {
            SPRITE_LIST* list = &spatialCells[cellIndex].layerSprites[lane];
            if (list->m_count == 0 || !list->m_data[list->m_count-1]) continue;
            const STRING subject("Sprite exist after delete");
            list = &spatialCells[cellIndex].layerSprites[lane];
            SPRITE* const survivor = list->m_count != 0 ? list->m_data[list->m_count-1] : 0;
            survivor->Error(E_ERROR, subject, static_cast<unsigned long>(cellIndex));
        }
    }
}

void SPRITE_COLLECTOR::OptimizeForDraw()
{
    const int oldCount = allSprites.m_count;
    int holes = 0;
    for (int spriteIndex = 0; spriteIndex<oldCount; ++spriteIndex)
    {
        if (holes != 0)
            allSprites.m_data[spriteIndex-holes] = allSprites.m_data[spriteIndex];
        if (allSprites.m_data[spriteIndex] == 0)
            ++holes;
    }

    const int compactedCount = oldCount-holes;
    allSprites.m_count = compactedCount;
    if (compactedCount>allSprites.m_capacity)
        static_cast<LIST<SPRITE*>*>(&allSprites)->Expand(compactedCount);

}

static int SpriteSpatialClampAxis(double coordinate, float scale, unsigned int cellCount)
{
    int cell = TruncateDoubleToInt32(coordinate*static_cast<double>(scale));
    if (cell<0) return 0;
    if (static_cast<unsigned int>(cell) >= cellCount) return static_cast<int>(cellCount-1u);
    return cell;
}
static unsigned int SpriteSpatialCellIndex(const SPRITE_COLLECTOR* manager, int x, int y)
{
    return static_cast<unsigned int>(x)+(static_cast<unsigned int>(y)<<(static_cast<unsigned int>(manager->gridShift)&31u));
}

void SPRITE_COLLECTOR::Insert(SPRITE* sprite)
{
    EX_SPRITE_DATA* owner = sprite->ExData();
    if (owner != 0 && owner->nameText.value[0] != 0)
    {
        namedSprites.ExpandForInsert();
        namedSprites.m_data[namedSprites.m_count] = sprite;
        ++namedSprites.m_count;
    }

    const VECTOR& coor = sprite->Coor();
    const int cellX = SpriteSpatialClampAxis(coor.x, gridScaleX, static_cast<unsigned int>(gridWidth));
    const int cellY = SpriteSpatialClampAxis(static_cast<float>(static_cast<double>(coor.y)-coor.z), gridScaleY, static_cast<unsigned int>(gridHeight));
    const unsigned int cellIndex = SpriteSpatialCellIndex(this, cellX, cellY);
    const unsigned int type = static_cast<unsigned int>(sprite->Vid()->layer);

    if (type == 11u)
    {
        depthSortedSprites.InsertSorted(sprite);
    }
    else
        spatialCells[cellIndex].Insert(sprite);

    if ((sprite->Vid()->runtimeFlags&0x80u) != 0u)
    {
        allSprites.Insert(sprite);
        (void) sprite->Release();
    }

    if ((sprite->Vid()->property&0x40u) != 0u && (sprite->RuntimeFlags()&0x08000000u) == 0u)
    {
        const int drawX = SpriteSpatialClampAxis(coor.x, gridScaleX, static_cast<unsigned int>(gridWidth));
        const int drawY = SpriteSpatialClampAxis(coor.y, gridScaleY, static_cast<unsigned int>(gridHeight));
        const unsigned int drawIndex = SpriteSpatialCellIndex(this, drawX, drawY);
        spatialCells[drawIndex].sprites.Insert(sprite);
    }

    if ((sprite->Vid()->spriteType&0x18u) != 0u)
    {
        const unsigned int category = (sprite->RuntimeFlags()>>17u)&7u;
        armySprites[category].Insert(sprite);
    }
}

void SPRITE_COLLECTOR::InsertToHash(SPRITE* sprite)
{
    VID* definition = sprite->Vid();
    if ((definition->property&0x40u) == 0u || (sprite->RuntimeFlags()&0x08000000u) != 0u)
        return;

    const VECTOR& coor = sprite->Coor();
    int x = TruncateDoubleToInt32(static_cast<double>(coor.x)*gridScaleX);
    if (x<0) x = 0;
    else if (x >= gridWidth) x = gridWidth-1;

    int y = TruncateDoubleToInt32(static_cast<double>(coor.y)*gridScaleY);
    if (y<0) y = 0;
    else if (y >= gridHeight) y = gridHeight-1;

    const unsigned int cellIndex = (static_cast<unsigned int>(y)<<static_cast<unsigned int>(gridShift))+static_cast<unsigned int>(x);
    spatialCells[cellIndex].sprites.Insert(sprite);
}

int SPRITE_COLLECTOR::DeleteFromHash(SPRITE* sprite)
{
    if (!spatialCells) return 0;
    VID* const definition = sprite->Vid();
    if ((definition->property&0x40u) == 0u || (sprite->RuntimeFlags()&0x08000000u) != 0u)
        return 0;

    const VECTOR& coor = sprite->Coor();
    int x = TruncateDoubleToInt32(static_cast<double>(coor.x)*gridScaleX);
    if (x<0) x = 0;
    else if (x >= gridWidth) x = gridWidth-1;

    int y = TruncateDoubleToInt32(static_cast<double>(coor.y)*gridScaleY);
    if (y<0) y = 0;
    else if (y >= gridHeight) y = gridHeight-1;

    const unsigned int cellIndex = (static_cast<unsigned int>(y)<<static_cast<unsigned int>(gridShift))+static_cast<unsigned int>(x);
    CELL& cell = spatialCells[cellIndex];
    if (x == scanCurrentX && y == scanCurrentY)
    {
        int cachedCount = scanItem;
        const int cellCount = cell.sprites.m_count;
        if (cachedCount>0 && cachedCount<cellCount &&
            cell.sprites.m_data[cachedCount-1] == sprite)
            scanItem = cachedCount-1;
    }

    return cell.sprites.Delete(sprite);
}

int SPRITE_COLLECTOR::Delete(SPRITE* sprite)
{
    const VECTOR& coor = sprite->Coor();

    const int drawX = SpriteSpatialClampAxis(coor.x, gridScaleX, static_cast<unsigned int>(gridWidth));
    const int drawY = SpriteSpatialClampAxis(coor.y, gridScaleY, static_cast<unsigned int>(gridHeight));

    unsigned int status = 0u;
    EX_SPRITE_DATA* owner = sprite->ExData();
    if (owner != 0 && owner->nameText.value[0] != 0)
    {
        LIST<SPRITE*>* tracked = &namedSprites;
        SPRITE* key = sprite;
        const int index = tracked->Location(key);
        status = static_cast<unsigned int>(tracked->DeleteNumber(index))<<5u;
    }

    const int cellY = SpriteSpatialClampAxis(static_cast<float>(static_cast<double>(coor.y)-coor.z), gridScaleY, static_cast<unsigned int>(gridHeight));
    const unsigned int cellIndex = SpriteSpatialCellIndex(this, drawX, cellY);
    VID* vid = sprite->Vid();
    const unsigned int type = static_cast<unsigned int>(vid->layer);
    if (type == 11u)
        status |= static_cast<unsigned int>(depthSortedSprites.DeleteSorted(sprite))<<2u;
    else
        status |= static_cast<unsigned int>(spatialCells[cellIndex].Delete(sprite))<<4u;

    vid = sprite->Vid();
    if ((vid->runtimeFlags&0x80u) != 0u)
    {
        bool found = false;
        for (int i = allSprites.m_count-1; i >= 0; --i)
        {
            if (allSprites.m_data[i] == sprite)
            {
                allSprites.m_data[i] = 0;
                found = true;
                break;
            }
        }
        if (!found) status |= 0x08u;
    }

    vid = sprite->Vid();
    if (spatialCells != 0 && (vid->property&0x40u) != 0u && (sprite->RuntimeFlags()&0x08000000u) == 0u)
    {
        const unsigned int drawIndex = SpriteSpatialCellIndex(this, drawX, drawY);
        CELL& drawCell = spatialCells[drawIndex];

        if (drawX == scanCurrentX && drawY == scanCurrentY)
        {
            int cursor = scanItem;
            if (cursor>0 && cursor<drawCell.sprites.m_count &&
                drawCell.sprites.m_data[cursor-1] == sprite)
                scanItem = cursor-1;
        }
        status |= drawCell.sprites.Delete(sprite);
    }

    vid = sprite->Vid();
    if ((vid->spriteType&0x18u) != 0u)
    {
        const unsigned int category = (sprite->RuntimeFlags()>>17u)&7u;
        status |= static_cast<unsigned int>(armySprites[category].Delete(sprite))<<1u;
    }

    if (status != 0u)
    {
        void* mapCollision = Mouse;
        const bool exempt = (sprite == mapCollision) ||
        (mapCollision != 0 && sprite == static_cast<SPRITE*>(mapCollision)->ChildChain()) ||
        sprite->Animation() >= 15;
        if (!exempt)
        {
            STRING subject;
            STRING objectFormat;
            subject = "hash can't delete";
            vid = sprite->Vid();
            const int vidId = vid?vid->nVid:-1;
            objectFormat = "SPRITE [%i](%i,%i,%i)";
            MYERROR* sink = ::Error;
            sink->Error(objectFormat, E_ERROR, subject, static_cast<int>(status), vidId, TruncateFloatToInt32(coor.x), TruncateFloatToInt32(coor.y), TruncateFloatToInt32(coor.z));
        }
    }
    return status;
}
