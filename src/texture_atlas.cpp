#include "myerror.h"
#include "texture_atlas.h"
#include "graph.h"
#include "vid.h"
#include "sprite.h"
#include "map.h"
#include "engine_globals.h"
#include "weapon_runtime.h"
#include <cstring>
#include <new>

namespace
{
int AtlasFormatBits(D3DFORMAT format)
{
    switch (format)
    {
    case D3DFMT_R8G8B8: return 24;
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8: return 32;
    case D3DFMT_R5G6B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_A4R4G4B4:
    case D3DFMT_A8L8:
    case D3DFMT_V8U8: return 16;
    case D3DFMT_A8:
    case D3DFMT_P8:
    case D3DFMT_L8:
    case D3DFMT_DXT3:
    case D3DFMT_DXT5: return 8;
    case D3DFMT_DXT1: return 4;
    default: return 0;
    }
}

VECTOR AtlasCurve(const WEAPON_RUNTIME* owner, const VECTOR* keys, float phase)
{
    const int index = phase >= -2147483648.0f && phase < 2147483648.0f
        ? static_cast<int>(phase) : (-2147483647-1);
    if (index >= owner->maxColumn) return keys[owner->maxColumn];
    return keys[index]+(keys[index+1]-keys[index])*(phase-static_cast<float>(index));
}

}

int SPRITE::GetDrawTransform(VECTOR* position, VECTOR* scale, ANGLE* angle) const
{
    const SPRITE* const sprite = this;
    VID* const vid = sprite->Vid();
    const EX_SPRITE_DATA* const extra = sprite->ExtraData();
    VECTOR localScale = vid->scaleXYZ*(extra ? extra->scale : VECTOR(1.0f, 1.0f, 1.0f));
    VECTOR localPosition = sprite->Coor();
    if ((vid->property&0x8000u) != 0u)
        localPosition += VECTOR(0.0f, 1000.0f, 1000.0f);
    if ((vid->property&0x10000u) != 0u)
        localPosition.z += FSin[(::CurrentTime>>3u)&255u]*vid->topZ;
    if ((vid->runtimeFlags&2u) != 0u)
        localScale = localScale*AtlasCurve(vid->weapon, vid->weapon->Scale, extra->effectCurvePosition);
    if ((vid->runtimeFlags&4u) != 0u)
        localPosition += AtlasCurve(vid->weapon, vid->weapon->Shift, extra->effectCurvePosition);

    const float extentX = vid->buildSizeX*0.5f*localScale.x*1.29999995f;
    const float extentY = vid->buildSizeY*0.5f*localScale.y*1.29999995f;
    const float projectedY = localPosition.y-localPosition.z;
    if (localPosition.x-extentX > VID::viewMax.x && projectedY-extentY > VID::viewMax.y)
        return 1;
    if (localPosition.x+extentX < VID::viewMin.x && projectedY+extentY < VID::viewMin.y)
        return 1;

    unsigned char direction = 0u;
    if ((vid->property&0x20000000u) != 0u)
    {
        direction = sprite->Direction().value;
        if ((vid->property&0x80000u) != 0u)
        {
            const VECTOR2 current = sprite->Coor().ViewCoor();
            const VECTOR2 previous = extra->cachedPosition.ViewCoor();
            if (current.x != previous.x || current.y != previous.y)
                direction = ANGLE((current-previous)*VECTOR2(1.0f, 1.41430664f)).value;
            else if (sprite->m_zSpeed != 0.0f)
            {
                const ANGLE stored(direction);
                const float x = stored.Sin()*sprite->m_speed;
                const float y = static_cast<float>((static_cast<double>(stored.Cos())*
                    sprite->m_speed+sprite->m_zSpeed)/-0.7070602178573608);
                direction = Decart2Polar(x, y).value;
            }
        }
        else if ((vid->property&0x2000u) == 0u)
            direction = static_cast<unsigned char>(direction-
                static_cast<unsigned char>((vid->RealDirectionWithGraphDetail(direction)<<8)/vid->noDir));
    }

    localPosition.x -= Map->ShiftXY().x;
    localPosition.y = static_cast<float>((localPosition.y-Map->ShiftXY().y)/0.7070602178573608);
    localPosition.x -= (Graph->viewLeft+Graph->viewRight)*0.5f;
    localPosition.y -= static_cast<float>((Graph->viewTop+Graph->viewBottom)/1.4141204357147217);
    *position = localPosition;
    *scale = localScale;
    *angle = ANGLE(direction);
    return 0;
}

unsigned short* TEXTURE_ATLAS::indices = 0;

TEXTURE_ATLAS::TEXTURE_ATLAS() : noVid(0), vertices(0)
{
    for (int index = 0; index < MaxVids; ++index) vids[index] = 0;
}

TEXTURE_ATLAS::~TEXTURE_ATLAS() noexcept(false)
{
    noVid = 0;
    ReleaseVideoMemory();
    delete[] indices;
    indices = 0;
    delete[] vertices;
    vertices = 0;
}

void TEXTURE_ATLAS::ReleaseVideoMemory()
{
    texture.ReleaseVideoMemory();
}

void TEXTURE_ATLAS::Clear()
{
    noVertex = 0;
}

TEXTURE* TEXTURE_ATLAS::GetTexture()
{
    return &texture;
}

int TEXTURE_ATLAS::IsAllocated() const
{
    return texture.m_texture || texture.m_cpuData ? 1 : 0;
}

void TEXTURE_ATLAS::Error(TYPE_ERROR type, const STRING& subject, unsigned long code) const
{
    ::Error->Error(STRING("TEXTURE_ATLAS[%i,%i,...]"), type, subject, code,
        vids[0] ? vids[0]->nVid : 0, vids[1] ? vids[1]->nVid : 0);
}

int TEXTURE_ATLAS::Intersects(const CADR_DATA& frame, int x, int y, int width, int height)
{
    const int storedWidth = static_cast<short>(frame.width);
    const int storedHeight = static_cast<short>(frame.height);
    const int deltaX = (storedWidth+2*frame.atlasOffsetX)/2-(width+2*x)/2;
    const int deltaY = (storedHeight+2*frame.atlasOffsetY)/2-(height+2*y)/2;
    const int absoluteX = deltaX < 0 ? -deltaX : deltaX;
    const int absoluteY = deltaY < 0 ? -deltaY : deltaY;
    return absoluteX < (storedWidth+width)/2 && absoluteY < (storedHeight+height)/2 ? 1 : 0;
}

int TEXTURE_ATLAS::AddVid(VID_SURFACE* vid)
{
    if (noVid >= MaxVids)
    {
        Error(E_CREATE, STRING("TextureAtlas, noVid >= MAX_VID_IN_ATLAS"), vid->nVid);
        return 1;
    }
    if (!IsAllocated())
    {
        texture.Create(AtlasSize, AtlasSize, D3DFMT_A8R8G8B8, 3u);
        int pitch;
        unsigned char* const data = texture.Lock(&pitch, 0);
        if (data)
        {
            std::memset(data, 0, static_cast<unsigned int>(AtlasFormatBits(texture.m_format)/8)*
                AtlasSize*AtlasSize);
            texture.UnLock();
        }
    }

    CADR_DATA* placed[MaxFrames];
    int noPlaced = 0;
    for (int index = 0; index < noVid; ++index)
    {
        VID_SURFACE* const existing = vids[index];
        if (existing == vid)
        {
            vid->textureAtlas = this;
            return 2;
        }
        for (VID* mirror = existing->nextMirror; mirror != existing; mirror = mirror->nextMirror)
        {
            if (mirror == vid)
            {
                vid->textureAtlas = this;
                return 2;
            }
        }
        for (int frame = 0; frame < static_cast<int>(existing->noCadr); ++frame)
            placed[noPlaced++] = existing->frames+frame;
    }

    for (int index = 0; index < static_cast<int>(vid->noCadr); ++index)
    {
        CADR_DATA* const frame = vid->frames+index;
        placed[noPlaced] = frame;
        const int width = static_cast<short>(frame->width);
        const int height = static_cast<short>(frame->height);
        bool found = false;
        for (int y = 0; y <= AtlasSize-height && !found; ++y)
        {
            int x = 0;
            while (x <= AtlasSize-width)
            {
                int collision = noPlaced-1;
                while (collision >= 0 && !Intersects(*placed[collision], x, y, width, height))
                    --collision;
                if (collision < 0)
                {
                    frame->atlasOffsetX = static_cast<unsigned short>(x);
                    frame->atlasOffsetY = static_cast<unsigned short>(y);
                    frame->CopyToTexture(&texture, vid, x, y);
                    ++noPlaced;
                    found = true;
                    break;
                }
                x = placed[collision]->atlasOffsetX+static_cast<short>(placed[collision]->width);
            }
        }
        if (!found)
        {
            Error(E_ERROR, STRING("Can't replace rectangle"), vid->nVid);
            return 1;
        }
    }

    vids[noVid++] = vid;
    vid->textureAtlas = this;
    for (VID* mirror = vid->nextMirror; mirror != vid; mirror = mirror->nextMirror)
        static_cast<VID_SURFACE*>(mirror)->textureAtlas = this;
    return 0;
}

int TEXTURE_ATLAS::AddSprite(SPRITE* sprite)
{
    if (!vertices) vertices = new(std::nothrow) ATLAS_VERTEX[MaxQuads*4];
    VID_SURFACE* const vid = static_cast<VID_SURFACE*>(sprite->Vid());
    CADR_DATA* const frame = vid->frames+sprite->CurrentFrame();
    const GAMMA gamma(vid->gammaRaw, sprite->GetGamma());
    VECTOR position, scale;
    ANGLE angle;
    (void)sprite->GetDrawTransform(&position, &scale, &angle);
    const float left = static_cast<float>(static_cast<short>(frame->offsetX)-vid->vidSizeX/2)*scale.x;
    const float right = static_cast<float>(static_cast<short>(frame->offsetX)-vid->vidSizeX/2+
        static_cast<short>(frame->width))*scale.x;
    const float top = static_cast<float>((static_cast<short>(frame->offsetY)-vid->vidSizeY/2)/
        0.7070602178573608)*scale.y;
    const float bottom = static_cast<float>((static_cast<short>(frame->offsetY)-vid->vidSizeY/2+
        static_cast<short>(frame->height))/0.7070602178573608)*scale.y;
    const float u0 = (static_cast<float>(frame->atlasOffsetX)+0.5f)/static_cast<float>(BitsToSigned32(texture.m_width));
    const float v0 = (static_cast<float>(frame->atlasOffsetY)+0.5f)/static_cast<float>(BitsToSigned32(texture.m_height));
    const float u1 = (static_cast<float>(frame->atlasOffsetX)+0.5f+
        static_cast<float>(static_cast<short>(frame->width)))/static_cast<float>(BitsToSigned32(texture.m_width));
    const float v1 = (static_cast<float>(frame->atlasOffsetY)+0.5f+
        static_cast<float>(static_cast<short>(frame->height)))/static_cast<float>(BitsToSigned32(texture.m_height));
    const VECTOR corners[4] = { VECTOR(left, top, 0.0f), VECTOR(left, bottom, 0.0f),
        VECTOR(right, top, 0.0f), VECTOR(right, bottom, 0.0f) };
    const VECTOR2 uv[4] = { VECTOR2(u0, v0), VECTOR2(u0, v1), VECTOR2(u1, v0), VECTOR2(u1, v1) };
    for (int index = 0; index < 4; ++index)
    {
        ATLAS_VERTEX& vertex = vertices[noVertex++];
        vertex.position = corners[index];
        vertex.uv = uv[index];
    }
    for (int index = noVertex-4; index < noVertex; ++index)
    {
        ATLAS_VERTEX& vertex = vertices[index];
        const float x = vertex.position.x;
        const float y = vertex.position.y;
        vertex.position.x = x*angle.Cos()-y*angle.Sin();
        vertex.position.y = x*angle.Sin()+y*angle.Cos();
        vertex.position += position;
        vertex.diffuse = ~gamma.modulate;
        vertex.specular = gamma.add&0x00FFFFFFu;
    }
    return 0;
}

void TEXTURE_ATLAS::DrawArrayObjects()
{
    if (noVertex == 0 || !IsAllocated()) return;
    IDirect3DDevice9* const device = Graph->device;
    ID3DXEffect* const effect = Graph->pseudo3DEffect;
    effect->SetTechnique("DrawArrayObjects");
    const unsigned int modulate = ~Graph->gamma.modulate;
    const unsigned int add = Graph->gamma.add&0x00FFFFFFu;
    const float factor = 0.00392156886f;
    const D3DXVECTOR4 diffuse(static_cast<float>((modulate>>16)&255u)*factor,
        static_cast<float>((modulate>>8)&255u)*factor, static_cast<float>(modulate&255u)*factor,
        static_cast<float>(modulate>>24)*factor);
    const D3DXVECTOR4 specular(static_cast<float>((add>>16)&255u)*factor,
        static_cast<float>((add>>8)&255u)*factor, static_cast<float>(add&255u)*factor, 0.0f);
    effect->SetVector(Graph->diffuseParameter, &diffuse);
    effect->SetVector(Graph->specularParameter, &specular);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    device->SetRenderState(D3DRS_ALPHAREF, 0);
    device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
    device->SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATEREQUAL);
    Graph->SetAlphaBlend(D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, 7u);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, 0u);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, 0u);
    if (!indices)
    {
        indices = new(std::nothrow) unsigned short[MaxQuads*6];
        for (int quad = 0; quad < MaxQuads; ++quad)
        {
            const unsigned short vertex = static_cast<unsigned short>(quad*4);
            unsigned short* const output = indices+quad*6;
            output[0] = vertex;
            output[1] = static_cast<unsigned short>(vertex+2u);
            output[2] = static_cast<unsigned short>(vertex+1u);
            output[3] = static_cast<unsigned short>(vertex+3u);
            output[4] = static_cast<unsigned short>(vertex+1u);
            output[5] = static_cast<unsigned short>(vertex+2u);
        }
    }
    texture.SetTextureToDevice(0);
    device->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_SPECULAR|D3DFVF_TEX1);
    unsigned int passes;
    effect->Begin(&passes, 0u);
    for (unsigned int pass = 0; pass < passes; ++pass)
    {
        effect->Pass(pass);
        device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0u, noVertex, noVertex/2,
            indices, D3DFMT_INDEX16, vertices, sizeof(ATLAS_VERTEX));
    }
    effect->End();
    MESH::noTri += noVertex/2;
    MESH::noVertex += noVertex;
    for (int index = 0; index < noVid; ++index) vids[index]->lastRenderUseTick = ::RealCurrentTime;
}
