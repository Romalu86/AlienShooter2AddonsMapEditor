#define INPUT WIN32_INPUT
#include <windows.h>
#include "engine_globals.h"
#include <d3d9.h>
#undef INPUT
#include "myerror.h"
#include "mylib.h"
#include "vid.h"

unsigned int VID_SURFACE::noObj = 0;
unsigned int VID_SURFACE::noVertex = 0;
unsigned int VID_SURFACE::noTri = 0;
unsigned int VID_SURFACE::noDrawReal[16] = {};
unsigned int VID_SURFACE::noDrawCall[16] = {};
unsigned int VID_SURFACE::noDrawShadow = 0;
#include "list.h"
#include "texture.h"
#include "texture_atlas.h"
#include "registry.h"
#include "basetype.h"
#include "region.h"
#include "sprite.h"
#include "mouse.h"
#include "map.h"
#include "resread.h"
#include "sprite_collector.h"
#include "graph.h"
#include "constant.h"
#include <new>
#include <cstdlib>
#include <intrin.h>
#include <cstddef>
#include <cstdio>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

IDirect3DVertexBuffer9* MESH::unitQuad = 0;
unsigned int MESH::noVertex = 0;
unsigned int MESH::noTri = 0;

MESH::MESH()
    : references(1), vertexBuffer(0), indexBuffer(0), primitiveType(D3DPT_TRIANGLELIST),
      vertexFormat(D3DFVF_XYZ), vertexStride(12)
{
}

MESH::~MESH()
{
    ReleaseSharedBuffers();
    ReleaseVideoMemory();
}

void MESH::AddRef() { ++references; }
void MESH::Release() { if (--references == 0) delete this; }

void MESH::ReleaseSharedBuffers()
{
    if (unitQuad)
    {
        unitQuad->Release();
        unitQuad = 0;
    }
}

void MESH::ReleaseVideoMemory()
{
    if (vertexBuffer)
    {
        vertexBuffer->Release();
        vertexBuffer = 0;
    }
    if (indexBuffer)
    {
        indexBuffer->Release();
        indexBuffer = 0;
    }
}

static void MeshError(TYPE_ERROR type, const char* text, HRESULT result)
{
    Error->Error(STRING("MESH"), type, STRING(text), result);
}

int MESH::CreateIndexBuffer(unsigned int count)
{
    const D3DPOOL pool = static_cast<D3DPOOL>(((Graph->capabilityFlags >> 4)&1u)*2u);
    const HRESULT result = Graph->device->CreateIndexBuffer(count*2u, D3DUSAGE_WRITEONLY,
        D3DFMT_INDEX16, pool, &indexBuffer, 0);
    if (result < 0)
    {
        MeshError(E_CREATE, "index buffer", result);
        return -1;
    }
    return 0;
}

int MESH::CreateVertexBuffer(unsigned int count, unsigned int format)
{
    vertexFormat = format;
    vertexStride = (format&D3DFVF_XYZRHW) ? 16u : ((format&D3DFVF_XYZ) ? 12u : 0u);
    if ((format&D3DFVF_TEX1) != 0u) vertexStride += 8u;
    if ((format&D3DFVF_DIFFUSE) != 0u) vertexStride += 4u;
    if ((format&D3DFVF_SPECULAR) != 0u) vertexStride += 4u;
    const D3DPOOL pool = static_cast<D3DPOOL>(((Graph->capabilityFlags >> 4)&1u)*2u);
    unsigned int retries = 0;
    HRESULT result;
    for (;;)
    {
        result = Graph->device->CreateVertexBuffer(count*vertexStride, D3DUSAGE_WRITEONLY,
            vertexFormat, pool, &vertexBuffer, 0);
        if (result != D3DERR_OUTOFVIDEOMEMORY || !VID::lastUse || ++retries >= 5u) break;
        VID::lastUse->lruPrevious->ReleaseVideoMemory();
    }
    if (result < 0)
    {
        MeshError(E_CREATE, "vertex buffer in default memory", result);
        result = Graph->device->CreateVertexBuffer(count*vertexStride, D3DUSAGE_WRITEONLY,
            vertexFormat, D3DPOOL_MANAGED, &vertexBuffer, 0);
        if (result < 0)
        {
            MeshError(E_CREATE, "Vertex buffer", result);
            return -1;
        }
    }
    return 0;
}

void MESH::CreateFromVid(VID_SURFACE* surface)
{
    if (!surface->frames) return;
    ReleaseVideoMemory();
    const CADR_DATA& last = surface->frames[surface->noCadr-1];
    const unsigned int vertexCount = last.baseVertex+last.vertexCount;
    const unsigned int indexCount = last.firstIndex+last.indexCount;
    if (vertexCount == 0u && indexCount == 0u) return;
    primitiveType = D3DPT_TRIANGLELIST;
    if (CreateVertexBuffer(vertexCount, D3DFVF_XYZ) < 0 || !vertexBuffer) return;
    VECTOR* vertices = 0;
    HRESULT result = vertexBuffer->Lock(0, vertexCount*vertexStride,
        reinterpret_cast<void**>(&vertices), 0);
    if (result < 0)
    {
        MeshError(E_LOCK, "for CreateFromVid vertex buffer", result);
        return;
    }
    unsigned short* indices = 0;
    if (indexCount != 0u)
    {
        if (CreateIndexBuffer(indexCount) < 0 || !indexBuffer)
        {
            vertexBuffer->Unlock();
            return;
        }
        result = indexBuffer->Lock(0, indexCount*2u, reinterpret_cast<void**>(&indices), 0);
        if (result < 0)
        {
            MeshError(E_LOCK, "for CreateFromVid index buffer", result);
            vertexBuffer->Unlock();
            return;
        }
    }
    const float halfX = static_cast<float>(-static_cast<int>(surface->vidSizeX)/2);
    const float halfY = static_cast<float>(-static_cast<int>(surface->vidSizeY)/2);
    for (int frameIndex = 0; frameIndex < surface->noCadr; ++frameIndex)
    {
        const CADR_DATA& frame = surface->frames[frameIndex];
        const unsigned char* const data = surface->surfaceData+frame.frameDataOffset;
        if (indices)
            memcpy(indices+frame.firstIndex, data, static_cast<unsigned int>(frame.indexCount)*2u);
        const unsigned char* const packed = data+static_cast<unsigned int>(frame.indexCount)*2u;
        for (unsigned int index = 0; index < frame.vertexCount; ++index)
        {
            unsigned int value;
            memcpy(&value, packed+index*4u, sizeof(value));
            VECTOR& vertex = vertices[frame.baseVertex+index];
            vertex.x = static_cast<float>(value&0x3FFu)+halfX;
            vertex.z = static_cast<float>((value >> 20)&0x3FFu)/
                static_cast<float>(1u << (value >> 30))-32.0f;
            vertex.y = (static_cast<float>((value >> 10)&0x3FFu)+halfY+vertex.z)/0.707060218f;
        }
    }
    if (indices) indexBuffer->Unlock();
    vertexBuffer->Unlock();
}

void MESH::CreateUnitQuad()
{
    primitiveType = D3DPT_TRIANGLESTRIP;
    vertexFormat = D3DFVF_XYZ;
    vertexStride = 12;
    if (!unitQuad)
    {
        if (CreateVertexBuffer(4, vertexFormat) < 0) return;
        void* destination = 0;
        const HRESULT result = vertexBuffer->Lock(0, 48, &destination, 0);
        if (result < 0)
        {
            MeshError(E_LOCK, "vertex buffer", result);
            return;
        }
        const VECTOR vertices[4] = {VECTOR(0,0,0), VECTOR(1,0,0), VECTOR(0,1,0), VECTOR(1,1,0)};
        memcpy(destination, vertices, sizeof(vertices));
        vertexBuffer->Unlock();
        unitQuad = vertexBuffer;
        unitQuad->AddRef();
    }
    else
    {
        vertexBuffer = unitQuad;
        vertexBuffer->AddRef();
    }
}

int MESH::CreateSphere(int stacks, int slices)
{
    ID3DXMesh* sphere = 0;
    D3DXCreateSphere(Graph->device, 1.0f, slices, stacks, &sphere, 0);
    unsigned char* sourceVertices;
    unsigned short* sourceIndices;
    sphere->LockVertexBuffer(D3DLOCK_READONLY, reinterpret_cast<void**>(&sourceVertices));
    sphere->LockIndexBuffer(D3DLOCK_READONLY, reinterpret_cast<void**>(&sourceIndices));
    const unsigned int sourceStride = sphere->GetNumBytesPerVertex();
    const unsigned int vertexCount = sphere->GetNumVertices();
    const unsigned int indexCount = sphere->GetNumFaces()*3u;
    VECTOR* positions = new VECTOR[vertexCount+1u];
    for (unsigned int i = 0; i<vertexCount; ++i)
        memcpy(positions+i, sourceVertices+i*sourceStride, sizeof(VECTOR));
    positions[vertexCount] = VECTOR(0.0f, 0.0f, 0.0f);
    ReleaseVideoMemory();
    primitiveType = D3DPT_TRIANGLELIST;
    if (CreateVertexBuffer(vertexCount+1u, D3DFVF_XYZ) == 0)
    {
        void* target;
        HRESULT result = vertexBuffer->Lock(0, (vertexCount+1u)*vertexStride, &target, 0);
        if (result < 0) MeshError(E_LOCK, "for CreateFromVid vertex buffer", result);
        else
        {
            memcpy(target, positions, (vertexCount+1u)*vertexStride);
            vertexBuffer->Unlock();
            if (CreateIndexBuffer(indexCount) == 0)
            {
                result = indexBuffer->Lock(0, indexCount*2u, &target, 0);
                if (result < 0) MeshError(E_LOCK, "for CreateFromVid index buffer", result);
                else
                {
                    memcpy(target, sourceIndices, indexCount*2u);
                    indexBuffer->Unlock();
                }
            }
        }
    }
    sphere->UnlockIndexBuffer();
    sphere->UnlockVertexBuffer();
    sphere->Release();
    delete[] positions;
    return 0;
}

void MESH::Draw(unsigned int firstVertex, unsigned int vertexCount,
                unsigned int firstIndex, unsigned int indexCount)
{
    if (!vertexBuffer)
    {
        MeshError(E_INVALID, "dxVertex Draw()", 0);
        return;
    }
    unsigned int passes;
    Graph->pseudo3DEffect->Begin(&passes, 0);
    for (unsigned int pass = 0; pass < passes; ++pass)
    {
        Graph->pseudo3DEffect->Pass(pass);
        if (indexBuffer)
        {
            const HRESULT result = Graph->device->SetIndices(indexBuffer);
            if (result < 0) MeshError(E_SET, "index", result);
        }
        const HRESULT result = Graph->device->SetStreamSource(0, vertexBuffer, 0, vertexStride);
        if (result < 0) MeshError(E_SET, "Vertex", result);
        Graph->device->SetFVF(vertexFormat);
        if (primitiveType == D3DPT_TRIANGLELIST)
        {
            Graph->device->DrawIndexedPrimitive(primitiveType, firstVertex, 0,
                vertexCount, firstIndex, indexCount/3u);
            noTri += indexCount/3u;
        }
        else
        {
            Graph->device->DrawPrimitive(primitiveType, firstVertex, vertexCount-2u);
            noTri += vertexCount-2u;
        }
        noVertex += vertexCount;
    }
    Graph->pseudo3DEffect->End();
}

void VID::DrawMesh(SPRITE*) {}
void VID::DrawOpaque(SPRITE*) {}
void VID::DrawOccluded(SPRITE*) {}

namespace
{
enum VidHardwareRuntimeFlags
{
    kVidRuntimeHidden       = 0x00000800u,
    kVidAnimateScale        = 0x00000002u,
    kVidAnimateOffset       = 0x00000004u
};
enum VidOwnerDrawFlags
{
    kVidApplyPixelCenterOffset = 0x00008000u,
    kVidApplyDirectionOffset   = 0x00010000u
};
}

static int ScaleHpRatioFpu(int currentHp, int newMaxHp, int oldMaxHp)
{

    const double product = static_cast<double>(currentHp)*static_cast<double>(newMaxHp);
    return TruncateDoubleToInt32(product/static_cast<double>(oldMaxHp));
}

static int TruncateFixedPointTowardZero(int value, unsigned int fractionalBits)
{
    if (value >= 0) return value>>fractionalBits;
    return -static_cast<int>((0u-static_cast<unsigned int>(value))>>fractionalBits);
}

static int ScaleHpRatioFixedShift4(int currentHp, int newMaxHp, int oldMaxHp)
{
    const unsigned int productBits = static_cast<unsigned int>(currentHp)*static_cast<unsigned int>(newMaxHp);
    const int quotient = static_cast<int>(productBits)/oldMaxHp;
    return TruncateFixedPointTowardZero(quotient, 4u);
}

static int ScaleHpRatioFixedShift8(int currentHp, int newMaxHp, int oldMaxHp)
{
    unsigned int productBits = static_cast<unsigned int>(currentHp)*static_cast<unsigned int>(newMaxHp);
    productBits <<= 8;
    const int quotient = static_cast<int>(productBits)/oldMaxHp;
    return TruncateFixedPointTowardZero(quotient, 8u);
}

static SPRITE* FirstAllSpriteForVid()
{
    SPRITE_COLLECTOR* collector = ::Sprites;
    return static_cast<SPRITE*>(collector->FirstAllSprite());
}

static SPRITE* NextAllSpriteForVid()
{
    SPRITE_COLLECTOR* collector = ::Sprites;
    return static_cast<SPRITE*>(collector->NextAllSprite());
}

void VID::SetHpCoeff(unsigned int direction, int hpCoeff)
{
    const unsigned int dir = static_cast<unsigned int>(direction)&7u;
    VID* vid = this;
    while (vid)
    {
        int oldMaxHp = vid->maxHpByPlayer[dir];
        if (oldMaxHp <= 1) oldMaxHp = 1;

        if (hpCoeff >= 0)
        {

            const int baseHp = vid->maxHp;
            int newMaxHp = static_cast<int>(static_cast<unsigned int>(baseHp)*static_cast<unsigned int>(hpCoeff));
            newMaxHp /= 100;
            if (newMaxHp <= 1) newMaxHp = 1;
            vid->maxHpByPlayer[dir] = newMaxHp;
        }

        if (vid->maxHp != 0)
        {
            SPRITE* sprite = FirstAllSpriteForVid();
            while (sprite)
            {
                if (sprite->Vid() == vid && ((sprite->RuntimeFlags()>>17)&7u) == dir)
                {
                    const int newMaxHp = vid->maxHpByPlayer[dir];
                    const int currentHp = sprite->m_hp;
                    int scaledHp;
                    if (newMaxHp>5000)
                        scaledHp = ScaleHpRatioFpu(currentHp, newMaxHp, oldMaxHp);
                    else
                        scaledHp = ScaleHpRatioFixedShift4(currentHp, newMaxHp, oldMaxHp);
                    if (scaledHp <= 1) scaledHp = 1;
                    sprite->ChangeHp(scaledHp);
                }
                sprite = NextAllSpriteForVid();
            }
        }

        vid = vid->linkVid;
    }
}

void VID::SetMaxHp(int hp)
{
    if (hp < 0)
        return;
    int oldMaxHp[PlayerSlotCount];
    for (unsigned int player = 0;player<PlayerSlotCount;++player)
    {
        oldMaxHp[player] = maxHpByPlayer[player]>1 ? maxHpByPlayer[player] : 1;
        maxHpByPlayer[player] = hp;
    }
    maxHp = hp;
    if (hp == 0)
        return;
    for (SPRITE* sprite = FirstAllSpriteForVid();sprite;sprite = NextAllSpriteForVid())
    {
        if (sprite->Vid() == this)
        {
            const unsigned int player = (sprite->RuntimeFlags()>>17u)&7u;
            int scaledHp = hp>5000
                ? ScaleHpRatioFpu(sprite->m_hp, hp, oldMaxHp[player])
                : ScaleHpRatioFixedShift8(sprite->m_hp, hp, oldMaxHp[player]);
            if (scaledHp < 2)
                scaledHp = 1;
            sprite->ChangeHp(scaledHp);
        }
    }
}

void VID::SetMaxHp(unsigned int direction, int hp)
{
    const unsigned int dir = static_cast<unsigned int>(direction)&7u;
    VID* vid = this;
    while (vid)
    {
        int oldMaxHp = vid->maxHpByPlayer[dir];
        if (oldMaxHp <= 1) oldMaxHp = 1;
        if (hp >= 0) vid->maxHpByPlayer[dir] = hp;

        if (vid->maxHp != 0)
        {
            SPRITE* sprite = FirstAllSpriteForVid();
            while (sprite)
            {
                if (sprite->Vid() == vid && ((sprite->RuntimeFlags()>>17)&7u) == dir)
                {
                    const int newMaxHp = vid->maxHpByPlayer[dir];
                    const int currentHp = sprite->m_hp;
                    const int scaledHp = (newMaxHp>5000)
                        ? ScaleHpRatioFpu(currentHp, newMaxHp, oldMaxHp)
                        : ScaleHpRatioFixedShift8(currentHp, newMaxHp, oldMaxHp);
                    sprite->ChangeHp(scaledHp);
                }
                sprite = NextAllSpriteForVid();
            }
        }

        vid = vid->linkVid;
    }
}

static bool DirectionQuantizationOverrideEnabled()
{

    return Map->IsNetGameStart() != 0;
}

int VID::RealDirectionWithGraphDetail(unsigned char direction)
{
    if ((property & 0x20000000u) != 0u)
        direction = static_cast<unsigned char>(direction + Graph->CameraDirection().value);
    const unsigned int wrapped = (static_cast<unsigned int>(directionQuantizationOffset)+static_cast<unsigned int>(direction))&0xFFu;
    const int directionCount = noDir;

    if ((runtimeFlags&0x00000200u) != 0u)
    {
        GRAPH* const camera = Graph;
        if (static_cast<int>(camera->graphDetail)<5 || DirectionQuantizationOverrideEnabled())
        {
            if (directionCount == 32)
                return static_cast<int>(((wrapped<<5)>>8)&0xFFFCu);
            return static_cast<int>(((wrapped*static_cast<unsigned int>(directionCount))>>8)&0xFFFEu);
        }
    }

    return static_cast<int>((wrapped*static_cast<unsigned int>(directionCount))>>8);
}

void VID_SURFACE::ReleaseVideoMemory()
{
    if (mesh) mesh->ReleaseVideoMemory();
    if (textureAtlas) textureAtlas->ReleaseVideoMemory();
    textureAtlas = 0;
    if (!frames || (surfaceFlags&1u) != 0u) return;
    for (int i = 0;i<static_cast<int>(noCadr);++i)
        frames[i].ReleaseVideoMemory();
}

void VID_SURFACE::SetFramesToDevice()
{
    if (frames)
    {
        if (!mesh) mesh = new MESH();
        if (!mesh->VertexBuffer())
        {
            if (frames[0].indexCount != 0u && surfaceMode == 3)
                mesh->CreateFromVid(this);
            else
                mesh->CreateUnitQuad();
        }
    }
    for (int frame = 0; frame<noCadr; ++frame)
        frames[frame].SetTextureToDevice(this);
}

CADR_DATA::CADR_DATA()
{
    lruNext = 0u;
    lruPrevious = 0u;
    indexCount = 0u;
    extraDataCount = 0u;
    vertexCount = 0u;
    width = 0u;
    height = 0u;
    offsetX = 0u;
    offsetY = 0u;
    textureObject = 0u;
    indexBuffer = 0u;
    vertexBuffer = 0u;
    minZ = -999999.0f;
    maxZ = 10.0f;
    lastUseTick = 0u;
    firstIndex = 0u;
    baseVertex = 0u;
}

CADR_DATA::~CADR_DATA() noexcept(false)
{
    if (textureObject)
    {
        TEXTURE* const object = textureObject;
        delete object;
        textureObject = 0u;
    }
    if (indexBuffer)
    {
        IUnknown* const object = indexBuffer;
        (void)object->Release();
        indexBuffer = 0u;
    }
    if (vertexBuffer)
    {
        IUnknown* const object = vertexBuffer;
        (void)object->Release();
        vertexBuffer = 0u;
    }
}

CADR_DATA& CADR_DATA::operator=(const CADR_DATA& source)
{
    if (this != &source)
    {
        lruNext = 0;
        lruPrevious = 0;
        lastUseTick = source.lastUseTick;
        indexCount = source.indexCount;
        extraDataCount = source.extraDataCount;
        vertexCount = source.vertexCount;
        width = source.width;
        height = source.height;
        offsetX = source.offsetX;
        offsetY = source.offsetY;
        frameDataOffset = source.frameDataOffset;
        std::memcpy(&maxZ, &source.maxZ, sizeof(maxZ));
        textureObject = source.textureObject;
        vertexBuffer = source.vertexBuffer;
        indexBuffer = source.indexBuffer;
        firstIndex = source.firstIndex;
        baseVertex = source.baseVertex;
        atlasOffsetX = source.atlasOffsetX;
        atlasOffsetY = source.atlasOffsetY;
    }
    return *this;
}

void CADR_DATA::CalcMaxMinZ(const VID_SURFACE* surface)
{
    if (!FloatEqualOrUnordered(minZ, -999999.0f)) return;
    if (vertexCount == 0u)
    {
        minZ = 0.0012f;
        return;
    }

    const unsigned int frameBlockOffset = frameDataOffset;
    const unsigned int indexWordCount = indexCount;
    const unsigned char* const data = surface->surfaceData;
    maxZ = 0.0f;
    minZ = 999.0f;
    const unsigned char* const vertexData = data+indexWordCount*2u+frameBlockOffset;
    const float bias = 32.0f;
    for (int index = 0; index < vertexCount; ++index)
    {
        const unsigned int packedVertex = *reinterpret_cast<const unsigned int*>(vertexData+index*4u);
        const unsigned int mantissa = (packedVertex>>20)&0x3FFu;
        const unsigned int divisor = 1u<<(packedVertex>>30);
        const float value = static_cast<float>(mantissa)/static_cast<float>(divisor)-bias;
        if (value>maxZ) maxZ = value;
        if (!(value >= minZ)) minZ = value;
    }
}

float VID::CalculateZSpeed(float numerator, float denominator) const
{
    const float scale = maxSpeed;
    float result;

    if (scale == 999999.0f)
        result = 0.0f;
    else if ((property&2u) != 0u || (property&4u) != 0u)
    {
        const float graphScale = (property&2u) != 0u ? Const->Gravitation : Const->Gravitation2;
        result = (denominator*graphScale/scale*0.5f)+(numerator*scale/denominator);
        result *= result>0.0f ? 1.1f : 0.9f;
    }
    else if ((property&0x08000000u) != 0u)
        result = maxZSpeed;
    else if (!FloatEqualOrUnordered(topZ, 0.0f) && spriteClass != 5u)
        result = 0.0f;
    else if (acceleration == 999999.0f)
        result = numerator*scale/denominator;
    else
        result = (numerator*scale)/((scale*scale*0.5f/acceleration)+denominator);

    if (result>maxZSpeed) return maxZSpeed;
    if (!(result >= -maxZSpeed)) result = -maxZSpeed;
    return result;
}

void CADR_DATA::ReleaseVideoMemory()
{
    if (textureObject)
    {
        TEXTURE* const texture = textureObject;
        const TEXTURE& info = *texture;
        const unsigned int pixelCount = info.m_width*info.m_height;
        unsigned int bytes = 0u;
        if (info.m_format == D3DFMT_DXT1)
            bytes = static_cast<unsigned int>(BitsToSigned32(pixelCount)/2);
        else
        {
            unsigned int bytesPerPixel = 2u;
            if (info.m_format == D3DFMT_P8 || info.m_format == D3DFMT_L8) bytesPerPixel = 1u;
            else if (info.m_format == D3DFMT_A8R8G8B8) bytesPerPixel = 4u;
            else if (info.m_format == D3DFMT_R8G8B8) bytesPerPixel = 3u;
            bytes = pixelCount*bytesPerPixel;
        }
        static_cast<void>(VID::useTextureMemory = VID::useTextureMemory-bytes);
        if (textureObject)
        {
            delete textureObject;
            textureObject = 0u;
        }

        if (lruPrevious)
        {
            if (VID::lastUse == this)
                static_cast<void>(VID::lastUse = lruNext);
            lruPrevious->lruNext = lruNext;
            lruNext->lruPrevious = lruPrevious;
            lruNext = 0u;
            lruPrevious = 0u;
            if (VID::lastUse == this)
                static_cast<void>(VID::lastUse = 0);
        }
    }

    if (indexBuffer)
    {
        IUnknown* const object = indexBuffer;
        (void)object->Release();
        indexBuffer = 0u;
    }

    if (vertexBuffer)
    {
        const unsigned int displayValue = Graph->vertexShaderVersion;
        const unsigned int bytesPerEntry = displayValue<256u ? 20u : 12u;
        static_cast<void>(VID::useVertexMemory = VID::useVertexMemory-static_cast<unsigned int>(vertexCount)*bytesPerEntry);
        if (vertexBuffer)
        {
            IUnknown* const object = vertexBuffer;
            (void)object->Release();
            vertexBuffer = 0u;
        }
    }
}

void VID::ResetGridZ(const SPRITE* sprite)
{
    if (sprite == Mouse || noGridZ == 0) return;
    if (!sprite->ExtraData())
        Error(E_ERROR, STRING("not exData for SetGrid"), 0u);

    const int frame = BitsToSigned32(sprite->ExtraData()->reserved38);
    if (frame<0) return;

    int begin = 0;
    int end = noGridZ;
    if (gridCadrShift)
    {
        const int frameCount = static_cast<int>(noCadr);
        begin = (frame<frameCount)?gridCadrShift[frame]:0;
        end = (frame<frameCount-1)?gridCadrShift[frame+1]:noGridZ;
    }
    const VECTOR scale = sprite->GetScale();
    for (int i = begin;i<end;++i)
        Map->ClearTempGroundZ(sprite->Coor()+gridZ[i]*scale);
}

void VID::SetGridZ(const SPRITE* sprite)
{
    if (sprite == Mouse || noGridZ == 0) return;
    if (!sprite->ExtraData())
        Error(E_ERROR, STRING("not exData for SetGrid"), 0u);

    sprite->ExtraData()->reserved38 = sprite->CurrentFrame();
    const int frame = sprite->CurrentFrame();

    int begin = 0;
    int end = noGridZ;
    if (gridCadrShift)
    {
        const int frameCount = static_cast<int>(noCadr);
        begin = (frame<frameCount)?gridCadrShift[frame]:0;
        end = (frame<frameCount-1)?gridCadrShift[frame+1]:noGridZ;
    }
    const VECTOR scale = sprite->GetScale();
    for (int i = begin;i<end;++i)
        Map->SetTempGroundZ(sprite->Coor()+gridZ[i]*scale);
}

static int VidFontInteger(double value)
{
    return value >= -2147483648.0 && value < 2147483648.0
        ? static_cast<int>(value) : (-2147483647 - 1);
}

VID_FONT::VID_FONT(VID* source, const STRING& fontName) : VID()
{
    frameSpeedDefault = 71u;
    noCadr = 256;
    vidSizeX = static_cast<short>(TruncateFloatToInt32OrMin(source->sizeXYZ.x));
    vidSizeY = static_cast<short>(TruncateFloatToInt32OrMin(source->sizeXYZ.y));
    buildSizeX = source->sizeXYZ.x;
    buildSizeY = source->sizeXYZ.y;
    type = 0x4000u;
    vidName = fontName;
    fontBackend = 0;

    LOGFONTA descriptor;

    HDC const dc = ::GetDC(0);
    const int dpiY = ::GetDeviceCaps(dc, LOGPIXELSY);
    const int scaledDpiY = VidFontInteger(static_cast<double>(dpiY)*source->scaleXYZ.y);
    descriptor.lfHeight = BitsToSigned32(0u-Signed32ToBits(::MulDiv(static_cast<int>(vidSizeY), scaledDpiY, 72)));
    const int dpiX = ::GetDeviceCaps(dc, LOGPIXELSX);
    const int scaledDpiX = VidFontInteger(static_cast<double>(dpiX)*source->scaleXYZ.x);
    descriptor.lfWidth = BitsToSigned32(0u-Signed32ToBits(::MulDiv(static_cast<int>(vidSizeX), scaledDpiX, 72)));

    descriptor.lfEscapement = 0;
    descriptor.lfOrientation = 0;
    descriptor.lfWeight = (property&0x00200000u)?700:400;
    descriptor.lfItalic = (property&0x00400000u)?1u:0u;
    descriptor.lfUnderline = 0u;
    descriptor.lfStrikeOut = 0u;
    descriptor.lfCharSet = 1u;
    descriptor.lfOutPrecision = 0u;
    descriptor.lfClipPrecision = 0u;
    descriptor.lfQuality = 1u;
    descriptor.lfPitchAndFamily = 1u;
    const char* const face = fontName.value;
    const unsigned int faceBytes = static_cast<unsigned int>(strlen(face))+1u;
    __movsd(reinterpret_cast<unsigned long*>(descriptor.lfFaceName),
        reinterpret_cast<const unsigned long*>(face), faceBytes>>2u);
    __movsb(reinterpret_cast<unsigned char*>(descriptor.lfFaceName)+(faceBytes&~3u),
        reinterpret_cast<const unsigned char*>(face)+(faceBytes&~3u), faceBytes&3u);
    (void)::ReleaseDC(0, dc);

    GRAPH* const graph = Graph;
    const HRESULT hr = D3DXCreateFontIndirect(graph->device, &descriptor, &fontBackend);
    if (hr<0)
    {
        STRING subject = "font: "+fontName;
        Error(E_CREATE, subject, static_cast<unsigned long>(hr));
        return;
    }

    RECT rect = {0, 0, 1, 1};
    static const char measureText[] = "X";
    (void)fontBackend->DrawTextA(measureText, 1, &rect, 0x0D20u, 0xFFFFFFFFu);
    vidSizeX = static_cast<short>(rect.right);
    vidSizeY = static_cast<short>(rect.bottom);
}

VID_SURFACE::VID_SURFACE(VID* source, RESOURCE* resource, const STRING& name)
{
    unsigned int flags = surfaceFlags;
    flags = (flags&0xFFFFFF00u)|(((flags&0xFFu)&0xFEu)|2u);
    this->surfaceBackend = 0;
    this->mesh = new(std::nothrow) MESH();
    this->textureAtlas = 0;
    this->frames = 0;
    this->surfaceData = 0;
    this->palette = 0;
    this->surfaceFlags = flags;
    this->recolorPalette = 0;
    (void)(this->vidName = name);
    this->type = 0x41u;
    this->noCadr = (source && source->spriteClass == 19u)?256:1;

    if (this->noCadr == 1)
    {

        const STRING stem = name.BeforeLast(".");
        const int stemLength = stem.Length();
        const int stemTail = (stemLength != 0)?static_cast<signed char>(stem[stemLength-1]):0;
        if (isdigit(stemTail) != 0)
        {
            const STRING suffix = name.AfterLast("_");
            if (isdigit(static_cast<signed char>(suffix[0])) != 0)
            {
                for (;;)
                {
                    bool found;
                    {
                        const STRING candidate = name.Add(this->noCadr);
                        std::FILE* file = 0;
                        if (!candidate.IsEmpty())
                            file = std::fopen(candidate.CharPtr(), "rb");
                        if (file) (void)std::fclose(file);
                        found = file != 0;
                    }
                    if (!found) break;
                    ++this->noCadr;
                }
            }
        }
    }

    if (resource) LoadParameters(resource);

    this->surfaceMode = (!(this->sizeXYZ.z>this->sizeXYZ.y))?1:0;
    this->renderMode = 0u;

    D3DXIMAGE_INFO imageInfo;
    (void)D3DXGetImageInfoFromFileA(name.CharPtr(), &imageInfo);
    this->pixelFormat = imageInfo.Format;

    unsigned int imageWidth = imageInfo.Width;
    unsigned int imageHeight = imageInfo.Height;
    if (this->spriteClass == 19u)
    {
        imageWidth = (imageWidth>>4u)-1u;
        imageHeight = (imageHeight>>4u)-1u;
    }
    this->vidSizeX = static_cast<short>(imageWidth);
    this->vidSizeY = static_cast<short>(imageHeight);
    this->buildSizeX = static_cast<float>(this->vidSizeX);
    this->buildSizeY = static_cast<float>(this->vidSizeY);

    if (this->pixelFormat == D3DFMT_A8R8G8B8 || this->pixelFormat == D3DFMT_A4R4G4B4 ||
       this->pixelFormat == D3DFMT_A1R5G5B5 || this->pixelFormat == D3DFMT_DXT3 ||
       this->pixelFormat == D3DFMT_DXT5)
        this->type = static_cast<unsigned short>(this->type|2u);

    const int frameCount = static_cast<int>(this->noCadr);
    this->frames = new(std::nothrow) CADR_DATA[frameCount];
    for (int frameIndex = 0;frameIndex<frameCount;++frameIndex)
        this->frames[frameIndex].height = 1u;

}

void VID_FONT::CalcTextSize(const STRING& textValue, int* outMaxWidth, int* outHeight,
    int* outLineCount, LIST<int>* lineOffsets, unsigned long alignFlags) const
{
    if (!fontBackend)
    {
        Error(E_ERROR, STRING("not font for CalcText"), 0);
        return;
    }

    const VID_FONT* selected = this;
    RECT measureRect = {0, 0, 1, 1};
    const char* const text = textValue.CharPtr();
    int lineWidth = 0;
    *outMaxWidth = 0;
    *outHeight = 0;
    *outLineCount = 0;
    lineOffsets->Release();

    int index = 0;
    int runLength = 0;
    for (;text[index];++index, ++runLength)
    {
        const char ch = text[index];
        const bool lineBreak = ch == '\n' || ch == '\r';
        const bool alignTag = ch == '<' && strncmp(text+index, "<Align=", 7) == 0;
        const bool fontTag = ch == '<' && strncmp(text+index, "<Font=", 6) == 0;
        const bool colorTag = ch == '<' && strncmp(text+index, "<Color=", 7) == 0;
        if (!lineBreak && !alignTag && !fontTag && !colorTag) continue;

        if (runLength>0)
        {
            (void)selected->fontBackend->DrawTextA(text+index-runLength, runLength,
                &measureRect, DT_CALCRECT|DT_NOCLIP|DT_SINGLELINE|DT_NOPREFIX, 0xFFFFFFFFu);
            lineWidth += measureRect.right;
        }

        if (lineBreak)
        {
            *outHeight += selected->vidSizeY;
            ++*outLineCount;
            runLength = -1;
            ++index;
            lineOffsets->Insert(lineWidth);
            if (lineWidth>*outMaxWidth) *outMaxWidth = lineWidth;
            lineWidth = 0;
        }
        else
        {
            runLength = -1;
            if (alignTag)
            {
                lineWidth = selected->vidSizeX*std::atoi(text+index+7);
            }
            else if (fontTag)
            {
                VID* const candidate = Map->Vid(std::atoi(text+index+6));
                if (candidate && candidate != EmptyVid && (candidate->type&0x4000u) != 0u &&
                    static_cast<VID_FONT*>(candidate)->fontBackend)
                    selected = static_cast<VID_FONT*>(candidate);
            }
            while (text[index] != '>' && text[index]) ++index;
        }
    }

    if (runLength>0)
    {
        (void)selected->fontBackend->DrawTextA(text+index-runLength, runLength,
            &measureRect, DT_CALCRECT|DT_NOCLIP|DT_SINGLELINE|DT_NOPREFIX, 0xFFFFFFFFu);
        *outHeight += measureRect.bottom;
        lineWidth += measureRect.right;
    }
    ++*outLineCount;
    lineOffsets->Insert(lineWidth);
    if (lineWidth>*outMaxWidth) *outMaxWidth = lineWidth;

    for (int line = 0;line<lineOffsets->No();++line)
    {
        if ((alignFlags&1u) != 0u) *(*lineOffsets)[line] = -*(*lineOffsets)[line]/2;
        else if ((alignFlags&2u) != 0u) *(*lineOffsets)[line] = -*(*lineOffsets)[line];
        else *(*lineOffsets)[line] = 0;
    }
}

namespace {
static VECTOR EvaluateVidHardwareCurve(
    const WEAPON_RUNTIME* owner,const VECTOR* keys,float phase);
static int VidHardwareHalfTowardZero(int value);
}

namespace {
struct VidShadowVertex
{
    float x;
    float y;
    float z;
    float rhw;
    float u;
    float v;
};

static bool VidShadowOutsideRuntimeBounds(const VID* owner, float x, float y, const VECTOR& scale)
{
    const float extentX = owner->buildSizeX*(0.5f)*scale.x;
    const float extentY = owner->buildSizeY*(0.5f)*scale.y;
    if (x-extentX>VID::viewMax.x &&
       y-extentY>VID::viewMax.y)
        return true;
    if (!(x+extentX >= VID::viewMin.x) &&
       !(y+extentY >= VID::viewMin.y))
        return true;
    return false;
}

static bool VidShadowUsesDirectionalQuad(const CADR_DATA* frame)
{
    const int width = static_cast<int>(static_cast<short>(frame->width));
    const int height = static_cast<int>(static_cast<short>(frame->height));
    const int maximum = width>height?width:height;
    const float threshold = frame->maxZ*(1.79999995f);
    return threshold<static_cast<float>(maximum);
}
}

void VID_SURFACE::DrawShadow(const SPRITE* spriteValue, const VECTOR& offset)
{
    VID_SURFACE* const self = this;

    CADR_DATA* const frameBase = self->frames;
    if (!frameBase || (self->runtimeFlags&kVidRuntimeHidden) != 0u ||
        self->renderMode != 0u || (self->type&2u) != 0u) return;

    const SPRITE* const sprite = spriteValue;
    const VECTOR* const shadowOrigin = &offset;

    const float directionX = shadowOrigin->x-sprite->Coor().x;
    const float directionY = shadowOrigin->y-sprite->Coor().y;
    const unsigned char shadowDirection = static_cast<unsigned char>(
        Decart2Polar(directionX, directionY).value+128u);
    unsigned char drawDirection = shadowDirection;

    CADR_DATA* frame = 0;
    bool directionalQuad = false;
    const unsigned int ownerProperty = self->property;
    if ((ownerProperty&0x20000000u) != 0u)
    {
        const int relativeFrame = static_cast<int>(sprite->CurrentFrame())-sprite->m_currentFrameBegin;
        const unsigned char relativeDirection = static_cast<unsigned char>(
            sprite->Direction().value-shadowDirection);
        const int realDirection = self->RealDirectionWithGraphDetail(relativeDirection);
        const int animation = sprite->Animation();
        const int frameIndex = relativeFrame+self->animationBaseFrame[animation]+
            realDirection*self->animationFrameCount[animation];
        frame = frameBase+frameIndex;

        directionalQuad = VidShadowUsesDirectionalQuad(frameBase);
    }
    else
    {
        frame = frameBase+static_cast<int>(sprite->CurrentFrame());
        directionalQuad = VidShadowUsesDirectionalQuad(frame);
    }

    if (frame->height == 0u) return;

    VECTOR scale;
    const EX_SPRITE_DATA* const transformOwner =
        sprite->ExtraData();
    if (transformOwner) scale = transformOwner->scale;
    else
    {
        scale.x = 1.0f;
        scale.y = 1.0f;
        scale.z = 1.0f;
    }
    scale.x *= self->scaleXYZ.x;
    scale.y *= self->scaleXYZ.y;
    scale.z *= self->scaleXYZ.z;

    float positionX = sprite->Coor().x;
    float positionY = sprite->Coor().y-sprite->Coor().z;
    if ((ownerProperty&0x00010000u) != 0u)
    {
        const unsigned int waveIndex = (::CurrentTime>>3u)&0xFFu;
        positionY -= FSin[waveIndex]*self->topZ;
    }

    const unsigned int drawFlags = self->runtimeFlags;
    if ((drawFlags&kVidAnimateScale) != 0u)
    {
        const WEAPON_RUNTIME* const curveOwner =
            self->weapon;
        const float phase = transformOwner->effectCurvePosition;
        const VECTOR curveScale = EvaluateVidHardwareCurve(curveOwner, curveOwner->Scale, phase);
        scale.x *= curveScale.x;
        scale.y *= curveScale.y;
        scale.z *= curveScale.z;
    }
    if ((drawFlags&kVidAnimateOffset) != 0u)
    {
        const WEAPON_RUNTIME* const curveOwner =
            self->weapon;
        const float phase = transformOwner->effectCurvePosition;
        const VECTOR curveOffset = EvaluateVidHardwareCurve(curveOwner, curveOwner->Shift, phase);
        positionX += curveOffset.x;
        positionY += curveOffset.y;
    }

    if (VidShadowOutsideRuntimeBounds(self, positionX, positionY, scale)) return;

    if (directionalQuad && (ownerProperty&0x20000000u) != 0u)
    {
        if ((ownerProperty&0x00002000u) != 0u) drawDirection = sprite->Direction().value;
        else
        {
            const unsigned char relativeDirection = static_cast<unsigned char>(
                sprite->Direction().value-shadowDirection);
            const int realDirection = self->RealDirectionWithGraphDetail(relativeDirection);
            const int correction = (realDirection<<8)/self->noDir;
            drawDirection = static_cast<unsigned char>(sprite->Direction().value-static_cast<unsigned char>(correction));
        }
    }

    self->lastRenderUseTick = ::RealCurrentTime;
    frame->SetTextureToDevice(self);
    if (FloatEqualOrUnordered(frame->minZ, -999999.0f))
        frame->CalcMaxMinZ(self);
    if (!(frame->maxZ>(0.0f))) return;

    const VECTOR2 groundPoint(sprite->Coor().x, sprite->Coor().y);
    float groundDelta = sprite->Coor().z-Map->GetGroundZ(groundPoint);
    if (groundDelta<0.0f) groundDelta = 0.0f;

    const int width = static_cast<int>(static_cast<short>(frame->width));
    const int height = static_cast<int>(static_cast<short>(frame->height));
    const int halfOwnerX = VidHardwareHalfTowardZero(self->vidSizeX);
    const int halfOwnerY = VidHardwareHalfTowardZero(self->vidSizeY);
    const int frameX = static_cast<int>(static_cast<short>(frame->offsetX));
    const int frameY = static_cast<int>(static_cast<short>(frame->offsetY));
    const int xSource[4] = {0, width, 0, width};
    const int ySource[4] = {0, 0, height, height};
    const TEXTURE* const texture = frame->textureObject;
    const float textureWidth = static_cast<float>(texture->m_width);
    const float textureHeight = static_cast<float>(texture->m_height);
    VidShadowVertex vertices[4];
    for (int i = 0;i<4;++i)
    {
        vertices[i].x = static_cast<float>(xSource[i]-halfOwnerX+frameX);
        vertices[i].y = static_cast<float>(ySource[i]-halfOwnerY+frameY)-groundDelta;
        vertices[i].z = (i<2?frame->maxZ*(0.300000012f):frame->minZ);
        vertices[i].rhw = 1.0f;
        vertices[i].u = static_cast<float>(xSource[i])/textureWidth;
        vertices[i].v = static_cast<float>(ySource[i])/textureHeight;
    }

    const ANGLE rotation(drawDirection);
    const float rotateXX = rotation.Cos();
    const float rotateXY = rotation.Sin();
    MAP* const world = Map;
    for (int i = 0;i<4;++i)
    {
        VidShadowVertex& vertex = vertices[i];
        vertex.x *= scale.x;
        vertex.y *= scale.y;
        vertex.z *= scale.z;
        vertex.z = (sprite->Coor().z+vertex.z+(128.0f))*
            (0.000122070312f);

        if (!directionalQuad || (ownerProperty&0x20000000u) != 0u)
        {
            const float x = vertex.x*rotateXX-vertex.y*rotateXY;
            const float y = vertex.y*rotateXX+vertex.x*rotateXY;
            vertex.x = x;
            vertex.y = y;
        }
        if (directionalQuad)
        {
            const float radius = frame->maxZ*(-0.600000024f);
            const ANGLE shadowAngle(shadowDirection);
            vertex.x -= radius*shadowAngle.Sin();
            vertex.y += radius*shadowAngle.CosY();
        }

        const float worldX = positionX+vertex.x;
        const float worldY = positionY+vertex.y;
        if (self->layer != 11)
        {
            const VECTOR2 projected = world->ToScreenScaled(VECTOR2(worldX, worldY));
            vertex.x = projected.x;
            vertex.y = projected.y;
        }
        else
        {
            vertex.x = worldX-world->ShiftXY().x;
            vertex.y = worldY-world->ShiftXY().y;
        }
        vertex.y += groundDelta;
    }

    GRAPH* const graph = Graph;
    IDirect3DDevice9* const device = graph->device;
    (void)device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2u, vertices, sizeof(VidShadowVertex));
    static_cast<void>(VID_SURFACE::noDrawShadow = VID_SURFACE::noDrawShadow+1u);
}

static unsigned int VidFontColor(const GAMMA& gamma)
{
    const unsigned int color = gamma.add;
    if ((color&0xFF000000u) != 0u) return color;
    return (color&0x00FFFFFFu) != 0u ? color|0xFF000000u : 0xFFFFFFFFu;
}

static void VidDrawFontRun(const VID_FONT* font, int x, int y,
    const char* text, int textLength, unsigned int color)
{
    RECT rect = {x, y, x+1, y+1};
    if (*text == 'W')
    {
        rect.left -= static_cast<int>(font->vidSizeX)/2;
        char prefixed[4096] = " ";
        std::strcat(prefixed, text);
        (void)font->fontBackend->DrawTextA(prefixed, textLength+1,
            &rect, DT_NOCLIP|DT_SINGLELINE|DT_NOPREFIX, color);
    }
    else
    {
        (void)font->fontBackend->DrawTextA(text, textLength,
            &rect, DT_NOCLIP|DT_SINGLELINE|DT_NOPREFIX, color);
    }
}

void VID_FONT::DrawTextA(int startX, int startY, const STRING& textValue, int textLength,
    int firstLine, int lastLine, COLOR colorValue, const LIST<int>& lineOffsets) const
{
    if (!fontBackend || textLength == 0 || !textValue[0]) return;

    const VID_FONT* selected = this;
    const unsigned int originalColor = colorValue.value;
    unsigned int currentColor = originalColor;
    RECT measureRect = {0, 0, 1, 1};
    int line = 0;
    int x = startX+*lineOffsets[line];
    int y = startY;
    const char* const text = textValue.CharPtr();
    ++lastLine;
    (void)Graph->device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
    (void)Graph->device->SetRenderState(D3DRS_ZWRITEENABLE, 0u);

    int index = 0;
    int runLength = 0;
    for (;index<textLength && line<=lastLine;++index, ++runLength)
    {
        const char ch = text[index];
        const bool lineBreak = ch == '\n' || ch == '\r';
        const bool alignTag = ch == '<' && strncmp(text+index, "<Align=", 7) == 0;
        const bool fontTag = ch == '<' && strncmp(text+index, "<Font=", 6) == 0;
        const bool colorTag = ch == '<' && strncmp(text+index, "<Color=", 7) == 0;
        if (!lineBreak && !alignTag && !fontTag && !colorTag) continue;

        if (runLength>0 && line>=firstLine)
        {
            while (x<0 && runLength != 0)
            {
                (void)selected->fontBackend->DrawTextA(text+index-runLength, 1,
                    &measureRect, DT_CALCRECT|DT_NOCLIP|DT_SINGLELINE|DT_NOPREFIX, currentColor);
                --runLength;
                x += measureRect.right;
            }
            if (!lineBreak)
                (void)selected->fontBackend->DrawTextA(text+index-runLength, runLength,
                    &measureRect, DT_CALCRECT|DT_NOCLIP|DT_SINGLELINE|DT_NOPREFIX, currentColor);
            VidDrawFontRun(selected, x, y, text+index-runLength, runLength, currentColor);
            if (!lineBreak) x += measureRect.right;
        }

        runLength = -1;
        if (lineBreak)
        {
            if (text[index+1] == '\n' || text[index+1] == '\r') ++index;
            y += selected->vidSizeY;
            ++line;
            x = startX+*lineOffsets[line];
        }
        else
        {
            if (alignTag)
            {
                x = startX+selected->vidSizeX*std::atoi(text+index+7);
            }
            else if (fontTag)
            {
                const int fontIndex = std::atoi(text+index+6);
                VID* const candidate = Map->Vid(fontIndex);
                if (candidate && candidate != EmptyVid && (candidate->type&0x4000u) != 0u &&
                    static_cast<VID_FONT*>(candidate)->fontBackend)
                {
                    selected = static_cast<VID_FONT*>(candidate);
                    currentColor = VidFontColor(selected->gammaRaw);
                }
                else
                {
                    Error(E_INVALID, STRING("font in DrawText()"), static_cast<unsigned long>(fontIndex));
                }
            }
            else if (colorTag)
            {
                int parsed = static_cast<int>(currentColor);
                (void)sscanf(text+index+7, "%i", &parsed);
                currentColor = static_cast<unsigned int>(parsed);
                if (currentColor == 0u) currentColor = originalColor;
                else if ((currentColor&0xFF000000u) == 0u) currentColor |= 0xFF000000u;
            }
            while (text[index] != '>' && index<textLength) ++index;
        }
    }

    if (line>=firstLine && line<=lastLine && runLength>0)
    {
        while (x<0 && runLength != 0)
        {
            (void)selected->fontBackend->DrawTextA(text+index-runLength, 1,
                &measureRect, DT_CALCRECT|DT_NOCLIP|DT_SINGLELINE|DT_NOPREFIX, currentColor);
            --runLength;
            x += measureRect.right;
        }
        VidDrawFontRun(selected, x, y, text+index-runLength, runLength, currentColor);
    }
}

static void VidReportResourceError(const VID* owner, TYPE_ERROR type, const char* text, int value)
{
    STRING subject;
    subject = text;
    owner->Error(type, subject, static_cast<unsigned long>(value));
}

static long VidResourceTell(RESOURCE* resource)
{
    return static_cast<long>(BitsToSigned32(static_cast<unsigned int>(std::ftell(resource->FileHandle()))));
}

static unsigned int VidResourceSectionSize(const RESOURCE* resource)
{
    return resource->RecordSize();
}

static int VidResourceBytesToEndOfSubrecord(RESOURCE* resource)
{
    const long position = VidResourceTell(resource);
    const int currentPosition = resource->DataPosition();
    const int currentSize = resource->SubRecordSize();
    return currentPosition-static_cast<int>(position)+currentSize+4;
}

static void VidSkipResourceBytes(RESOURCE* resource, long bytes)
{
    resource->SetIoState(2u);
    (void)std::fseek(resource->FileHandle(), BitsToSigned32(Signed32ToBits(static_cast<int>(bytes))), SEEK_CUR);
}

template<class T>
static void VidReadCachedField(const unsigned char*& cursor, T& value)
{
    std::memcpy(&value, cursor, sizeof(value));
    cursor += sizeof(value);
}

namespace {
static void VidReleaseFrameArray(VID_SURFACE& surface)
{
    if (surface.nextMirror != &surface) return;
    if (surface.frames)
    {
        delete[] surface.frames;
        surface.frames = 0;
    }
    surface.surfaceData = 0;
}
}

static int VidPixelFormatBits(D3DFORMAT format)
{
    switch (format)
    {
    case D3DFMT_R8G8B8: return 24;
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8: return 32;
    case D3DFMT_R5G6B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_A4R4G4B4: return 16;
    case D3DFMT_P8:
    case D3DFMT_L8: return 8;
    case D3DFMT_DXT1: return 4;
    case D3DFMT_DXT3:
    case D3DFMT_DXT5: return 8;
    default: return 0;
    }
}

void CADR_DATA::CopyToTexture(TEXTURE* texture, VID_SURFACE* surface, int x, int y)
{
    const int imageWidth = static_cast<short>(width);
    const int imageHeight = static_cast<short>(height);
    const unsigned short flags = surface->type;
    const bool hasDetail = (flags&0x8000u) != 0u;
    const bool hasNormal = (flags&0x2000u) != 0u;
    const bool hasDepth = (flags&4u) != 0u;
    RECT rect = {x, y, x+imageWidth, y+imageHeight*(hasNormal ? 2 : 1)};
    if ((flags&0x10u) != 0u) rect.left = 999999;
    int pitch;
    unsigned char* destination = texture->Lock(&pitch, &rect);
    if (!destination)
    {
        surface->Error(E_LOCK, STRING("texture"), static_cast<unsigned long>(this-surface->frames));
        return;
    }
    const int normalPlaneOffset = imageHeight*pitch;
    const unsigned char* source = surface->surfaceData+frameDataOffset+
        2u*indexCount+4u*(vertexCount+extraDataCount);
    const D3DFORMAT sourceFormat = surface->pixelFormat;
    const int sourceBits = VidPixelFormatBits(sourceFormat);
    const int sourceStride = sourceBits/8+hasDetail+hasNormal+hasDepth*2;
    if (sourceFormat == D3DFMT_DXT1 || sourceFormat == D3DFMT_DXT3 || sourceFormat == D3DFMT_DXT5)
    {
        memcpy(destination, source, imageWidth*imageHeight*sourceBits/8);
    }
    else if ((flags&8u) == 0u)
    {
        const bool expand4444 = sourceFormat == D3DFMT_A4R4G4B4 && texture->m_format == D3DFMT_A8R8G8B8;
        const int destinationStride = sourceFormat == D3DFMT_A8R8G8B8 || expand4444 ? 4 :
            (sourceFormat == D3DFMT_R8G8B8 ? 3 : 2);
        while (*reinterpret_cast<const unsigned short*>(source) != 0u)
        {
            const unsigned int skip = *source++;
            if (expand4444)
            {
                for (unsigned int i = 0; i<skip; ++i)
                    *reinterpret_cast<unsigned short*>(destination+i*4u) = 0;
            }
            else if (sourceFormat != D3DFMT_R8G8B8)
                memset(destination, 0, skip*destinationStride);
            destination += skip*destinationStride;
            unsigned int count = *source++;
            while (count-- != 0u)
            {
                if (sourceFormat == D3DFMT_A8R8G8B8)
                    *reinterpret_cast<unsigned int*>(destination) = *reinterpret_cast<const unsigned int*>(source);
                else if (sourceFormat == D3DFMT_R8G8B8)
                    *reinterpret_cast<unsigned int*>(destination) |= *reinterpret_cast<const unsigned int*>(source)&0xFFFFFFu;
                else if (expand4444)
                {
                    const unsigned int color = *reinterpret_cast<const unsigned short*>(source);
                    *reinterpret_cast<unsigned int*>(destination) = ((color&0xF000u)<<16u)|
                        ((color&0xF00u)<<12u)|((color&0xF0u)<<8u)|((color&15u)<<4u);
                }
                else
                    *reinterpret_cast<unsigned short*>(destination) = *reinterpret_cast<const unsigned short*>(source);
                source += sourceStride;
                destination += destinationStride;
            }
        }
    }
    else
    {
        const unsigned int* const activePalette = surface->recolorPalette ? surface->recolorPalette : surface->palette;
        const bool argb8888 = texture->m_format == D3DFMT_A8R8G8B8;
        const bool argb4444 = texture->m_format == D3DFMT_A4R4G4B4 && !(hasDetail || hasNormal || hasDepth);
        const int destinationStride = argb8888 ? 4 : 2;
        while (*reinterpret_cast<const unsigned short*>(source) != 0u)
        {
            const unsigned int skip = *source++;
            memset(destination, 0, skip*destinationStride);
            destination += skip*destinationStride;
            unsigned int count = *source++;
            while (count-- != 0u)
            {
                const unsigned int color = activePalette[*source++];
                if (argb8888)
                {
                    *reinterpret_cast<unsigned int*>(destination) = color;
                    if (hasDetail && hasNormal)
                    {
                        const unsigned int detail = *source++;
                        const unsigned int normal = *source++;
                        *reinterpret_cast<unsigned int*>(destination+normalPlaneOffset) =
                            COLOR(normal&0xF8u, ((detail>>3u)&0x18u)+(normal<<5u), (detail&0x3Fu)*4u).value;
                    }
                    else
                    {
                        if (hasDetail) ++source;
                        if (hasNormal)
                        {
                            const unsigned int normal = *source++;
                            *reinterpret_cast<unsigned int*>(destination+normalPlaneOffset) =
                                COLOR(normal&0xF8u, normal<<5u, 0).value;
                        }
                    }
                }
                else
                {
                    *reinterpret_cast<unsigned short*>(destination) = static_cast<unsigned short>(argb4444 ?
                        ((color>>16u)&0xF000u)|((color>>12u)&0xF00u)|((color>>8u)&0xF0u)|((color>>4u)&15u) :
                        ((color>>6u)&0x3E0u)|((color>>3u)&0x1Fu)|((color>>9u)&0xFC00u)|0x8000u);
                    if (hasDetail && hasNormal)
                    {
                        const unsigned int normal = *reinterpret_cast<const unsigned short*>(source);
                        source += 2;
                        *reinterpret_cast<unsigned short*>(destination+normalPlaneOffset) =
                            static_cast<unsigned short>((normal>>1u)|0x8000u);
                    }
                    else
                    {
                        if (hasDetail) ++source;
                        if (hasNormal)
                            *reinterpret_cast<unsigned short*>(destination+normalPlaneOffset) =
                                static_cast<unsigned short>((*source++|0xFF00u)<<7u);
                    }
                }
                if (hasDepth) source += 2;
                destination += destinationStride;
            }
        }
    }
    texture->UnLock();
}

void CADR_DATA::SetTextureToDevice(VID_SURFACE* surface)
{
    if (textureObject == 0u && height != 0u)
    {
        D3DFORMAT desiredFormat = surface->pixelFormat;
        if (desiredFormat == D3DFMT_P8)
        {
            desiredFormat = ((surface->type&2u) != 0u || (Graph->capabilityFlags&0x8000u) != 0u)?D3DFMT_A8R8G8B8:D3DFMT_A1R5G5B5;
        }
        const unsigned int memoryMB = Graph->maxTextureMemoryMB;
        const unsigned int memoryBudget = memoryMB >= 1024u ? 256000000u :
            (memoryMB >= 384u ? 192000000u : (memoryMB >= 256u ? 128000000u : 90000000u));
        CADR_DATA* root = VID::lastUse;
        while (root)
        {
            CADR_DATA* const oldest = root->lruPrevious;
            const unsigned int age = ::RealCurrentTime-oldest->lastUseTick;
            if (age <= 5000u) break;
            if (VID::useTextureMemory <= memoryBudget && age <= 20000u) break;
            oldest->ReleaseVideoMemory();
            root = VID::lastUse;
        }

        D3DXIMAGE_INFO imageInfo;
        const bool fileTexture = (surface->surfaceFlags&2u) != 0u;
        if (fileTexture)
        {
            const int frameIndex = static_cast<int>(this-surface->frames);
            STRING frameName = surface->vidName.Add(frameIndex);
            const unsigned long colorKey = surface->spriteClass == 19u ? 0xFF000000u : 0u;
            textureObject = new(std::nothrow) TEXTURE(frameName, colorKey, &imageInfo);
            TEXTURE* const texture = textureObject;
            if (texture->m_texture != 0 || texture->m_cpuData != 0)
            {
                width = static_cast<unsigned short>(surface->spriteClass == 19u?
                    (imageInfo.Width>>4u)-1u:imageInfo.Width);
                height = static_cast<unsigned short>(surface->spriteClass == 19u?
                    (imageInfo.Height>>4u)-1u:imageInfo.Height);
            }
        }
        else
        {
            const int textureHeight = (static_cast<int>(static_cast<short>(height))*
                ((surface->type&0x2000u) != 0u ? 2 : 1)+3)&~3;
            const int textureWidth = (static_cast<int>(static_cast<short>(width))+3)&~3;
            textureObject = new(std::nothrow) TEXTURE(textureWidth, textureHeight,
                desiredFormat, 3u);
        }

        TEXTURE* const texture =
            textureObject;
        const D3DFORMAT format = texture->m_format;
        const unsigned int pixels = texture->m_height*texture->m_width;
        unsigned int textureBytes;
        if (format == D3DFMT_DXT1) textureBytes = static_cast<unsigned int>(BitsToSigned32(pixels)/2);
        else if (format == D3DFMT_P8 || format == D3DFMT_L8) textureBytes = pixels;
        else if (format == D3DFMT_A8R8G8B8) textureBytes = pixels*4u;
        else if (format == D3DFMT_R8G8B8) textureBytes = pixels*3u;
        else textureBytes = pixels*2u;
        static_cast<void>(VID::useTextureMemory = VID::useTextureMemory+textureBytes);

        TEXTURE* const liveTexture = textureObject;
        if (liveTexture->m_texture == 0u && liveTexture->m_cpuData == 0u)
        {
            STRING subject("texture");
            const int frameIndex = static_cast<int>(this-surface->frames);
            surface->Error(E_CREATE, subject, static_cast<unsigned long>(frameIndex));
            return;
        }

        if (!fileTexture) CopyToTexture(textureObject, surface, 0, 0);
    }

    if (textureObject != 0u)
    {
        textureObject->SetTextureToDevice(0);
        lastUseTick = ::RealCurrentTime;
        if ((surface->surfaceFlags&1u) == 0u)
        {
            const bool linked = lruPrevious != 0u;
            CADR_DATA* root = VID::lastUse;
            if (linked)
            {
                if (root == this) static_cast<void>(VID::lastUse = lruNext);
                CADR_DATA* const previous = lruPrevious;
                CADR_DATA* const nextAddress = lruNext;
                previous->lruNext = nextAddress;
                CADR_DATA* const next = lruNext;
                CADR_DATA* const previousAddress = lruPrevious;
                next->lruPrevious = previousAddress;
                lruNext = 0u;
                lruPrevious = 0u;
                root = VID::lastUse;
                if (root == this) goto frameLruSelf;
            }
            if (root != 0u)
            {
                lruNext = root;
                lruPrevious = VID::lastUse->lruPrevious;
                lruPrevious->lruNext = this;
                lruNext->lruPrevious = this;
            }
            else
            {
frameLruSelf:
                lruNext = this;
                lruPrevious = this;
            }
            static_cast<void>(VID::lastUse = this);
        }
    }
}


namespace {
struct VidHardwareDrawTransform
{
    VECTOR scale;
    VECTOR position;
};
static VECTOR EvaluateVidHardwareCurve(
    const WEAPON_RUNTIME* owner, const VECTOR* keys, float phase)
{

    const int index = phase >= -2147483648.0f && phase < 2147483648.0f
        ? static_cast<int>(phase) : (-2147483647-1);
    if (index >= owner->maxColumn)
        return keys[owner->maxColumn];

    const float fraction = phase-static_cast<float>(index);
    const VECTOR first = keys[index];
    const VECTOR next = keys[index+1];
    const VECTOR delta = next-first;
    const VECTOR scaled = delta*fraction;
    return first+scaled;
}
static unsigned char SelectVidHardwareDrawDirection(
    VID_SURFACE* owner, const SPRITE* request, const float& speed, const float& zSpeed)
{

    const unsigned int ownerFlags = owner->property;
    unsigned char result = 0u;
    if ((ownerFlags&0x20000000u) == 0u) return result;

    if ((ownerFlags&0x00080000u) != 0u)
    {
        const EX_SPRITE_DATA* source =
            request->ExtraData();
        const VECTOR2 requestPosition = request->Coor().ViewCoor();
        const VECTOR2 sourcePosition = source->cachedPosition.ViewCoor();
        if (requestPosition.x != sourcePosition.x || requestPosition.y != sourcePosition.y)
        {
            const VECTOR2 delta = (requestPosition-sourcePosition)*VECTOR2(1.0f, 1.41430664f);
            return ANGLE(delta).value;
        }

        if (zSpeed == 0.0f)
            return request->Direction().value;
        const ANGLE directionAngle(request->Direction().value);
        const float first = directionAngle.Sin()*speed;
        const float second = static_cast<float>(-(static_cast<double>(directionAngle.Cos())*speed
            +zSpeed)/0.707060218f);
        return Decart2Polar(first, second).value;
    }

    if ((ownerFlags&0x00002000u) != 0u) return request->Direction().value;

    const unsigned char stored = request->Direction().value;
    VID* directionVid = request->Vid();
    const int realDirection = directionVid->RealDirectionWithGraphDetail(stored);
    const int directionCount = owner->noDir;
    const int correction = (realDirection<<8)/directionCount;
    return static_cast<unsigned char>(stored-static_cast<unsigned char>(correction));
}

static int VidHardwareHalfTowardZero(int value)
{
    return value/2;
}
}



VID_SOFTWARE::VID_SOFTWARE()
{
    frameOffsets = 0;
    allocatedBytes = 0u;
    mappedFileBase = 0;
}

VID_SOFTWARE::VID_SOFTWARE(VID_SOFTWARE& source) : VID()
{
    nextMirror = source.nextMirror;
    source.nextMirror = this;
    layer = source.layer;
    type = source.type;
    noCadr = source.noCadr;
    frameSpeedDefault = source.frameSpeedDefault;
    vidSizeX = source.vidSizeX;
    vidSizeY = source.vidSizeY;
    buildSizeX = source.buildSizeX;
    buildSizeY = source.buildSizeY;
    frameOffsets = source.frameOffsets;
    mappedFileBase = source.mappedFileBase;
    allocatedBytes = source.allocatedBytes;
    noGridZ = source.noGridZ;
    cadrLinkXYZ = source.cadrLinkXYZ;
    vidName = source.vidName;
    CopyBytes(paletteTables, source.paletteTables, sizeof(paletteTables));
    runtimeFlags = (runtimeFlags&~0x800u)|(source.runtimeFlags&kVidRuntimeHidden);
    storageFlags &= ~4u;

    if (noGridZ != 0)
    {
        const unsigned int bytes = static_cast<unsigned int>(noGridZ)*sizeof(VECTOR);
        gridZ = new(std::nothrow) VECTOR[static_cast<unsigned int>(noGridZ)];
        if (noGridZ>0)
            CopyBytes(gridZ, source.gridZ, bytes);
        if (source.gridCadrShift)
        {
            const int count = static_cast<int>(noCadr);
            gridCadrShift = new(std::nothrow) int[static_cast<unsigned int>(count)];
            if (count>0)
                CopyBytes(gridCadrShift, source.gridCadrShift,
                                 static_cast<unsigned int>(count)*sizeof(int));
        }
        else gridCadrShift = 0;
    }
}

VID_SOFTWARE::~VID_SOFTWARE()
{
    if (nextMirror == this)
    {
        if (mappedFileBase)
            (void)UnmapViewOfFile(mappedFileBase);
        mappedFileBase = 0;
        if (frameOffsets) delete[] frameOffsets;
        frameOffsets = 0;
        static_cast<void>(VID::MemoryInUse = VID::MemoryInUse-allocatedBytes);
        allocatedBytes = 0u;
    }
}

VID_LIGHT::VID_LIGHT() : VID()
{
    lightDataBytes = 0u;
    lightData = 0;
}

VID_LIGHT::VID_LIGHT(VID_LIGHT& source) : VID()
{
    nextMirror = source.nextMirror;
    source.nextMirror = this;
    layer = source.layer;
    type = source.type;
    noCadr = source.noCadr;
    frameSpeedDefault = source.frameSpeedDefault;
    vidSizeX = source.vidSizeX;
    vidSizeY = source.vidSizeY;
    buildSizeX = source.buildSizeX;
    buildSizeY = source.buildSizeY;
    lightData = source.lightData;
    lightDataBytes = source.lightDataBytes;
    cadrLinkXYZ = source.cadrLinkXYZ;
    extendedHeaderValue = source.extendedHeaderValue;
    lastRenderUseTick = source.lastRenderUseTick;
    storageFlags &= ~4u;
}

VID_LIGHT::~VID_LIGHT()
{
    if (nextMirror == this)
    {
        if (lightData) ::operator delete(lightData);
        lightData = 0;
        static_cast<void>(VID::MemoryInUse = VID::MemoryInUse-lightDataBytes);
        lightDataBytes = 0u;
    }
}

VID_FONT::VID_FONT(VID_FONT& source) : VID()
{
    nextMirror = source.nextMirror;
    source.nextMirror = this;
    layer = source.layer;
    type = source.type;
    noCadr = source.noCadr;
    frameSpeedDefault = source.frameSpeedDefault;
    vidSizeX = source.vidSizeX;
    vidSizeY = source.vidSizeY;
    buildSizeX = source.buildSizeX;
    buildSizeY = source.buildSizeY;
    cadrLinkXYZ = source.cadrLinkXYZ;
    fontBackend = source.fontBackend;
    lastRenderUseTick = source.lastRenderUseTick;
    storageFlags &= ~4u;
}

VID_FONT::~VID_FONT()
{
    if (nextMirror == this && fontBackend)
    {
        (void)fontBackend->Release();
        fontBackend = 0;
    }
}

VID_SURFACE::VID_SURFACE()
{
    const unsigned int flags = surfaceFlags&~3u;
    surfaceBackend = 0;
    mesh = new(std::nothrow) MESH();
    textureAtlas = 0;
    frames = 0;
    surfaceData = 0;
    palette = 0;
    renderMode = 0u;
    recolorPalette = 0;
    surfaceMode = -1;
    surfaceFlags = flags;
}

VID_SURFACE::~VID_SURFACE()
{
    if (surfaceBackend)
    {
        (void)surfaceBackend->Release();
        surfaceBackend = 0;
    }
    if (recolorPalette)
    {
        delete[] recolorPalette;
        recolorPalette = 0;
    }
    if (mesh)
    {
        mesh->Release();
        mesh = 0;
    }
    if (nextMirror == this)
    {
        delete textureAtlas;
        textureAtlas = 0;
        VidReleaseFrameArray(*this);
    }
}

namespace
{
    const short kEmptySoftwareGridCell=static_cast<short>(-32000);

    static int SoftwareGridCellIndex(int x, int y)
    {

        return (y>>3)*256+(x>>3);
    }

    static void SoftwareRasterizeDepthFrame(const unsigned char* frame, short* cells)
    {
        const int headerCount = *reinterpret_cast<const short*>(frame);
        const unsigned char* runs = frame+2+headerCount*6;
        int y = *reinterpret_cast<const short*>(runs);
        const int endY = y+*reinterpret_cast<const short*>(runs+2);
        runs += 4;

        while (y<endY)
        {
            int x = 0;
            while (*reinterpret_cast<const unsigned short*>(runs) != 0u)
            {
                const unsigned int skip = runs[0];
                const unsigned int count = runs[1];
                runs += 2;
                x += static_cast<int>(skip);

                const unsigned char* pixels = runs;
                for (unsigned int i = 0;i<count;++i, ++x, pixels += 2)
                {
                    const int z = (static_cast<int>(*reinterpret_cast<const unsigned short*>(pixels))>>3)-128;
                    const int projectedY = y+z;
                    if (x >= 0 && x<2048 && projectedY >= 0 && projectedY<2048)
                    {
                        short& cell = cells[SoftwareGridCellIndex(x, projectedY)];
                        if (z>cell) cell = static_cast<short>(z);
                    }
                }

                runs += count*3u;
            }
            runs += 2;
            ++y;
        }
    }

    static void SoftwareRasterizeMaskFrame(const unsigned char* frame, short* cells)
    {
        const int headerCount = *reinterpret_cast<const short*>(frame);
        const unsigned char* runs = frame+2+headerCount*6;
        int y = *reinterpret_cast<const short*>(runs);
        const int endY = y+*reinterpret_cast<const short*>(runs+2);
        runs += 4;

        while (y<endY)
        {
            int x = 0;
            while (*reinterpret_cast<const unsigned short*>(runs) != 0u)
            {
                const unsigned int skip = *runs++;
                const unsigned int count = *runs++;
                x += static_cast<int>(skip);
                for (unsigned int i = 0;i<count;++i, ++x)
                {
                    if (x >= 0 && x<2048 && y >= 0 && y<2048)
                        cells[SoftwareGridCellIndex(x, y)] = 0;
                }
                runs += count;
            }
            runs += 2;
            ++y;
        }
    }
}

void VID_SOFTWARE::Load(RESOURCE* resource)
{

    if ((type&0x0100u) != 0u)
    {
        SetPropHide(1);
        return;
    }

    {
        STRING fileName = resource->GetFileName();
        mappedFileBase = static_cast<unsigned char*>(FFileMapping(fileName));
    }

    if (!mappedFileBase)
    {
        VidReportResourceError(this, E_CREATE, "file mapping", 0);
        SetPropHide(1);
        return;
    }

    if ((type&0x0008u) != 0u)
    {
        if (resource->GoNext(ResourceType::Palette) == 0)
        {
            const long paletteOffset = VidResourceTell(resource);
            mappedPalette = mappedFileBase+paletteOffset;
        }
        else
        {
            VidReportResourceError(this, E_LOAD, "PAL ", 0);
        }
    }

    if (resource->GoNext(ResourceType::Data) != 0)
        VidReportResourceError(this, E_LOAD, "DATA", 0);

    allocatedBytes = VidResourceSectionSize(resource);
    if ((type&0x0008u) != 0u) allocatedBytes += 2048u;

    frameOffsets = new(std::nothrow) unsigned int[static_cast<unsigned int>(static_cast<int>(noCadr))];
    if (!frameOffsets)
    {
        {
            STRING subject("cadrShift");
            Error(E_MEMORY, subject, static_cast<unsigned long>(static_cast<int>(noCadr)));
        }
        return;
    }

    if ((type&0x0008u) != 0u)
    {
        for (unsigned int offset = 0;offset<1024u;offset += 4u)
            *reinterpret_cast<unsigned int*>(paletteTables+offset) =
                *reinterpret_cast<const unsigned int*>(mappedPalette+offset);
        __movsd(reinterpret_cast<unsigned long*>(paletteTables+8192u),
                reinterpret_cast<const unsigned long*>(paletteTables), 256u);
    }

    unsigned int frameBytes;
    for (int frameIndex = 0;frameIndex<static_cast<int>(noCadr);++frameIndex)
    {
        (void)resource->Read(&frameBytes, sizeof(frameBytes));

        if (frameBytes != 2u)
        {
            frameOffsets[frameIndex] = static_cast<unsigned int>(VidResourceTell(resource));
        }
        else
        {
            const long frameOffset = VidResourceTell(resource);
            const short linkedFrame = *reinterpret_cast<const short*>(mappedFileBase+frameOffset);
            frameOffsets[frameIndex] = frameOffsets[linkedFrame];
        }

        VidSkipResourceBytes(resource, static_cast<long>(frameBytes));

        if (VidResourceBytesToEndOfSubrecord(resource) >= 12)
        {
            if (!cadrLinkXYZ)
                cadrLinkXYZ = new(std::nothrow) VECTOR[static_cast<unsigned int>(static_cast<int>(noCadr))];
            VECTOR* const frameLink = cadrLinkXYZ+frameIndex;
            (void)resource->Read(&frameLink->x, 4u);
            (void)resource->Read(&frameLink->y, 4u);
            (void)resource->Read(&frameLink->z, 4u);
        }

        (void)resource->GoNextSub(ResourceType::Data);
    }

    static_cast<void>(VID::MemoryInUse = VID::MemoryInUse+allocatedBytes);

    SetLayer();

    if ((property&0x20u) == 0u || (type&0x0008u) == 0u || (type&0x0001u) == 0u)
        return;

    minXGridZ = maxXGridZ = minYGridZ = maxYGridZ = 0.0f;

    VECTOR* const scratch = new(std::nothrow) VECTOR[4194304u];

    gridCadrShift = new(std::nothrow) int[static_cast<unsigned int>(static_cast<int>(noCadr))];

    short cells[256*256];
    for (int frameIndex = 0;frameIndex<static_cast<int>(noCadr);++frameIndex)
    {
        for (int i = 0;i<256*256;++i) cells[i] = kEmptySoftwareGridCell;
        gridCadrShift[frameIndex] = noGridZ;

        const unsigned char* const frame = mappedFileBase+frameOffsets[frameIndex];
        if ((type&0x0004u) != 0u) SoftwareRasterizeDepthFrame(frame, cells);
        else SoftwareRasterizeMaskFrame(frame, cells);

        const int rows = static_cast<int>(vidSizeY)/8;
        const int columns = static_cast<int>(vidSizeX)/8;
        for (int row = rows-1;row >= 0;--row)
        {
            for (int column = 0;column<columns;++column)
            {
                const short z = cells[row*256+column];
                if (z == kEmptySoftwareGridCell) continue;

                VECTOR& point = scratch[noGridZ++];
                point.x = static_cast<float>(column*8-static_cast<int>(vidSizeX)/2);
                point.y = static_cast<float>(row*8-static_cast<int>(vidSizeY)/2);
                point.z = static_cast<float>(z);
            }
        }
    }

    gridZ = new(std::nothrow) VECTOR[static_cast<unsigned int>(noGridZ)];
    for (int i = 0;i<noGridZ;++i)
    {
        gridZ[i] = scratch[i];
        if (gridZ[i].x<minXGridZ) minXGridZ = gridZ[i].x;
        if (gridZ[i].x>maxXGridZ) maxXGridZ = gridZ[i].x;
        if (gridZ[i].y<minYGridZ) minYGridZ = gridZ[i].y;
        if (gridZ[i].y>maxYGridZ) maxYGridZ = gridZ[i].y;
    }

    delete[] scratch;
}

void VID_LIGHT::Load(RESOURCE* resource)
{
    if (resource->GoNext(ResourceType::Data) != 0)
    {
        const STRING text("DATA");
        Error(E_LOAD, text, 0u);
    }

    vidSizeX = static_cast<short>(TruncateFloatToInt32(sizeXYZ.x));
    vidSizeY = static_cast<short>(TruncateFloatToInt32(sizeXYZ.y));
    buildSizeX = sizeXYZ.x;
    buildSizeY = sizeXYZ.y;

    lightDataBytes = static_cast<unsigned int>(resource->SubLoad(&lightData, 0));
    if (lightDataBytes == 0u)
    {
        const STRING text("cadr");
        Error(E_LOAD, text, 0u);
    }
    static_cast<void>(VID::MemoryInUse = VID::MemoryInUse+lightDataBytes);
}

void VID_LIGHT::SetLayer()
{
    layer = 7;
}

void VID_LIGHT::Draw(SPRITE* sprite)
{

    const GAMMA spriteGamma = sprite->GetGamma();
    GAMMA vidGamma;
    vidGamma.modulate = gammaRaw.modulate;
    vidGamma.add = gammaRaw.add;
    GAMMA finalGamma(vidGamma, spriteGamma);
    if ((property&0x800u) == 0u)
    {
        GAMMA graphGamma;
        graphGamma.modulate = Graph->gamma.modulate;
        graphGamma.add = Graph->gamma.add;
        finalGamma = GAMMA(graphGamma, finalGamma);
    }

    const int frameIndex = sprite->CurrentFrame();
    const unsigned int frameColor = reinterpret_cast<const unsigned int*>(lightData)[frameIndex];
    if ((runtimeFlags&kVidRuntimeHidden) != 0u || (frameColor&0x00FFFFFFu) == 0u)
        return;

    VECTOR drawScale;
    EX_SPRITE_DATA* const transformOwner = sprite->ExtraData();
    if (transformOwner)
        drawScale = transformOwner->scale;
    else
    {
        drawScale.x = 1.0f;
        drawScale.y = 1.0f;
        drawScale.z = 1.0f;
    }

    VECTOR2 halfSize;
    halfSize.x = sizeXYZ.x*0.5f*scaleXYZ.x*drawScale.x;
    halfSize.y = sizeXYZ.y*0.5f*scaleXYZ.y*drawScale.y;

    VECTOR position;
    position = sprite->Coor();

    if ((runtimeFlags&kVidAnimateScale) != 0u)
    {
        const WEAPON_RUNTIME* const curveOwner =
            weapon;
        const float phase = transformOwner->effectCurvePosition;
        const VECTOR curveScale = EvaluateVidHardwareCurve(curveOwner, curveOwner->Scale, phase);
        halfSize.x *= curveScale.x;
        halfSize.y *= curveScale.y;
    }

    if ((runtimeFlags&kVidAnimateOffset) != 0u)
    {
        const WEAPON_RUNTIME* const curveOwner =
            weapon;
        const float phase = transformOwner->effectCurvePosition;
        const VECTOR curveOffset = EvaluateVidHardwareCurve(curveOwner, curveOwner->Shift, phase);
        position.x += curveOffset.x;
        position.y += curveOffset.y;
        position.z += curveOffset.z;
    }

    if ((property&kVidApplyDirectionOffset) != 0u)
    {
        const unsigned int waveIndex = (::CurrentTime>>3u)&0xFFu;
        position.z += FSin[waveIndex]*topZ;
    }

    COLOR sourceColor;
    sourceColor.value = frameColor;
    const COLOR drawColor(finalGamma, sourceColor);
    Graph->DrawLight(position, halfSize, drawColor, sprite->Direction(), property&0x1000u, 0);
}

void VID_FONT::InvalidateDeviceObjects()
{
    if (fontBackend && (storageFlags&4u) != 0u)
        (void)fontBackend->OnLostDevice();
}

void VID_FONT::RestoreDeviceObjects()
{
    if (fontBackend && (storageFlags&4u) != 0u)
        (void)fontBackend->OnResetDevice();
}

void VID_FONT::SetLayer()
{
    layer = 11;
}

void VID_FONT::Draw(SPRITE* sprite)
{
    if (!fontBackend) return;

    const GAMMA gamma = sprite->GetGamma();
    unsigned int color = VidFontColor(gamma);

    VECTOR2 projected(sprite->Coor().x, sprite->Coor().y-sprite->Coor().z);
    VECTOR2 shifted = projected-Map->ShiftXY();
    const int x = TruncateFloatToInt32(shifted.x);
    projected.x = sprite->Coor().x;
    projected.y = sprite->Coor().y-sprite->Coor().z;
    shifted = projected-Map->ShiftXY();
    int y = TruncateFloatToInt32(shifted.y);

    if ((property&kVidApplyDirectionOffset) != 0u)
    {
        const unsigned int index = (::CurrentTime>>3)&0xFFu;
        const float wave = FSin[index];
        y = BitsToSigned32(Signed32ToBits(y)-Signed32ToBits(TruncateFloatToInt32(wave*topZ)));
    }

    if (sprite == Mouse)
        y = BitsToSigned32(Signed32ToBits(y)-Signed32ToBits(static_cast<int>(vidSizeY)/2));
    else
    {
        const unsigned int behavior = sprite->Behave();
        if ((behavior&0x08u) != 0u)
            y = BitsToSigned32(Signed32ToBits(y)-Signed32ToBits(static_cast<int>(vidSizeY)/2));
        else if ((behavior&0x04u) != 0u)
            y = BitsToSigned32(Signed32ToBits(y)-Signed32ToBits(static_cast<int>(vidSizeY)));
    }

    RECT rect = {x, y, BitsToSigned32(Signed32ToBits(x)+1u), BitsToSigned32(Signed32ToBits(y)+1u)};
    GRAPH* const video = Graph;
    if (static_cast<double>(x)>video->screenWidth || static_cast<double>(y)>video->screenHeight) return;
    if (BitsToSigned32(Signed32ToBits(x)+6u*Signed32ToBits(static_cast<int>(vidSizeX))+10u)<0 ||
       BitsToSigned32(Signed32ToBits(y)+Signed32ToBits(static_cast<int>(vidSizeY))+10u)<0) return;
    if ((color&0x00FFFFFFu) == 0u) color = 0xFFFFFFFFu;

    IDirect3DDevice9* const device = video->device;
    (void)device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
    (void)Graph->device->SetRenderState(D3DRS_ZWRITEENABLE, 0u);
    (void)fontBackend->DrawTextA(
        "abcABC", 6, &rect, (DT_NOCLIP|DT_SINGLELINE|DT_NOPREFIX), color);
}

void VID_SURFACE::Load(RESOURCE* resource)
{

    if (nextMirror == this)
    {
        VidReleaseFrameArray(*this);
        surfaceData = 0;
    }

    (void)resource->Read(&pixelFormat, sizeof(pixelFormat));
    (void)resource->Read(&surfaceMode, sizeof(surfaceMode));
    if ((surfaceMode == 0u || surfaceMode == 0xFFFFFFFFu) &&
        std::strstr(vidName.CharPtr(), "\\101.") == 0)
        surfaceMode = 1u;
    if (VidResourceBytesToEndOfSubrecord(resource) != 0)
        (void)resource->Read(&renderMode, sizeof(renderMode));

    storageFlags |= STORAGE_LEGACY_SURFACE_HEADER;

    if (pixelFormat == D3DFMT_UNKNOWN) return;

    const int frameCount = static_cast<int>(noCadr);
    frames = new(std::nothrow) CADR_DATA[frameCount];

    unsigned int cachedOffset;

    struct CachedSurfaceHeader
    {
        unsigned int key0;
        unsigned int key1;
    };
    if (VidResourceBytesToEndOfSubrecord(resource) != 0)
    {
        CachedSurfaceHeader requested;
        (void)resource->Read(&cachedOffset, sizeof(cachedOffset));
        (void)resource->Read(&requested, sizeof(requested));
        if (VidResourceBytesToEndOfSubrecord(resource) != 0)
        {
            int headerDirections = 0;
            (void)resource->Read(&headerDirections, sizeof(headerDirections));
            if (headerDirections > 0 && noDir == 0)
                noDir = headerDirections;
            (void)resource->Read(&extendedHeaderValue, sizeof(extendedHeaderValue));
            VidSkipResourceBytes(resource, 128);
            VidSkipResourceBytes(resource, 128);
            VidSkipResourceBytes(resource, 128);
            storageFlags &= ~STORAGE_LEGACY_SURFACE_HEADER;
        }
        const CachedSurfaceHeader* const available =
            reinterpret_cast<const CachedSurfaceHeader*>(Map->m_fastLoadIndex);

        if (available && requested.key0 == available->key0 && requested.key1 == available->key1)
        {
            storageFlags |= STORAGE_FAST_LOAD;
            type = static_cast<unsigned short>(type&0x5FFBu);
            surfaceData = Map->m_fastLoadData;

            const unsigned char* cursor =
                Map->m_fastLoadIndex+cachedOffset;
            for (int i = 0;i<static_cast<int>(noCadr);++i)
            {
                CADR_DATA& frame = frames[i];
                VidReadCachedField(cursor, frame.indexCount);
                if ((type&0x0800u) != 0u)
                    VidReadCachedField(cursor, frame.extraDataCount);
                VidReadCachedField(cursor, frame.vertexCount);
                VidReadCachedField(cursor, frame.width);
                VidReadCachedField(cursor, frame.height);
                VidReadCachedField(cursor, frame.offsetX);
                VidReadCachedField(cursor, frame.offsetY);
                VidReadCachedField(cursor, frame.frameDataOffset);
                VidReadCachedField(cursor, frame.minZ);
                VidReadCachedField(cursor, frame.maxZ);
                if (i != 0)
                {
                    frame.firstIndex = frames[i-1].firstIndex+frames[i-1].indexCount;
                    frame.baseVertex = frames[i-1].baseVertex+frames[i-1].vertexCount;
                }
            }

            minXGridZ = *reinterpret_cast<const float*>(cursor);
            cursor+=4;
            CADR_DATA* const firstFrameRecord = frames;
            maxXGridZ = *reinterpret_cast<const float*>(cursor);
            cursor+=4;
            minYGridZ = *reinterpret_cast<const float*>(cursor);
            cursor+=4;
            maxYGridZ = *reinterpret_cast<const float*>(cursor);
            cursor+=4;

            unsigned char* const firstFrame = surfaceData+firstFrameRecord[0].frameDataOffset;
            const unsigned int packedGridCount =
                *reinterpret_cast<const unsigned int*>(firstFrame-sizeof(unsigned int));
            unsigned int prefixBytes = sizeof(unsigned int);
            if (packedGridCount != 0u)
                prefixBytes = (packedGridCount*3u+static_cast<unsigned int>(static_cast<int>(noCadr)))*4u+4u;

            const unsigned short cachedType = type;
            if ((cachedType&0x0008u) != 0u)
            {
                prefixBytes+=1024u;
                palette = reinterpret_cast<unsigned int*>(firstFrame-prefixBytes);
            }

            if ((cachedType&0x0200u) != 0u)
            {
                unsigned char* linkBase = firstFrame-prefixBytes;
                linkBase-=static_cast<unsigned int>(static_cast<int>(noCadr))*sizeof(VECTOR);
                cadrLinkXYZ = reinterpret_cast<VECTOR*>(linkBase);
            }
            return;
        }
    }
    else if (VidResourceBytesToEndOfSubrecord(resource) == 0)
    {
        VidReportResourceError(this, E_ERROR,
            "old format vid. Can't create fastLoad", 0);
    }

    {
        STRING fileName = resource->GetFileName();
        surfaceData = static_cast<unsigned char*>(FFileMapping(fileName));
    }
    if (!surfaceData)
    {
        VidReportResourceError(this, E_CREATE, "file mapping", 0);
        return;
    }

    if ((type&0x0008u) != 0u)
    {
        if (resource->GoNext(ResourceType::Palette) == 0)
        {
            const long paletteOffset = VidResourceTell(resource);
            palette = reinterpret_cast<unsigned int*>(surfaceData+paletteOffset);
        }
        else
        {
            VidReportResourceError(this, E_LOAD, "PAL ", 0);
            return;
        }
    }

    (void)resource->GoNext(ResourceType::Any);

    if ((type&0x0200u) != 0u)
    {
        const int linkCount = static_cast<int>(noCadr);
        cadrLinkXYZ = new(std::nothrow) VECTOR[static_cast<unsigned int>(linkCount)];
    }

    if (resource->RecordType() != ResourceType::Frame)
        (void)resource->GoNext(ResourceType::Frame);
    if (resource->RecordType() != ResourceType::Frame)
    {
        VidReportResourceError(this, E_LOAD, "CADR", 0);
        goto surfaceFrameBounds;
    }

    for (int frameIndex = 0;frameIndex<static_cast<int>(noCadr);++frameIndex)
    {
        if (VidResourceBytesToEndOfSubrecord(resource)>4)
        {
            if ((type&0x0200u) != 0u)
            {
                VECTOR* const frameLink = cadrLinkXYZ+frameIndex;
                (void)resource->Read(&frameLink->x, 4u);
                (void)resource->Read(&frameLink->y, 4u);
                (void)resource->Read(&frameLink->z, 4u);
            }

            (void)resource->Read(&frames[frameIndex].indexCount, 2u);
            if ((type&0x0800u) != 0u)
                (void)resource->Read(&frames[frameIndex].extraDataCount, 2u);
            (void)resource->Read(&frames[frameIndex].vertexCount, 2u);
            (void)resource->Read(&frames[frameIndex].width, 2u);
            (void)resource->Read(&frames[frameIndex].height, 2u);
            (void)resource->Read(&frames[frameIndex].offsetX, 2u);
            (void)resource->Read(&frames[frameIndex].offsetY, 2u);
            const unsigned int streamPosition = static_cast<unsigned int>(VidResourceTell(resource));
            frames[frameIndex].frameDataOffset = streamPosition;
            if (frameIndex != 0)
            {
                frames[frameIndex].firstIndex = frames[frameIndex-1].firstIndex+frames[frameIndex-1].indexCount;
                frames[frameIndex].baseVertex = frames[frameIndex-1].baseVertex+frames[frameIndex-1].vertexCount;
            }
        }
        else
        {
            const long linkOffset = VidResourceTell(resource);
            const int linkedIndex = *reinterpret_cast<const int*>(surfaceData+linkOffset);
            frames[frameIndex] = frames[linkedIndex];
        }

        if (frameIndex<static_cast<int>(noCadr)-1)
            (void)resource->GoNextSub(ResourceType::Frame);
    }

surfaceFrameBounds:
    if ((property&0x1000u) != 0u && frames)
        frames[0].CalcMaxMinZ(this);
}

void VID_SURFACE::SetReColorForArmy(COLOR color)
{
    if ((type&8u) == 0u)
    {
        const STRING text("SetReColorForArmy for non paletted vid");
        Error(E_ERROR, text, 0u);
        return;
    }

    if (!recolorPalette)
        recolorPalette = new(std::nothrow) unsigned int[256u];

    for (unsigned int i = 0;i<256u;++i)
    {
        const unsigned int sourceColor = palette[i];
        const int red = static_cast<int>((sourceColor>>16u)&0xFFu);
        const int green = static_cast<int>((sourceColor>>8u)&0xFFu);
        const int blue = static_cast<int>(sourceColor&0xFFu);
        const float ramp = static_cast<float>(red)*(1.0f/148.0f);

        if (red>20 &&
           green <= static_cast<int>(ramp*80.0f) &&
           (red >= blue?red-blue:blue-red) <= static_cast<int>(ramp*30.0f))
        {
            const float recolorScale = static_cast<float>(red)*(1.0f/128.0f);
            const int scaledBlue = static_cast<int>(static_cast<float>(color.value&0xFFu)*recolorScale);
            const int scaledGreen = static_cast<int>(static_cast<float>((color.value>>8u)&0xFFu)*recolorScale);
            const int scaledRed = static_cast<int>(static_cast<float>((color.value>>16u)&0xFFu)*recolorScale);
            const unsigned int tint = 0xFF000000u|
                (static_cast<unsigned int>(ClampByteInt(scaledRed))<<16u)|
                (static_cast<unsigned int>(ClampByteInt(scaledGreen))<<8u)|
                static_cast<unsigned int>(ClampByteInt(scaledBlue));

            const unsigned int gray = static_cast<unsigned int>(green);
            const unsigned int addBlue = (gray+(tint&0xFFu)>0xFFu)?0xFFu:gray+(tint&0xFFu);
            const unsigned int addGreen = ((gray<<8u)+(tint&0xFF00u)>0xFF00u)?0xFF00u:(gray<<8u)+(tint&0xFF00u);
            const unsigned int addRed = ((gray<<16u)+(tint&0xFF0000u)>0xFF0000u)?0xFF0000u:(gray<<16u)+(tint&0xFF0000u);
            recolorPalette[i] = 0xFF000000u|addRed|addGreen|addBlue;
        }
        else
        {
            recolorPalette[i] = sourceColor;
        }
    }

    ReleaseVideoMemory();

    if (nextMirror != this)
    {
        VID* predecessor = nextMirror;
        while (predecessor->nextMirror != this) predecessor = predecessor->nextMirror;
        predecessor->nextMirror = nextMirror;

        CADR_DATA* const oldFrames = frames;
        const int frameCount = static_cast<int>(noCadr);
        nextMirror = this;
        frames = new(std::nothrow) CADR_DATA[frameCount];
        for (int i = 0;i<frameCount;++i)
            frames[i] = oldFrames[i];

        if (cadrLinkXYZ && (storageFlags&8u) == 0u)
        {
            VECTOR* const oldLinks = cadrLinkXYZ;
            cadrLinkXYZ = new(std::nothrow) VECTOR[static_cast<unsigned int>(frameCount)];
            for (int i = 0;i<frameCount;++i) cadrLinkXYZ[i] = oldLinks[i];
        }
        return;
    }

}

void VID_SURFACE::InvalidateDeviceObjects()
{
    if ((storageFlags&4u) != 0u && surfaceBackend)
    {
        (void)surfaceBackend->Release();
        surfaceBackend = 0;
    }
    if (textureAtlas) textureAtlas->ReleaseVideoMemory();
    textureAtlas = 0;
    ReleaseVideoMemory();
}

CADR_DATA* VID_SURFACE::PrepareDraw(const SPRITE* request, RECT* captureRect)
{
    VID_SURFACE* const owner = this;
    const int currentFrame = request->CurrentFrame();
    if (currentFrame >= owner->noCadr)
    {
        request->Error(E_INVALID, STRING("currentCadr >= noCadr"), currentFrame);
        return 0;
    }
    int frameIndex = currentFrame;
    if ((owner->property&0x20000000u) != 0u)
    {
        const int animation = request->m_currentAnimation;
        frameIndex += owner->animationBaseFrame[animation]+owner->animationFrameCount[animation]*
            owner->RealDirectionWithGraphDetail(request->Direction().value)-request->m_currentFrameBegin;
    }
    CADR_DATA* const frame = owner->frames+frameIndex;
    if (frame->height == 0u) return 0;
    VidHardwareDrawTransform transform;
    transform.scale = request->ExtraData() ? request->ExtraData()->scale : VECTOR(1.0f, 1.0f, 1.0f);
    transform.scale.x *= owner->scaleXYZ.x;
    transform.scale.y *= owner->scaleXYZ.y;
    transform.scale.z *= owner->scaleXYZ.z;
    transform.position = request->Coor();
    if ((owner->property&0x200000u) != 0u)
    {
        const EX_SPRITE_DATA* source = request->ExtraData();
        const float distance = (request->Coor().ViewCoor()-source->cachedPosition.ViewCoor()).NearLength();
        const float ratio = distance/static_cast<short>(owner->frames[currentFrame-request->m_currentFrameBegin].height);
        if (ratio > 1.0f)
        {
            transform.position = (request->Coor()+source->cachedPosition)*0.5f;
            transform.scale.y = ratio;
        }
    }
    if ((owner->property&0x8000u) != 0u)
    {
        transform.position.y += 1000.0f;
        transform.position.z += 1000.0f;
    }
    if ((owner->property&0x10000u) != 0u)
        transform.position.z += FSin[(::CurrentTime>>3u)&255u]*owner->topZ;
    if ((owner->runtimeFlags&kVidAnimateScale) != 0u)
    {
        const VECTOR scale = EvaluateVidHardwareCurve(owner->weapon, owner->weapon->Scale,
            request->ExtraData()->effectCurvePosition);
        transform.scale.x *= scale.x;
        transform.scale.y *= scale.y;
        transform.scale.z *= scale.z;
    }
    if ((owner->runtimeFlags&kVidAnimateOffset) != 0u)
        transform.position += EvaluateVidHardwareCurve(owner->weapon, owner->weapon->Shift,
            request->ExtraData()->effectCurvePosition);
    const VECTOR2 extent(owner->buildSizeX*0.5f*transform.scale.x*1.29999995f,
                         owner->buildSizeY*0.5f*transform.scale.y*1.29999995f);
    const VECTOR2 point = transform.position.ViewCoor();
    if (point.x+extent.x < VID::viewMin.x || point.y+extent.y < VID::viewMin.y ||
        point.x-extent.x > VID::viewMax.x || point.y-extent.y > VID::viewMax.y) return 0;
    if ((owner->property&0x1000u) != 0u && owner->renderMode == 4u)
        Graph->DrawShadowToStencil(request->Coor(), extent);
    unsigned char direction = SelectVidHardwareDrawDirection(owner, request, request->m_speed, request->m_zSpeed);
    if (!owner->mesh) owner->mesh = new(std::nothrow) MESH();
    if (!owner->mesh->VertexBuffer())
    {
        if (owner->frames[0].indexCount != 0 && owner->surfaceMode == 3)
            owner->mesh->CreateFromVid(owner);
        else
            owner->mesh->CreateUnitQuad();
    }
    frame->SetTextureToDevice(owner);
    if (captureRect && owner->surfaceMode == 6)
    {
        VECTOR2 halfSize(static_cast<short>(frame->width)*transform.scale.x*1.29999995f*0.5f,
                         static_cast<short>(frame->height)*transform.scale.y*1.29999995f*0.5f);
        if (halfSize.x > 128.0f) halfSize.x = 128.0f;
        if (halfSize.y > 128.0f) halfSize.y = 128.0f;
        const VECTOR2 centre = transform.position.ViewCoor()-Map->ShiftXY();
        captureRect->left = TruncateFloatToInt32(centre.x-halfSize.x);
        captureRect->top = TruncateFloatToInt32(centre.y-halfSize.y);
        captureRect->right = TruncateFloatToInt32(centre.x+halfSize.x);
        captureRect->bottom = TruncateFloatToInt32(centre.y+halfSize.y);
    }
    transform.position.x -= Map->ShiftXY().x;
    transform.position.y = (transform.position.y-Map->ShiftXY().y)/0.707060218f;
    if (owner->surfaceMode == 0 || (owner->surfaceMode == -1 && owner->sizeXYZ.z > owner->sizeXYZ.y))
        direction = 0;
    if (owner->renderMode == 4u &&
        (Graph->lightRenderTexture->SizeX() != Graph->screenWidth ||
         Graph->lightRenderTexture->SizeY() != Graph->screenHeight))
    {
        const VECTOR2 ratio(static_cast<float>(Graph->lightRenderTexture->SizeX())/Graph->screenWidth,
                            static_cast<float>(Graph->lightRenderTexture->SizeY())/Graph->screenHeight);
        transform.scale.x *= ratio.x;
        transform.scale.y *= ratio.y;
        transform.position.x *= ratio.x;
        transform.position.y *= ratio.y;
        transform.position.z *= ratio.y;
    }
    transform.position.x -= (Graph->viewLeft+Graph->viewRight)*0.5f;
    transform.position.y -= (Graph->viewTop+Graph->viewBottom)/1.41412044f;
    const ANGLE rotation(direction);
    D3DXMATRIX world;
    memset(&world, 0, sizeof(world));
    world._11 = rotation.Cos()*transform.scale.x;
    world._12 = rotation.Sin()*transform.scale.x;
    world._21 = -rotation.Sin()*transform.scale.y;
    world._22 = rotation.Cos()*transform.scale.y;
    world._33 = transform.scale.z;
    world._41 = transform.position.x;
    world._42 = transform.position.y;
    world._43 = transform.position.z;
    world._44 = 1.0f;
    Graph->SetTransformMatrix(D3DTS_WORLD, &world);
    unsigned int filtering = D3DTEXF_POINT;
    if (((owner->type&2u) != 0u || (owner->renderMode != 0u && owner->renderMode != 6u)) &&
        (transform.scale.x != 1.0f || transform.scale.y != 1.0f || transform.scale.z != 1.0f))
        filtering = D3DTEXF_LINEAR;
    Graph->device->SetSamplerState(0, D3DSAMP_MINFILTER, filtering);
    Graph->device->SetSamplerState(0, D3DSAMP_MAGFILTER, filtering);
    return frame;
}

namespace
{
static GAMMA SurfaceDrawGamma(VID_SURFACE* owner, const SPRITE* sprite)
{
    GAMMA result(owner->gammaRaw, sprite->GetGamma());
    if (owner->layer > 6 && (owner->property&0x800u) == 0u)
        result = GAMMA(Graph->gamma, result);
    return result;
}

static void SetSurfaceShaderGamma(const GAMMA& gamma)
{
    const unsigned int diffuse = ~gamma.modulate;
    const unsigned int specular = gamma.add&0x00FFFFFFu;
    const float scale = 0.00392156886f;
    const D3DXVECTOR4 modulate(static_cast<float>((diffuse>>16)&255u)*scale,
        static_cast<float>((diffuse>>8)&255u)*scale, static_cast<float>(diffuse&255u)*scale,
        static_cast<float>(diffuse>>24)*scale);
    const D3DXVECTOR4 add(static_cast<float>((specular>>16)&255u)*scale,
        static_cast<float>((specular>>8)&255u)*scale, static_cast<float>(specular&255u)*scale, 0.0f);
    Graph->pseudo3DEffect->SetVector(Graph->diffuseParameter, &modulate);
    Graph->pseudo3DEffect->SetVector(Graph->specularParameter, &add);
}

static void SetSurfaceTextureCoordinates(const VID_SURFACE* owner, const SPRITE* request,
                                        const CADR_DATA* frame, bool normalPlane)
{
    VECTOR2 shift(static_cast<float>(owner->vidSizeX/2-static_cast<short>(frame->offsetX))+0.5f,
                  static_cast<float>(owner->vidSizeY/2-static_cast<short>(frame->offsetY))+0.5f);
    if (request && owner->spriteClass == 19u)
    {
        shift.x += static_cast<float>(request->CurrentFrame()%16)*(static_cast<short>(frame->width)+1.0f);
        shift.y += static_cast<float>(request->CurrentFrame()/16)*(static_cast<short>(frame->height)+1.0f);
    }
    if (normalPlane) shift.y += static_cast<short>(frame->height);
    const VECTOR2 size(static_cast<float>(frame->textureObject->SizeX()),
                       static_cast<float>(frame->textureObject->SizeY()));
    Graph->pseudo3DEffect->SetValue(Graph->texShiftParameter, &shift, sizeof(shift));
    Graph->pseudo3DEffect->SetValue(Graph->texSizeParameter, &size, sizeof(size));
}

static void DrawSurfaceGeometry(VID_SURFACE* owner, const CADR_DATA* frame)
{
    if (owner->surfaceMode == 3)
    {
        owner->mesh->Draw(frame->baseVertex, frame->vertexCount, frame->firstIndex, frame->indexCount);
        return;
    }
    const float x0 = static_cast<float>(static_cast<short>(frame->offsetX)-owner->vidSizeX/2);
    const float x1 = x0+static_cast<short>(frame->width);
    const float y0 = static_cast<float>(static_cast<short>(frame->offsetY)-owner->vidSizeY/2)/0.707060218f;
    const float y1 = static_cast<float>(static_cast<short>(frame->offsetY)-owner->vidSizeY/2+
        static_cast<short>(frame->height))/0.707060218f;
    VECTOR vertices[4];
    if (owner->surfaceMode == 0)
    {
        vertices[0] = VECTOR(x0,y1,y1-y0);
        vertices[1] = VECTOR(x1,y1,y1-y0);
        vertices[2] = VECTOR(x0,y1,0.0f);
        vertices[3] = VECTOR(x1,y1,0.0f);
    }
    else
    {
        vertices[0] = VECTOR(x0,y0,0.0f);
        vertices[1] = VECTOR(x1,y0,0.0f);
        vertices[2] = VECTOR(x0,y1,0.0f);
        vertices[3] = VECTOR(x1,y1,0.0f);
    }
    Graph->device->SetFVF(D3DFVF_XYZ);
    unsigned int passes;
    Graph->pseudo3DEffect->Begin(&passes, 0);
    for (unsigned int pass = 0; pass < passes; ++pass)
    {
        Graph->pseudo3DEffect->Pass(pass);
        Graph->device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(VECTOR));
    }
    Graph->pseudo3DEffect->End();
    MESH::noTri += 2;
    MESH::noVertex += 4;
}

static void SetSurfaceTechnique(const VID_SURFACE* owner, const SPRITE* sprite,
                                const CADR_DATA* frame, const GAMMA& gamma)
{
    if (Graph->vertexShaderVersion < 0x100u)
    {
        Graph->pseudo3DEffect->SetTechnique(Graph->drawObjectWithoutShaderTechnique);
        Graph->device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
        Graph->device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
        Graph->device->SetRenderState(D3DRS_TEXTUREFACTOR, ~gamma.modulate);
        Graph->device->SetRenderState(D3DRS_SPECULARENABLE, 0);
    }
    else
    {
        Graph->pseudo3DEffect->SetTechnique(Graph->drawObjectTechnique);
        Graph->device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        Graph->device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        Graph->device->SetRenderState(D3DRS_SPECULARENABLE, gamma.add != 0u);
        SetSurfaceTextureCoordinates(owner, sprite, frame, false);
        SetSurfaceShaderGamma(gamma);
    }
}
}

void VID_SURFACE::Draw(SPRITE* sprite)
{
    ++noDrawCall[layer];
    if (!frames || (runtimeFlags&kVidRuntimeHidden) != 0u) return;
    RECT captureRect;
    CADR_DATA* const frame = PrepareDraw(sprite, &captureRect);
    if (!frame) return;
    GAMMA finalGamma = SurfaceDrawGamma(this, sprite);
    if (sprite->IsSpriteClass(2u) || (sprite->Vid()->weapon->Property&0x40u) != 0u)
    {
        SPRITE* const blocker = sprite->CanPlace(sprite->Coor());
        if (blocker == Mouse) finalGamma = GAMMA(-190, -190, 255);
        else if (blocker) finalGamma = GAMMA(255, -190, -190);
        if (sprite->IsSpriteClass(2u) && TruncateFloatToInt32(sprite->Z())%90 > 5 &&
            TruncateFloatToInt32(sprite->Z())%90 < 85) finalGamma.SetGreen(255);
    }
    Graph->device->SetRenderState(D3DRS_ALPHATESTENABLE, 1);
    Graph->device->SetRenderState(D3DRS_ALPHAREF, 0);
    Graph->device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
    Graph->device->SetRenderState(D3DRS_ZFUNC,
        (renderMode == 7u || renderMode == 4u || surfaceMode == 6) ? D3DCMP_ALWAYS : D3DCMP_GREATEREQUAL);
    if ((type&2u) == 0u && (renderMode == 0u || renderMode == 6u))
    {
        Graph->device->SetRenderState(D3DRS_ALPHABLENDENABLE, 0);
        Graph->device->SetRenderState(D3DRS_ZWRITEENABLE, renderMode != 6u);
        Graph->device->SetRenderState(D3DRS_COLORWRITEENABLE, Graph->graphDetail >= 8u ? 15u : 7u);
    }
    else
    {
        if (renderMode == 1u) Graph->SetAlphaBlend(D3DBLEND_DESTCOLOR, D3DBLEND_SRCCOLOR);
        else if (renderMode == 2u || renderMode == 4u) Graph->SetAlphaBlend(D3DBLEND_ONE, D3DBLEND_ONE);
        else if (renderMode == 5u) Graph->SetAlphaBlend(D3DBLEND_DESTCOLOR, D3DBLEND_ONE);
        else if ((renderMode == 0u || renderMode == 7u || renderMode == 6u) &&
                 pixelFormat != D3DFMT_A1R5G5B5 && (type&2u) != 0u)
            Graph->SetAlphaBlend(D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
        else if (renderMode == 0u || renderMode == 7u || renderMode == 6u)
            Graph->device->SetRenderState(D3DRS_ALPHABLENDENABLE, 0);
        Graph->device->SetRenderState(D3DRS_ZWRITEENABLE, renderMode == 7u);
        Graph->device->SetRenderState(D3DRS_COLORWRITEENABLE, 7);
    }
    SetSurfaceTechnique(this, sprite, frame, finalGamma);
    if (surfaceMode == 6)
    {
        if (captureRect.right > Graph->screenWidth) captureRect.right = TruncateFloatToInt32(Graph->screenWidth);
        if (captureRect.bottom > Graph->screenHeight) captureRect.bottom = TruncateFloatToInt32(Graph->screenHeight);
        const RECT destination = {0,0,captureRect.right-captureRect.left,captureRect.bottom-captureRect.top};
        const HRESULT result = Graph->device->StretchRect(Graph->renderTarget, &captureRect,
            Graph->distortionTexture->GetSurface(), &destination, D3DTEXF_NONE);
        if (result < 0) Error(E_ERROR, STRING("StretchRect"), result);
        Graph->pseudo3DEffect->SetTechnique("DrawDistortion");
        Graph->distortionTexture->SetTextureToDevice(1);
        Graph->device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_BUMPENVMAP);
        Graph->device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        Graph->device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
        Graph->device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        Graph->device->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    }
    DrawSurfaceGeometry(this, frame);
    if (surfaceMode == 6)
    {
        Graph->device->SetTexture(1, 0);
        Graph->device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    }
    if ((property&0x1000u) != 0u && renderMode == 4u)
        Graph->device->SetRenderState(D3DRS_STENCILENABLE, 0);
    lastRenderUseTick = ::RealCurrentTime;
    if (spriteClass == 8u) ++::NoBuildAlpha;
    ++noObj;
    ++noDrawReal[layer];
}

void VID_SURFACE::DrawMesh(SPRITE* sprite)
{
    ++noDrawCall[layer];
    if (!frames || (runtimeFlags&kVidRuntimeHidden) != 0u || (renderMode != 0u && renderMode != 6u)) return;
    CADR_DATA* const frame = PrepareDraw(sprite, 0);
    if (!frame) return;
    SetSurfaceTechnique(this, sprite, frame, SurfaceDrawGamma(this, sprite));
    Graph->device->SetRenderState(D3DRS_COLORWRITEENABLE, 7);
    Graph->device->SetRenderState(D3DRS_ZFUNC, renderMode == 7u ? D3DCMP_ALWAYS : D3DCMP_GREATEREQUAL);
    Graph->device->SetRenderState(D3DRS_ZWRITEENABLE, 1);
    Graph->device->SetRenderState(D3DRS_ALPHABLENDENABLE, 0);
    Graph->device->SetRenderState(D3DRS_ALPHATESTENABLE, 0);
    Graph->device->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
    Graph->device->SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFFu);
    Graph->device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
    Graph->device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    DrawSurfaceGeometry(this, frame);
    lastRenderUseTick = ::RealCurrentTime;
    Graph->device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    Graph->device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
}

void VID_SURFACE::DrawOnlyZ(const SPRITE* sprite)
{
    const bool flat = surfaceMode == 1 || (surfaceMode == -1 && sizeXYZ.y >= sizeXYZ.z);
    if ((type&0x2000u) == 0u && sprite->Z() == 0.0f && flat) return;
    if (!frames || (runtimeFlags&kVidRuntimeHidden) != 0u) return;
    CADR_DATA* const frame = PrepareDraw(sprite, 0);
    if (!frame) return;
    unsigned int color = 0xFFFFFFFFu;
    if (frame->indexCount == 0u && (type&0x2000u) == 0u && flat) color = 0xFF78C0FFu;
    SetSurfaceTextureCoordinates(this, 0, frame, (type&0x2000u) != 0u);
    const float scale = 0.00392156886f;
    const D3DXVECTOR4 diffuse(static_cast<float>((color>>16)&255u)*scale,
        static_cast<float>((color>>8)&255u)*scale, static_cast<float>(color&255u)*scale,
        static_cast<float>(color>>24)*scale);
    Graph->pseudo3DEffect->SetVector(Graph->diffuseParameter, &diffuse);
    Graph->device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    Graph->device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    DrawSurfaceGeometry(this, frame);
}

void VID_SURFACE::DrawOpaque(SPRITE* sprite)
{
    if (!frames || (runtimeFlags&kVidRuntimeHidden) != 0u || (type&2u) != 0u || layer >= 4 || renderMode != 0u) return;
    CADR_DATA* const frame = PrepareDraw(sprite, 0);
    if (!frame) return;
    D3DXHANDLE technique = Graph->drawObjectWithoutShaderTechnique;
    if (Graph->vertexShaderVersion >= 0x100u)
    {
        technique = Graph->graphDetail >= 8u && (Map->Flags()&MAP::FLAG_FORCE_NO_Z) == 0u ?
            Graph->drawObjectWithZTechnique : Graph->drawObjectTechnique;
        SetSurfaceTextureCoordinates(this, 0, frame, false);
    }
    Graph->pseudo3DEffect->SetTechnique(technique);
    DrawSurfaceGeometry(this, frame);
    lastRenderUseTick = ::RealCurrentTime;
    ++noObj;
}

void VID_SURFACE::DrawOccluded(SPRITE* sprite)
{
    ++noDrawCall[layer];
    if (!frames || (runtimeFlags&kVidRuntimeHidden) != 0u ||
        (renderMode != 0u && renderMode != 6u) || (type&2u) != 0u) return;
    CADR_DATA* const frame = PrepareDraw(sprite, 0);
    if (!frame) return;
    const GAMMA gamma = SurfaceDrawGamma(this, sprite);
    SetSurfaceTechnique(this, sprite, frame, gamma);
    Graph->device->SetRenderState(D3DRS_ALPHATESTENABLE, 1);
    Graph->device->SetRenderState(D3DRS_ALPHAREF, 0);
    Graph->device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
    Graph->device->SetRenderState(D3DRS_ALPHABLENDENABLE, 0);
    Graph->device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
    Graph->device->SetRenderState(D3DRS_ZWRITEENABLE, 0);
    Graph->device->SetRenderState(D3DRS_COLORWRITEENABLE, 7);
    Graph->device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
    Graph->device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
    Graph->SetAlphaBlend(D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
    Graph->device->SetRenderState(D3DRS_TEXTUREFACTOR, ((~gamma.modulate)&0x00FFFFFFu)|0x40000000u);
    mesh->Draw(frame->baseVertex, frame->vertexCount, frame->firstIndex, frame->indexCount);
    lastRenderUseTick = ::RealCurrentTime;
    Graph->device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    Graph->device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
}

int VID_SURFACE::GetSizeTextureData(int frameIndex) const
{
    if (!frames || static_cast<short>(frames[frameIndex].height) == 0) return 0;
    const CADR_DATA& frame = frames[frameIndex];
    const unsigned char* const begin = surfaceData+frame.frameDataOffset+
        2u*frame.indexCount+4u*(frame.vertexCount+frame.extraDataCount);
    int formatBits = 0;
    switch (pixelFormat)
    {
    case D3DFMT_R8G8B8: formatBits = 24; break;
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8: formatBits = 32; break;
    case D3DFMT_R5G6B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_A4R4G4B4: formatBits = 16; break;
    case D3DFMT_P8: formatBits = 8; break;
    case D3DFMT_DXT1: formatBits = 4; break;
    case D3DFMT_DXT3:
    case D3DFMT_DXT5: formatBits = 8; break;
    default: break;
    }
    if (pixelFormat == D3DFMT_DXT1 || pixelFormat == D3DFMT_DXT3 || pixelFormat == D3DFMT_DXT5)
    {
        const unsigned int product = static_cast<unsigned int>(static_cast<int>(static_cast<short>(frame.width)))*
            static_cast<unsigned int>(static_cast<int>(static_cast<short>(frame.height)))*
            static_cast<unsigned int>(formatBits);
        return BitsToSigned32(product)/8;
    }
    int stride = formatBits/8;
    if ((type&0x8000u) != 0u) ++stride;
    if ((type&0x2000u) != 0u) ++stride;
    if ((type&4u) != 0u) stride += 2;
    const unsigned char* data = begin;
    while (*reinterpret_cast<const unsigned short*>(data) != 0u)
    {
        ++data;
        const unsigned int count = *data++;
        data += count*stride;
    }
    data += 2;
    return static_cast<int>(data-begin);
}

float VID_SURFACE::GetZInCadr(int frameIndex, const VECTOR2& point, COLOR* color,
    VECTOR* normal, unsigned int* detail) const
{
    if (color) color->value = 0;
    const CADR_DATA& frame = frames[frameIndex];
    if (static_cast<short>(frame.height) == 0 || pixelFormat == D3DFMT_DXT1 ||
        pixelFormat == D3DFMT_DXT3 || pixelFormat == D3DFMT_DXT5)
        return -999999.0f;
    int width = static_cast<short>(frame.width);
    if ((type&0x10u) != 0u)
    {
        int roundedWidth = 4;
        while (width>roundedWidth) roundedWidth += roundedWidth;
        width = roundedWidth;
    }
    const int pixelBytes = VidPixelFormatBits(pixelFormat)/8;
    const unsigned char* data = surfaceData+frame.frameDataOffset+
        2u*frame.indexCount+4u*(frame.vertexCount+frame.extraDataCount);
    int pixelPosition = 0;
    unsigned char detailValue = 0;
    unsigned char normalValue = 0;
    while (*reinterpret_cast<const unsigned short*>(data) != 0u)
    {
        pixelPosition += *data++;
        const unsigned int count = *data++;
        for (unsigned int i = 0; i<count; ++i)
        {
            ++pixelPosition;
            const unsigned int raw = *reinterpret_cast<const unsigned int*>(data);
            data += pixelBytes;
            if ((type&0x8000u) != 0u) detailValue = *data++;
            if ((type&0x2000u) != 0u) normalValue = *data++;
            const int x = pixelPosition%width+static_cast<short>(frame.offsetX)-vidSizeX/2;
            const int y = pixelPosition/width+static_cast<short>(frame.offsetY)-vidSizeY/2;
            if (x == TruncateFloatToInt32(point.x) && y == TruncateFloatToInt32(point.y))
            {
                if (color)
                {
                    switch (pixelFormat)
                    {
                    case D3DFMT_P8: color->value = palette[raw&255u]; break;
                    case D3DFMT_R8G8B8:
                    case D3DFMT_A8R8G8B8: color->value = raw; break;
                    case D3DFMT_A4R4G4B4:
                        color->value = COLOR((raw&0xF000u)>>8, (raw&0x0F00u)>>4,
                            raw&0x00F0u, (raw&0x000Fu)<<4).value;
                        break;
                    default: break;
                    }
                    if ((type&0x2000u) != 0u)
                        color->value = (color->value&0xFFFFFFu)|(static_cast<unsigned int>(normalValue)<<24u);
                }
                if ((type&0x8000u) != 0u && detail) *detail = detailValue;
                if ((type&0x2000u) != 0u && normal)
                {
                    const ANGLE azimuth((normalValue>>3u)*128/31+128);
                    const ANGLE elevation(99-((normalValue&7u)*4+(detailValue>>6u))*128/31);
                    normal->x = elevation.Sin()*azimuth.Cos();
                    normal->y = -elevation.Sin()*azimuth.Sin();
                    normal->z = elevation.Cos();
                }
                if ((type&4u) == 0u) return -999999.0f;
                return (static_cast<float>(*reinterpret_cast<const unsigned short*>(data))-1024.0f)*0.125f;
            }
            if ((type&4u) != 0u) data += 2;
        }
    }
    return -999999.0f;
}

namespace
{
    struct VidGridVertex
    {
        float x;
        float y;
        float z;
    };

    static int VidSurfacePixelBytes(unsigned int format)
    {

        switch (format)
        {
        case D3DFMT_R8G8B8:
                return 3;
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8:
                return 4;
        case D3DFMT_R5G6B5:
        case D3DFMT_X1R5G5B5:
        case D3DFMT_A1R5G5B5:
        case D3DFMT_A4R4G4B4:
                return 2;
        case D3DFMT_P8:
                return 1;
        case D3DFMT_DXT1:
                return 0;
        case D3DFMT_DXT3:
        case D3DFMT_DXT5:
                return 1;
        default:
                return 0;
        }
    }

    static int VidSurfaceRoundedTextureExtent(int value)
    {
        int rounded = 4;
        while (value>rounded) rounded += rounded;
        return rounded;
    }

    static VidGridVertex VidDecodeGridVertex(unsigned int packed)
    {
        VidGridVertex decodedVertex;
        decodedVertex.x = static_cast<float>(packed&0x3FFu);
        decodedVertex.y = static_cast<float>((packed>>10)&0x3FFu);
        const unsigned int z = (packed>>20)&0x3FFu;
        const unsigned int divisor = 1u<<(packed>>30);
        decodedVertex.z = static_cast<float>(z)/static_cast<float>(divisor)-32.0f;
        return decodedVertex;
    }

    static void VidSwapGridVertex(VidGridVertex& a, VidGridVertex& b)
    {
        const VidGridVertex t = a;
        a = b;
        b = t;
    }

    static void VidGridUpdateCell(short* cells, int x, int projectedY, int z)
    {

        short& cell = cells[(projectedY/8)*256+(x/8)];
        if (z>static_cast<int>(cell)) cell = static_cast<short>(z);
    }

    static void VidRasterizeGridTriangle(const CADR_DATA& frame,
                                         const unsigned char* pixelMask,
                                         int textureWidth,
                                         short* cells,
                                         VidGridVertex a,
                                         VidGridVertex b,
                                         VidGridVertex c)
    {

        if (a.y>b.y) VidSwapGridVertex(a, b);
        if (a.y>c.y) VidSwapGridVertex(a, c);
        if (b.y>c.y) VidSwapGridVertex(b, c);

        const int firstY = static_cast<int>(ceil(static_cast<double>(a.y)));
        const int floorLastY = static_cast<int>(floor(static_cast<double>(c.y)));
        if (floorLastY <= firstY) return;

        const float dyAC = c.y-a.y;
        const float splitRatio = (b.y-a.y)/dyAC;
        const float splitX = (c.x-a.x)*splitRatio+a.x;
        const float splitZ = (c.z-a.z)*splitRatio+a.z;
        const float dzdx = (splitZ-b.z)/(splitX-b.x);
        const float longDx = (c.x-a.x)/dyAC;
        const float longDz = (c.z-a.z)/dyAC;

        const int middleY = static_cast<int>(ceil(static_cast<double>(b.y)));
        float shortDx;
        float shortDz;
        float shortX;
        float shortZ;
        if (middleY>firstY)
        {
            const float dy = b.y-a.y;
            shortDx = (b.x-a.x)/dy;
            shortDz = (b.z-a.z)/dy;
            shortX = a.x;
            shortZ = a.z;
        }
        else
        {
            const float dy = c.y-b.y;
            shortDx = (c.x-b.x)/dy;
            shortDz = (c.z-b.z)/dy;
            shortX = b.x;
            shortZ = b.z;
        }

        float longX = a.x;
        float longZ = a.z;
        int y = firstY;
        if (static_cast<int>(ceil(static_cast<double>(c.y))) <= y) return;

        const int frameX = static_cast<int>(static_cast<short>(frame.offsetX));
        const int frameY = static_cast<int>(static_cast<short>(frame.offsetY));

        while (y<static_cast<int>(ceil(static_cast<double>(c.y))))
        {

            if (y == middleY)
            {
                const float dy = c.y-b.y;
                shortDx = (c.x-b.x)/dy;
                shortDz = (c.z-b.z)/dy;
                shortX = b.x;
                shortZ = b.z;
            }

            int xBegin;
            int xEnd;
            float z;
            if (longX>shortX)
            {
                xBegin = static_cast<int>(ceil(static_cast<double>(shortX)));
                xEnd = static_cast<int>(ceil(static_cast<double>(longX)));
                z = shortZ;
            }
            else
            {
                xBegin = static_cast<int>(ceil(static_cast<double>(longX)));
                xEnd = static_cast<int>(ceil(static_cast<double>(shortX)));
                z = longZ;
            }

            for (int x = xBegin;x<xEnd;++x, z += dzdx)
            {

                const int iz = TruncateFloatToInt32(z);
                const int projectedY = y+iz;
                if (projectedY<0 || projectedY >= 2048 || x<0 || x >= 2048) continue;

                const int localX = x-frameX;
                if (localX<0 || localX >= 1024) continue;

                const int localY = y-frameY;
                if (localY<0 || localY >= 1024) continue;
                if (pixelMask[localY*textureWidth+localX] == 0u) continue;

                VidGridUpdateCell(cells, x, projectedY, iz);
            }

            longX += longDx;
            longZ += longDz;
            shortX += shortDx;
            shortZ += shortDz;
            ++y;
        }
    }
}

void VID_SURFACE::SetLayer()
{

    if (spriteClass == 8u) runtimeFlags &= ~0x80u;

    if (surfaceMode == 6) layer = 10;
    else if (spriteClass == 8u)
    {
        if ((type&2u) == 0u && renderMode == 0u) layer = 0;
        else
        {
            layer = 2;
            runtimeFlags &= ~0x80u;
        }
    }
    else if (spriteClass == 23u) layer = 5;
    else if (spriteClass == 10u || spriteClass == 19u) layer = 11;
    else if (renderMode == 4u || renderMode == 5u)
    {
        type = static_cast<unsigned short>(type|0x80u);
        layer = 7;
    }
    else if ((type&2u) == 0u && (renderMode == 0u || renderMode == 6u)) layer = 3;
    else if ((property&0x40000000u) == 0u && renderMode != 7u) layer = 9;
    else layer = 4;

    if ((property&0x20u) == 0u || !frames)
    {
        SetStateBlock();
        return;
    }

    if ((storageFlags&8u) != 0u)
    {
        unsigned char* const frameData = surfaceData+frames[0].frameDataOffset;
        noGridZ = *reinterpret_cast<int*>(frameData-sizeof(int));
        if (noGridZ != 0)
        {
            const int frameCount = static_cast<int>(noCadr);
            gridCadrShift = reinterpret_cast<int*>(
                frameData-(static_cast<unsigned int>(frameCount)*sizeof(int)+sizeof(int)));
            const unsigned int vectorDwords = static_cast<unsigned int>(frameCount)+
                3u*static_cast<unsigned int>(noGridZ)+1u;
            gridZ = reinterpret_cast<VECTOR*>(
                frameData-vectorDwords*sizeof(unsigned int));
        }
    }
    if (gridZ)
    {
        SetStateBlock();
        return;
    }

    (void)::Error->Log("not fast for %s %i %i", vidName.m_value, (storageFlags>>3)&1u, static_cast<unsigned int>(noGridZ));

    maxYGridZ = 0.0f;
    minYGridZ = 0.0f;
    maxXGridZ = 0.0f;
    minXGridZ = 0.0f;

    VECTOR* const scratch = new(std::nothrow) VECTOR[4194304u];
    unsigned char* const pixelMask = new(std::nothrow) unsigned char[1048576u];
    gridCadrShift = new(std::nothrow) int[static_cast<unsigned int>(static_cast<int>(noCadr))];
    noGridZ = 0;

    const int bytesPerPixel = VidSurfacePixelBytes(pixelFormat);
    short depthGrid[256*256];

    for (int frameIndex = 0;frameIndex<static_cast<int>(noCadr);++frameIndex)
    {
        CADR_DATA& frame = frames[frameIndex];
        int textureWidth = static_cast<int>(static_cast<short>(frame.width));
        if ((type&0x10u) != 0u)
        {
            textureWidth = VidSurfaceRoundedTextureExtent(textureWidth);

            (void)VidSurfaceRoundedTextureExtent(static_cast<int>(static_cast<short>(frame.height)));
        }

        for (int i = 0;i<256*256;++i) depthGrid[i] = kEmptySoftwareGridCell;
        memset(pixelMask, 0, 1048576u);
        gridCadrShift[frameIndex] = noGridZ;

        if (frame.height == 0u) continue;

        const bool compressed = pixelFormat == D3DFMT_DXT1 ||
                              pixelFormat == D3DFMT_DXT3 ||
                              pixelFormat == D3DFMT_DXT5;
        if (!compressed)
        {
            unsigned char* stream = surfaceData+frame.frameDataOffset+
                static_cast<unsigned int>(frame.indexCount)*2u+
                static_cast<unsigned int>(frame.vertexCount+frame.extraDataCount)*4u;

            if (*reinterpret_cast<unsigned short*>(stream) != 0u)
            {
                unsigned char* maskCursor = pixelMask;
                while (*reinterpret_cast<unsigned short*>(stream) != 0u)
                {
                    maskCursor += *stream++;
                    int runCount = *stream++;
                    while (runCount-- > 0)
                    {
                        *maskCursor = 1u;
                        ++maskCursor;

                        stream += bytesPerPixel;
                        if ((type&0x8000u) != 0u) ++stream;
                        if ((type&0x2000u) != 0u) ++stream;

                        const int maskOffset = static_cast<int>(maskCursor-pixelMask);
                        int localX = maskOffset%textureWidth;
                        int localY = maskOffset/textureWidth;
                        const int x = localX+static_cast<int>(static_cast<short>(frame.offsetX));
                        const int y = localY+static_cast<int>(static_cast<short>(frame.offsetY));

                        if ((type&4u) != 0u)
                        {
                            const int rawDepth = static_cast<int>(*reinterpret_cast<unsigned short*>(stream));
                            stream += 2;
                            const int z = (rawDepth-1024)/8;
                            if (nVid == 632 && surfaceMode == 3)
                            {
                                const int projectedY = y+z;
                                if (projectedY >= 0 && projectedY<2048 && x >= 0 && x<2048)
                                    VidGridUpdateCell(depthGrid, x, projectedY, z);
                            }
                        }

                        if (frame.vertexCount == 0u)
                            depthGrid[(y/8)*256+(x/8)] = 0;
                    }
                }
            }
        }

        if (frame.vertexCount != 0u &&
           !(nVid == 632 && (type&4u) != 0u && surfaceMode == 3))
        {
            const unsigned short* const indices =
                reinterpret_cast<const unsigned short*>(surfaceData+frame.frameDataOffset);
            const unsigned int* const vertices = reinterpret_cast<const unsigned int*>(
                surfaceData+frame.frameDataOffset+static_cast<unsigned int>(frame.indexCount)*2u);

            for (unsigned int i = 0;i<static_cast<unsigned int>(frame.indexCount);i += 3u)
            {
                VidGridVertex a = VidDecodeGridVertex(vertices[indices[i+0u]]);
                VidGridVertex b = VidDecodeGridVertex(vertices[indices[i+1u]]);
                VidGridVertex c = VidDecodeGridVertex(vertices[indices[i+2u]]);
                VidRasterizeGridTriangle(frame, pixelMask, textureWidth, depthGrid, a, b, c);
            }
        }

        const int rows = static_cast<int>(vidSizeY)/8;
        for (int row = rows-1;row >= 0;--row)
        {
            for (int column = 0;column<static_cast<int>(vidSizeX)/8;++column)
            {
                const short z = depthGrid[row*256+column];
                if (z == kEmptySoftwareGridCell) continue;

                scratch[noGridZ].x = static_cast<float>(column*8-static_cast<int>(vidSizeX)/2);
                scratch[noGridZ].y = static_cast<float>(row*8-static_cast<int>(vidSizeY)/2);
                scratch[noGridZ].z = static_cast<float>(z);
                noGridZ = BitsToSigned32(static_cast<unsigned int>(noGridZ)+1u);
            }
        }
    }

    gridZ = new(std::nothrow) VECTOR[static_cast<unsigned int>(noGridZ)];
    for (int i = 0;i<noGridZ;++i)
    {
        gridZ[i] = scratch[i];
        if (!(gridZ[i].x >= minXGridZ)) minXGridZ = gridZ[i].x;
        if (gridZ[i].x>maxXGridZ) maxXGridZ = gridZ[i].x;
        if (!(gridZ[i].y >= minYGridZ)) minYGridZ = gridZ[i].y;
        if (gridZ[i].y>maxYGridZ) maxYGridZ = gridZ[i].y;
    }

    delete[] scratch;
    delete[] pixelMask;
    SetStateBlock();
}

void VID_SURFACE::CopyGraphFromVid(VID_SURFACE* source)
{
    ReleaseVideoMemory();
    if (mesh) mesh->Release();
    mesh = source->mesh;
    if (mesh) mesh->AddRef();

    if (nextMirror == this)
    {
        if ((storageFlags&8u) == 0u && cadrLinkXYZ)
        {
            delete[] cadrLinkXYZ;
            cadrLinkXYZ = 0;
        }
        cadrLinkXYZ = 0;

        VidReleaseFrameArray(*this);
        surfaceData = 0;
    }
    else
    {
        VID* predecessor = nextMirror;
        while (predecessor->nextMirror != this) predecessor = predecessor->nextMirror;
        predecessor->nextMirror = nextMirror;
    }

    nextMirror = source->nextMirror;
    source->nextMirror = this;

    layer = source->layer;
    type = source->type;
    noCadr = source->noCadr;
    frameSpeedDefault = source->frameSpeedDefault;
    vidSizeX = source->vidSizeX;
    vidSizeY = source->vidSizeY;
    buildSizeX = source->buildSizeX;
    buildSizeY = source->buildSizeY;
    vidName = source->vidName;

    surfaceData = source->surfaceData;
    cadrLinkXYZ = source->cadrLinkXYZ;
    frames = source->frames;
    palette = source->palette;
    pixelFormat = source->pixelFormat;
    surfaceMode = source->surfaceMode;
    renderMode = source->renderMode;
    extendedHeaderValue = source->extendedHeaderValue;
    lastRenderUseTick = source->lastRenderUseTick;

    surfaceFlags = (surfaceFlags&~1u)|(source->surfaceFlags&1u);
    surfaceFlags = (surfaceFlags&~2u)|(source->surfaceFlags&2u);
    storageFlags = ((storageFlags&~8u)|(source->storageFlags&8u))&~4u;

    if (source->recolorPalette)
    {
        if (!recolorPalette)
            recolorPalette = new(std::nothrow) unsigned int[256u];
        for (unsigned int i = 0;i<256u;++i) recolorPalette[i] = source->recolorPalette[i];
    }
    else if (recolorPalette)
    {
        delete[] recolorPalette;
        recolorPalette = 0;
    }

    if ((storageFlags&8u) == 0u || (property&8u) != 0u)
    {
        if (gridZ)
        {
            delete[] gridZ;
            gridZ = 0;
        }
        if (gridCadrShift)
        {
            delete[] gridCadrShift;
            gridCadrShift = 0;
        }
    }

    noGridZ = source->noGridZ;
    gridZ = source->gridZ;
    gridCadrShift = source->gridCadrShift;
    minXGridZ = source->minXGridZ;
    maxXGridZ = source->maxXGridZ;
    minYGridZ = source->minYGridZ;
    maxYGridZ = source->maxYGridZ;
}

void VID_SURFACE::CopyFromVid(VID_SURFACE* source)
{
    sourceVid = source;
    CopyGraphFromVid(source);

    spriteType = source->spriteType;
    spriteClass = source->spriteClass;
    property = source->property;
    moveMask = source->moveMask;
    sizeXYZ = source->sizeXYZ;
    maxHp = source->maxHp;
    maxSpeed = source->maxSpeed;
    randomSpeed = source->randomSpeed;
    maxZSpeed = source->maxZSpeed;
    randomZSpeed = source->randomZSpeed;
    acceleration = source->acceleration;
    slow = source->slow;
    rotationSpeed = source->rotationSpeed;
    deathRange = source->deathRange;
    deathDamageMin = source->deathDamageMin;
    deathDamageMax = source->deathDamageMax;
    deathPush = source->deathPush;
    linkXYZ = source->linkXYZ;
    nLinkVid = source->nLinkVid;
    linkVid = source->linkVid;
    topZ = source->topZ;
    forMoveUpZ = source->forMoveUpZ;
    forMoveDownZ = source->forMoveDownZ;
    lifeTime = source->lifeTime;
    noDir = source->noDir;
    extendedHeaderValue = source->extendedHeaderValue;
    gammaRaw = source->gammaRaw;
    scaleXYZ = source->scaleXYZ;
    halfSizeXY = source->halfSizeXY;
    realSizeXYZ = source->realSizeXYZ;

    for (int i = 0;i<AnimationSlotCount;++i)
    {
        noAnimCadr[i] = source->noAnimCadr[i];
        sfx[i] = source->sfx[i];
        frameSpeed[i] = source->frameSpeed[i];
        childX[i] = source->childX[i];
        childY[i] = source->childY[i];
        childZ[i] = source->childZ[i];
        nChildVid[i] = source->nChildVid[i];
        childVid[i] = source->childVid[i];
        noChild[i] = source->noChild[i];
        animationBaseFrame[i] = source->animationBaseFrame[i];
        animationFrameCount[i] = source->animationFrameCount[i];
    }

    for (int i = 0;i<PlayerSlotCount;++i)
    {
        maxHpByPlayer[i] = source->maxHpByPlayer[i];
        gammaByPlayer[i] = source->gammaByPlayer[i];
    }

    actionAuxStateRequired = source->actionAuxStateRequired;
    movementTactEnabled = source->movementTactEnabled;
    runtimeFlags = (runtimeFlags&~0x80u)|(source->runtimeFlags&0x80u);
    runtimeFlags = (runtimeFlags&~0x100u)|(source->runtimeFlags&0x100u);
    runtimeFlags = (runtimeFlags&~0x1000u)|(source->runtimeFlags&0x1000u);
    runtimeFlags = (runtimeFlags&~0x200u)|(source->runtimeFlags&0x200u);
}

template<> int* LIST<int>::operator[](int index)
{
    return m_data+index;
}

template<> const int* LIST<int>::operator[](int index) const
{
    return m_data+index;
}

template<> int LIST<int>::No() const
{
    return m_count;
}

template<>
void LIST<int>::Expand(int requestedCapacity)
{
    if (requestedCapacity <= m_capacity) return;
    int* const oldData = m_data;
    int* const newData = new(std::nothrow) int[static_cast<unsigned int>(requestedCapacity)];
    m_data = newData;
    if (!newData)
    {
        STRING format("!!!ERROR!!!::LIST: Not enough memory %i");
        ::Error->LogExit(format, requestedCapacity);
    }
    if (oldData)
    {
        for (int index = 0;index<m_capacity;++index) m_data[index] = oldData[index];
        delete[] oldData;
    }
    m_capacity = requestedCapacity;
}

VID* VID_SOFTWARE::CreateMirror()
{
    return new(std::nothrow) VID_SOFTWARE(*this);
}

VID* VID_FONT::CreateMirror()
{
    return new(std::nothrow) VID_FONT(*this);
}

VID* VID_SURFACE::CreateMirror()
{
    VID_SURFACE* const object = new(std::nothrow) VID_SURFACE();
    if (!object) return 0;
    object->CopyGraphFromVid(this);
    return object;
}

void VID_SURFACE::RestoreDeviceObjects()
{
    SetStateBlock();
}

void VID_SURFACE::SetStateBlock()
{
}

VID* VID_LIGHT::CreateMirror()
{
    return new(std::nothrow) VID_LIGHT(*this);
}
