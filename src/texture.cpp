#include "mylib.h"
#include <new>
#define INPUT WIN32_INPUT
#include <windows.h>
#include <d3d9.h>
#undef INPUT

#include "myerror.h"
#include "texture.h"
#include "basetype.h"
#include "graph.h"
#include "vid.h"
#include "region.h"
#include "resread.h"

namespace {
IDirect3DSurface9* stagingSurface = 0;
IDirect3DTexture9* stagingTexture = 0;
RECT stagingRect = {};
}

int TEXTURE::memoryInUse = 0;
int TEXTURE::noPalettes = 0;
int TEXTURE::capsSquare = 0;
int TEXTURE::capsDynamic = 0;
int TEXTURE::capsPalette = 0;
int TEXTURE::capsBump = 0;
int TEXTURE::capsDXT = 0;
int TEXTURE::capsAlphaPalette = 0;
int TEXTURE::capsCondSizePow2 = 0;
int TEXTURE::capsSizePower2 = 1;
int TEXTURE::capsMaxSizeX = 256;
int TEXTURE::capsMaxSizeY = 256;

void TEXTURE::SetDeviceCaps(IDirect3DDevice9* device)
{
    D3DCAPS9 caps;
    IDirect3DDevice9* const d3dDevice = device;
    const HRESULT capsHr = d3dDevice->GetDeviceCaps(&caps);
    if (capsHr<0)
    {
        STRING subject("Caps");
        if (::Error)
        {
            STRING objectFormat("TEXTURE");
            ::Error->Error(objectFormat, E_GET, subject, 0UL);
        }
    }

    const unsigned int caps2 = caps.Caps2;
    const unsigned int textureCaps = caps.TextureCaps;
    capsMaxSizeX = caps.MaxTextureWidth;
    capsMaxSizeY = caps.MaxTextureHeight;
    capsSquare = textureCaps&D3DPTEXTURECAPS_SQUAREONLY;
    capsDynamic = caps2&D3DCAPS2_DYNAMICTEXTURES;
    capsSizePower2 = textureCaps&D3DPTEXTURECAPS_POW2;
    capsAlphaPalette = textureCaps&D3DPTEXTURECAPS_ALPHAPALETTE;
    capsCondSizePow2 = textureCaps&D3DPTEXTURECAPS_NONPOW2CONDITIONAL;
    if (capsSizePower2 == 0u)
        capsCondSizePow2 = 1u;

    IDirect3DSurface9* backBuffer;
    if (d3dDevice->GetBackBuffer(0u, 0u, D3DBACKBUFFER_TYPE_MONO, &backBuffer) == 0u)
    {
        D3DSURFACE_DESC desc;
        (void) backBuffer->GetDesc(&desc);

        IDirect3D9* d3dObject;
        (void) d3dDevice->GetDirect3D(&d3dObject);
        if (d3dObject->CheckDeviceFormat(caps.AdapterOrdinal, caps.DeviceType, desc.Format,
            0u, D3DRTYPE_TEXTURE, D3DFMT_P8) == 0u)
        capsPalette = 1u;
        else
        capsAlphaPalette = 0u;
        if (d3dObject->CheckDeviceFormat(caps.AdapterOrdinal, caps.DeviceType, desc.Format,
            0u, D3DRTYPE_TEXTURE, D3DFMT_V8U8) == D3D_OK)
            capsBump |= 1u;

        if (d3dObject->CheckDeviceFormat(caps.AdapterOrdinal, caps.DeviceType, desc.Format,
            0u, D3DRTYPE_TEXTURE, D3DFMT_DXT1) == 0u)
        capsDXT = capsDXT | 1u;
        if (d3dObject->CheckDeviceFormat(caps.AdapterOrdinal, caps.DeviceType, desc.Format,
            0u, D3DRTYPE_TEXTURE, D3DFMT_DXT3) == 0u)
        capsDXT = capsDXT | 4u;
        if (d3dObject->CheckDeviceFormat(caps.AdapterOrdinal, caps.DeviceType, desc.Format,
            0u, D3DRTYPE_TEXTURE, D3DFMT_DXT5) == 0u)
        capsDXT = capsDXT | 0x10u;
        (void) backBuffer->Release();
        (void) d3dObject->Release();
    }

    capsPalette = 0u;
    capsAlphaPalette = 0u;

    STRING capability;
    if (capsSquare != 0u) capability += "SQUARE ";
    capability += (capsSizePower2 != 0u)
    ? "POWER2 "
    : "NOTPOWER2 ";
    if (capsCondSizePow2 != 0u) capability += "COND_NON_POW2 ";
    if (capsDynamic != 0u) capability += "DYNAMIC ";
    if (capsPalette != 0u) capability += "PALETTE ";
    if (capsBump != 0u) capability += "BUMP ";
    if (capsAlphaPalette != 0u) capability += "ALPHA_PALETTE ";
    if ((capsDXT&1u) != 0u) capability += "DXT1 ";
    if ((capsDXT&4u) != 0u) capability += "DXT3 ";
    if ((capsDXT&0x10u) != 0u) capability += "DXT5 ";

    const unsigned int textureMemory = Graph->device->GetAvailableTextureMem();
    {
        STRING limits = Printf("MAXSIZE=%i,%i MAXTEXTUREMEMORY = %i", capsMaxSizeX, capsMaxSizeY, textureMemory);
        capability += limits;
    }
    ::Error->Log("TextureCaps=%s", capability.m_value);
    capsSizePower2 = 1u;
}

TEXTURE::TEXTURE()
{
    m_texture = 0;
    m_surface = 0;
    m_cpuData = 0;
}

TEXTURE::TEXTURE(int width, int height, D3DFORMAT format, unsigned long flags)
{
    Create(width, height, format, flags);
}

TEXTURE::TEXTURE(int width, int height, D3DFORMAT format, unsigned long flags,
    COLOR* colors, RESOURCE* resource)
{
    static COLOR palette[256];
    Create(width, height, format, flags);

    PALETTEENTRY* paletteEntries = 0;
    PALETTEENTRY entries[256];
    if (colors)
    {
        for (int index = 0; index<256; ++index)
        {
            entries[index].peFlags = static_cast<BYTE>(colors[index].value>>24);
            entries[index].peRed = static_cast<BYTE>(colors[index].value>>16);
            entries[index].peGreen = static_cast<BYTE>(colors[index].value>>8);
            entries[index].peBlue = static_cast<BYTE>(colors[index].value);
        }
        paletteEntries = entries;
    }
    if (m_format == D3DFMT_P8) SetPalette(paletteEntries);

    int bitsPerPixel;
    switch (format)
    {
    case D3DFMT_A8R8G8B8:
    case D3DFMT_X8R8G8B8:
        bitsPerPixel = 32;
        break;
    case D3DFMT_R5G6B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_A4R4G4B4:
        bitsPerPixel = 16;
        break;
    case D3DFMT_R8G8B8:
        bitsPerPixel = 24;
        break;
    case D3DFMT_P8:
    case D3DFMT_DXT3:
    case D3DFMT_DXT5:
        bitsPerPixel = 8;
        break;
    case D3DFMT_DXT1:
        bitsPerPixel = 4;
        break;
    default:
        bitsPerPixel = 0;
        break;
    }
    const int dataSize = BitsToSigned32(static_cast<unsigned int>(width)*
        static_cast<unsigned int>(height)*static_cast<unsigned int>(bitsPerPixel))/8;
    unsigned char* const data = new(std::nothrow) unsigned char[static_cast<unsigned int>(dataSize)];
    (void)resource->Read(data, dataSize);

    IDirect3DSurface9* surface;
    (void)m_texture->GetSurfaceLevel(0u, &surface);
    const PALETTEENTRY* const destinationPalette = m_format == D3DFMT_P8?paletteEntries:0;
    RECT sourceRect = {0, 0, width, height};
    RECT destinationRect = {0, 0, width, height};
    unsigned int pitch = static_cast<unsigned int>(bitsPerPixel)*static_cast<unsigned int>(width);
    if (format == D3DFMT_DXT1 || format == D3DFMT_DXT3 || format == D3DFMT_DXT5)
        pitch >>= 1;
    else
        pitch >>= 3;
    const unsigned int colorKey = (m_flags&8u) != 0u?0u:COLOR(0, 0, 0).value;
    (void)D3DXLoadSurfaceFromMemory(surface, destinationPalette, &destinationRect,
        data, format, pitch, paletteEntries, &sourceRect, D3DX_FILTER_NONE, colorKey);
    (void)surface->Release();
    delete[] data;
}

static unsigned int TextureByteCount(D3DFORMAT format, unsigned int width, unsigned int height)
{
    const unsigned int pixels = width*height;
    if (format == D3DFMT_DXT1) return static_cast<unsigned int>(BitsToSigned32(pixels)/2);
    if (format == D3DFMT_P8 || format == D3DFMT_L8) return pixels;
    if (format == D3DFMT_A8R8G8B8) return pixels*4u;
    if (format == D3DFMT_R8G8B8) return pixels*3u;
    return pixels*2u;
}

void TEXTURE::Create(int width, int height, D3DFORMAT format, unsigned long flags)
{
    TEXTURE* const owner = this;
    owner->m_texture = 0;
    owner->m_surface = 0;
    owner->m_cpuData = 0;

    int effectiveWidth = width;
    int effectiveHeight = height;
    D3DFORMAT effectiveFormat = format;
    unsigned int effectiveFlags = static_cast<unsigned int>(flags);

    if (capsDynamic == 0u && (effectiveFlags&7u) == 1u)
        effectiveFlags &= ~1u;

    if (capsSquare != 0u)
    {
        if (effectiveWidth>effectiveHeight) effectiveHeight = effectiveWidth;
        else if (effectiveWidth<effectiveHeight) effectiveWidth = effectiveHeight;
    }

    if (capsCondSizePow2 == 0u)
    {
        int powerOfTwoWidth = 1;
        int powerOfTwoHeight = 1;
        while (effectiveWidth>powerOfTwoWidth) powerOfTwoWidth += powerOfTwoWidth;
        while (effectiveHeight>powerOfTwoHeight) powerOfTwoHeight += powerOfTwoHeight;
        effectiveWidth = powerOfTwoWidth;
        effectiveHeight = powerOfTwoHeight;
    }
    if (effectiveWidth <= 4) effectiveWidth = 4;
    if (effectiveHeight <= 4) effectiveHeight = 4;

    if (effectiveWidth>static_cast<int>(capsMaxSizeX) || effectiveWidth <= 0)
    {
        STRING subject("initial sizeX");
        if (Error)
        {
            STRING object("TEXTURE");
            Error->Error(object, E_INVALID, subject, static_cast<unsigned int>(effectiveWidth));
        }
        return;
    }
    if (effectiveHeight>static_cast<int>(capsMaxSizeY) || effectiveHeight <= 0)
    {
        STRING subject("initial sizeY");
        if (Error)
        {
            STRING object("TEXTURE");
            Error->Error(object, E_INVALID, subject, static_cast<unsigned int>(effectiveHeight));
        }
        return;
    }

    owner->m_flags = effectiveFlags;
    if (effectiveFormat == D3DFMT_A8R8G8B8 || effectiveFormat == D3DFMT_A4R4G4B4 || effectiveFormat == D3DFMT_A1R5G5B5 || effectiveFormat == D3DFMT_DXT3 || effectiveFormat == D3DFMT_DXT5)
    {
        owner->m_flags = effectiveFlags | 8u;
    }

    if (effectiveFormat == D3DFMT_P8 && (owner->m_flags&8u) != 0u && capsAlphaPalette == 0u)
        effectiveFormat = D3DFMT_A8R8G8B8;

    if ((effectiveFlags&0x10u) != 0u)
    {
        if (effectiveFormat == D3DFMT_P8)
        {
            if ((owner->m_flags&8u) == 0u) effectiveFormat = D3DFMT_A1R5G5B5;
            else if (capsPalette == 0u) effectiveFormat = D3DFMT_R5G6B5;
        }
        else if (effectiveFormat == D3DFMT_R5G6B5) effectiveFormat = D3DFMT_A1R5G5B5;
        else if (effectiveFormat == D3DFMT_R8G8B8) effectiveFormat = D3DFMT_A8R8G8B8;
    }
    else if (effectiveFormat == D3DFMT_P8 && capsPalette == 0u)
    {
        effectiveFormat = D3DFMT_R5G6B5;
    }

    HRESULT hr = 0;
    if ((effectiveFlags&4u) != 0u && effectiveFormat == D3DFMT_D16)
    {
        const unsigned int bytes = static_cast<unsigned int>(effectiveWidth)*static_cast<unsigned int>(effectiveHeight)*2u;
        owner->m_cpuData = new(std::nothrow) unsigned char[bytes];
    }
    else
    {
        for (; ;)
        {
            IDirect3DDevice9* const device = Graph->device;
            const unsigned int mode = effectiveFlags&7u;
            unsigned int usage = 0u;
            if (mode == 2u) usage = D3DUSAGE_RENDERTARGET;
            if (mode == 1u) usage |= D3DUSAGE_DYNAMIC;
            const D3DPOOL pool = (mode == 0u)?D3DPOOL_MANAGED:
            ((mode == 4u)?D3DPOOL_SYSTEMMEM:D3DPOOL_DEFAULT);
            hr = device->CreateTexture(static_cast<unsigned int>(effectiveWidth), static_cast<unsigned int>(effectiveHeight),
                1u, usage, effectiveFormat, pool, &owner->m_texture, 0);
            if (hr == 0u) break;

            if (hr == D3DERR_OUTOFVIDEOMEMORY)
            {
                CADR_DATA* root = VID::lastUse;
                if (root == 0) break;
                const unsigned int initialBytes = VID::useTextureMemory;
                while (root != 0)
                {
                    if (initialBytes-VID::useTextureMemory >= 1000000u) break;
                    root->lruPrevious->ReleaseVideoMemory();
                    root = VID::lastUse;
                }
                continue;
            }

            if (effectiveFormat == D3DFMT_P8)
            {
                effectiveFormat = ((owner->m_flags&8u) != 0u)?D3DFMT_A8R8G8B8:D3DFMT_R5G6B5;
                continue;
            }
            if (effectiveFormat == D3DFMT_L8)
            {
                effectiveFormat = D3DFMT_A8R8G8B8;
                continue;
            }
            if (effectiveFormat == D3DFMT_DXT1)
            {
                effectiveFormat = D3DFMT_R5G6B5;
                continue;
            }
            if (effectiveFormat == D3DFMT_DXT3)
            {
                effectiveFormat = D3DFMT_A4R4G4B4;
                continue;
            }
            if (effectiveFormat == D3DFMT_R5G6B5)
            {
                effectiveFormat = D3DFMT_X1R5G5B5;
                continue;
            }
            if (effectiveFormat == D3DFMT_X1R5G5B5)
            {
                effectiveFormat = D3DFMT_A1R5G5B5;
                continue;
            }
            if (effectiveFormat == D3DFMT_R8G8B8)
            {
                effectiveFormat = D3DFMT_X8R8G8B8;
                continue;
            }
            if (effectiveFormat == D3DFMT_X8R8G8B8)
            {
                effectiveFormat = D3DFMT_A8R8G8B8;
                continue;
            }

            if (capsCondSizePow2 != 0u)
            {
                int powerOfTwoWidth = 1;
                int powerOfTwoHeight = 1;
                while (effectiveWidth>powerOfTwoWidth) powerOfTwoWidth += powerOfTwoWidth;
                while (effectiveHeight>powerOfTwoHeight) powerOfTwoHeight += powerOfTwoHeight;
                if (powerOfTwoWidth != effectiveWidth || powerOfTwoHeight != effectiveHeight)
                {
                    effectiveWidth = powerOfTwoWidth;
                    effectiveHeight = powerOfTwoHeight;
                    continue;
                }
            }
            break;
        }
    }

    owner->m_width = static_cast<unsigned int>(effectiveWidth);
    owner->m_height = static_cast<unsigned int>(effectiveHeight);
    owner->m_format = effectiveFormat;

    if (hr != 0u || owner->m_texture == 0) return;
    const unsigned int bytes = TextureByteCount(effectiveFormat, static_cast<unsigned int>(effectiveWidth), static_cast<unsigned int>(effectiveHeight));
    memoryInUse = memoryInUse+bytes;
}

IDirect3DSurface9* TEXTURE::GetSurface()
{
    TEXTURE* const owner = this;
    if (owner->m_surface == 0)
    {
        IDirect3DTexture9* const texture = owner->m_texture;
        (void) texture->GetSurfaceLevel(0u, &owner->m_surface);
    }
    return owner->m_surface;
}

unsigned char* TEXTURE::Lock(int* pitch, RECT* rect) const
{
    const TEXTURE* const owner = this;
    const unsigned int mode = owner->m_flags&7u;
    RECT* const lockRect = rect;

    if (mode == 4u && owner->m_format == D3DFMT_D16)
    {
        *pitch = BitsToSigned32(owner->m_width*2u);
        if (lockRect)
        {
            const unsigned int top = static_cast<unsigned int>(lockRect->top);
            const unsigned int left = static_cast<unsigned int>(lockRect->left);
            const unsigned int offset = (top*owner->m_width+left)*2u;
            return owner->m_cpuData+offset;
        }
        return owner->m_cpuData;
    }

    D3DLOCKED_RECT locked;
    HRESULT hr;
    if (mode != 2u && mode != 3u)
    {
        IDirect3DTexture9* const texture = owner->m_texture;
        hr = (texture->LockRect(
            0u, &locked, rect, 0u));
    }
    else
    {
        const unsigned int width = owner->m_width;
        const unsigned int height = owner->m_height;
        unsigned int lockWidth = width, lockHeight = height;
        if (lockRect)
        {
            if (lockRect->left == 999999)
            {
                lockWidth = 4u;
                lockHeight = 4u;
                while (BitsToSigned32(width)>BitsToSigned32(lockWidth)) lockWidth += lockWidth;
                while (BitsToSigned32(height)>BitsToSigned32(lockHeight)) lockHeight += lockHeight;
                lockRect->left = 0;
            }
            else
            {
                lockWidth = static_cast<unsigned int>(lockRect->right);
                lockHeight = static_cast<unsigned int>(lockRect->bottom);
            }
        }

        hr = (Graph->device->CreateOffscreenPlainSurface(
            lockWidth, lockHeight, owner->m_format, D3DPOOL_SYSTEMMEM,
            &stagingSurface, 0));
        if (hr<0)
        {
            hr = (Graph->device->CreateTexture(
                lockWidth, lockHeight, 1u, 0u, owner->m_format, D3DPOOL_SYSTEMMEM,
                &stagingTexture, 0));
            if (hr<0)
            {
                STRING subject("surface in system memory for lock surface in video memory");
                if (Error)
                {
                    STRING object("TEXTURE");
                    Error->Error(object, E_CREATE, subject, hr);
                }
                return 0;
            }
            IDirect3DTexture9* const texture = stagingTexture;
            (void) texture->GetSurfaceLevel(0u,
                &stagingSurface);
        }

        if (lockRect)
        {
            stagingRect.left = lockRect->left;
            stagingRect.top = lockRect->top;
            stagingRect.right = lockRect->right;
            stagingRect.bottom = lockRect->bottom;
        }
        else
        {
            stagingRect.left = 0;
            stagingRect.top = 0;
            stagingRect.right = BitsToSigned32(owner->m_width);
            stagingRect.bottom = BitsToSigned32(owner->m_height);
        }
        IDirect3DSurface9* const surface = stagingSurface;
        hr = (surface->LockRect(
            &locked,
            &stagingRect, D3DLOCK_NO_DIRTY_UPDATE));
    }

    if (hr<0)
    {
        STRING subject(StringGlobals::EmptyText());
        if (Error)
        {
            STRING object("TEXTURE");
            Error->Error(object, E_LOCK, subject, hr);
        }
        return 0;
    }
    *pitch = locked.Pitch;
    return static_cast<unsigned char*>(locked.pBits);
}

void TEXTURE::UnLock()
{
    TEXTURE* const owner = this;
    if (owner->m_texture == 0) return;
    const unsigned int mode = owner->m_flags&7u;
    if ((mode == 2u || mode == 3u) && stagingSurface != 0)
    {
        (void) stagingSurface->UnlockRect();
        POINT point = {0, 0};
        IDirect3DSurface9* const dest = GetSurface();
        IDirect3DDevice9* const device = Graph->device;
        const HRESULT hr = device->UpdateSurface(stagingSurface, &stagingRect, dest, &point);
        if (hr<0)
        {
            STRING subject("system memory texture to video for unlock");
            if (Error)
            {
                STRING object("TEXTURE");
                Error->Error(object, E_COPY, subject, hr);
            }
        }
        IDirect3DSurface9* const surface = stagingSurface;
        if (surface)
        {
            (void) surface->Release();
            stagingSurface = 0;
        }
        IDirect3DTexture9* const texture = stagingTexture;
        if (texture)
        {
            (void) texture->Release();
            stagingTexture = 0;
        }
        return;
    }
    IDirect3DTexture9* const texture = owner->m_texture;
    (void) texture->UnlockRect(0u);
}

void TEXTURE::SetTextureToDevice(int stage)
{
    const TEXTURE* const owner = this;
    IDirect3DDevice9* const device = Graph->device;
    HRESULT hr = (device->SetTexture(
        static_cast<unsigned int>(stage), owner->m_texture));
    if (hr != 0u)
    {
        STRING subject(StringGlobals::EmptyText());
        if (Error)
        {
            STRING object("TEXTURE");
            Error->Error(object, E_SET, subject, hr);
        }
    }
    if (owner->m_format == D3DFMT_P8)
    {
        hr = (Graph->device->SetCurrentTexturePalette(owner->m_paletteIndex));
        if (hr != 0u)
        {
            STRING subject("palette");
            if (Error)
            {
                STRING object("TEXTURE");
                Error->Error(object, E_SET, subject, hr);
            }
        }
    }
}

struct TextureVertex {
    float x, y, z, rhw;
    unsigned int diffuse, specular;
    float u, v;
};

int TEXTURE::MemoryInUse()
{
    return memoryInUse;
}

int TEXTURE::SizeX() const
{
    return BitsToSigned32(m_width);
}

int TEXTURE::SizeY() const
{
    return BitsToSigned32(m_height);
}

void TEXTURE::SetPalette(PALETTEENTRY* entries)
{
    if (m_texture == 0)
    {
        STRING subject("palette for non initialized texture");
        if (Error)
        {
            STRING object("TEXTURE");
            Error->Error(object, E_SET, subject, 0UL);
        }
        return;
    }
    if (m_format != D3DFMT_P8)
    {
        STRING subject("palette for non palette texture");
        if (Error)
        {
            STRING object("TEXTURE");
            Error->Error(object, E_SET, subject, 0UL);
        }
        return;
    }
    const HRESULT hr = Graph->device->SetPaletteEntries(noPalettes, entries);
    if (hr<0)
    {
        STRING subject("palettes");
        if (Error)
        {
            STRING object("TEXTURE");
            Error->Error(object, E_SET, subject, hr);
        }
    }
    else
    {
        m_paletteIndex = noPalettes;
        ++noPalettes;
    }
}

void TEXTURE::SetPalette(COLOR* colors)
{
    PALETTEENTRY entries[256];
    for (int i = 0; i<256; ++i)
    {
        entries[i].peFlags = static_cast<BYTE>(colors[i].value>>24);
        entries[i].peRed = static_cast<BYTE>(colors[i].value>>16);
        entries[i].peGreen = static_cast<BYTE>(colors[i].value>>8);
        entries[i].peBlue = static_cast<BYTE>(colors[i].value);
    }
    SetPalette(entries);
}

long TEXTURE::CopyFromSameSurface(IDirect3DSurface9* sourceSurface, RECT* sourceRect, RECT* destRect)
{
    int destPitch;
    unsigned int* dest = reinterpret_cast<unsigned int*>(Lock(&destPitch, destRect));
    D3DLOCKED_RECT sourceLock;
    const HRESULT hr = sourceSurface->LockRect(&sourceLock, sourceRect, 0u);
    if (hr != 0)
    {
        STRING subject("for CopyFromSameSurface");
        if (Error)
        {
            STRING object("TEXTURE");
            Error->Error(object, E_OPEN, subject, hr);
        }
    }
    else if (dest)
    {
        const int width = sourceRect->right-sourceRect->left;
        const int height = sourceRect->bottom-sourceRect->top;
        const int destStep = destPitch/4-width;
        const int sourceStep = sourceLock.Pitch/4-width;
        const unsigned int* source = static_cast<const unsigned int*>(sourceLock.pBits);
        for (int y = 0; y<height; ++y)
        {
            for (int x = 0; x<width; ++x) *dest++ = *source++;
            dest += destStep;
            source += sourceStep;
        }
    }
    UnLock();
    (void)sourceSurface->UnlockRect();
    return static_cast<long>(hr);
}

void TEXTURE::Draw(unsigned long fvf, const void* vertices, unsigned int stride)
{
    HRESULT hr = Graph->device->SetTexture(0u, m_texture);
    if (hr != 0)
    {
        STRING subject(StringGlobals::EmptyText());
        if (Error)
        {
            STRING object("TEXTURE");
            Error->Error(object, E_SET, subject, hr);
        }
    }
    if (m_format == D3DFMT_P8)
    {
        hr = Graph->device->SetCurrentTexturePalette(m_paletteIndex);
        if (hr != 0)
        {
            STRING subject("palette");
            if (Error)
            {
                STRING object("TEXTURE");
                Error->Error(object, E_SET, subject, hr);
            }
        }
    }
    (void)Graph->device->SetFVF(fvf);
    hr = Graph->device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2u, vertices, stride);
    if (hr != 0)
    {
        STRING subject("DrawPrimitiveUP");
        if (Error)
        {
            STRING object("TEXTURE");
            Error->Error(object, E_ERROR, subject, hr);
        }
    }
}

void TEXTURE::Draw(float topZ, float bottomZ, RECT* destRect, RECT* sourceRect, const GAMMA& gamma)
{
    const unsigned int diffuse = ~gamma.modulate;
    const unsigned int specular = gamma.add;
    const float left = static_cast<float>(static_cast<double>(sourceRect->left)+0.5f);
    const float top = static_cast<float>(static_cast<double>(sourceRect->top)+0.5f);
    const float right = static_cast<float>(static_cast<double>(sourceRect->right)+0.5f);
    const float bottom = static_cast<float>(static_cast<double>(sourceRect->bottom)+0.5f);
    const float u0 = static_cast<float>(left/static_cast<double>(SizeX()));
    const float v0 = static_cast<float>(top/static_cast<double>(SizeY()));
    const float u1 = static_cast<float>(right/static_cast<double>(SizeX()));
    const float v1 = static_cast<float>(bottom/static_cast<double>(SizeY()));
    const TextureVertex vertices[4] = {
        {static_cast<float>(destRect->left), static_cast<float>(destRect->top), topZ, 1.0f, diffuse, specular, u0, v0},
        {static_cast<float>(destRect->right), static_cast<float>(destRect->top), topZ, 1.0f, diffuse, specular, u1, v0},
        {static_cast<float>(destRect->right), static_cast<float>(destRect->bottom), bottomZ, 1.0f, diffuse, specular, u1, v1},
        {static_cast<float>(destRect->left), static_cast<float>(destRect->bottom), bottomZ, 1.0f, diffuse, specular, u0, v1}
    };
    SetTextureToDevice(0);
    (void)Graph->device->SetRenderState(D3DRS_SPECULARENABLE, gamma.add?1u:0u);
    (void)Graph->device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1);
    const bool sameSize =
        static_cast<unsigned int>(destRect->right)-static_cast<unsigned int>(destRect->left) ==
        static_cast<unsigned int>(sourceRect->right)-static_cast<unsigned int>(sourceRect->left) &&
        static_cast<unsigned int>(destRect->bottom)-static_cast<unsigned int>(destRect->top) ==
        static_cast<unsigned int>(sourceRect->bottom)-static_cast<unsigned int>(sourceRect->top);
    const unsigned int filter = sameSize?D3DTEXF_POINT:D3DTEXF_LINEAR;
    (void)Graph->device->SetSamplerState(0u, D3DSAMP_MINFILTER, filter);
    (void)Graph->device->SetSamplerState(0u, D3DSAMP_MAGFILTER, filter);
    const HRESULT hr = Graph->device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2u, vertices, sizeof(TextureVertex));
    if (hr != 0)
    {
        STRING subject("DrawPrimitiveUP");
        if (Error)
        {
            STRING object("TEXTURE");
            Error->Error(object, E_ERROR, subject, hr);
        }
    }
}

long TEXTURE::CopyFromSurface(IDirect3DSurface9* sourceSurface, RECT* sourceRect, POINT* destPoint)
{
    const TEXTURE* const owner = this;
    IDirect3DTexture9* const texture = owner->m_texture;
    IDirect3DSurface9* destSurface;
    HRESULT hr = (texture->GetSurfaceLevel(0u, &destSurface));
    if (hr != 0u)
    {
        STRING subject("surface for copy");
        if (Error)
        {
            STRING object("TEXTURE");
            Error->Error(object, E_GET, subject, hr);
        }
        return static_cast<long>(hr);
    }

    RECT localDest;
    RECT* destRect = 0;
    if (destPoint)
    {
        const int x = destPoint->x;
        const int y = destPoint->y;
        localDest.left = x;
        localDest.top = y;
        localDest.right = BitsToSigned32(static_cast<unsigned int>(sourceRect->right)-
            static_cast<unsigned int>(sourceRect->left)+static_cast<unsigned int>(x));
        localDest.bottom = BitsToSigned32(static_cast<unsigned int>(sourceRect->bottom)-
            static_cast<unsigned int>(sourceRect->top)+static_cast<unsigned int>(y));
        destRect = &localDest;
    }

    hr = D3DXLoadSurfaceFromSurface(destSurface, 0, destRect, sourceSurface, 0, sourceRect, 1u, 0u);
    if (hr != 0u)
    {
        STRING subject("from surface");
        if (Error)
        {
            STRING object("TEXTURE");
            Error->Error(object, E_COPY, subject, hr);
        }
    }
    (void) destSurface->Release();
    return static_cast<long>(hr);
}

long TEXTURE::Draw(RECT* destRect, RECT* sourceRect, const GAMMA& gamma)
{
    if (!m_texture && !m_cpuData) return E_FAIL;

    RECT* sampleRect = sourceRect;
    if (!sampleRect)
    {
        static RECT fullTextureRect = {0, 0, SizeX(), SizeY()};
        sampleRect = &fullTextureRect;
    }

    const double width = SizeX();
    const double height = SizeY();
    const float left = static_cast<float>(static_cast<double>(sampleRect->left)+0.5);
    const float top = static_cast<float>(static_cast<double>(sampleRect->top)+0.5);
    const float right = static_cast<float>(static_cast<double>(sampleRect->right)+0.5);
    const float bottom = static_cast<float>(static_cast<double>(sampleRect->bottom)+0.5);
    const float u0 = static_cast<float>(left/width);
    const float v0 = static_cast<float>(top/height);
    const float u1 = static_cast<float>(right/width);
    const float v1 = static_cast<float>(bottom/height);
    const unsigned int diffuse = ~gamma.modulate;
    const unsigned int specular = gamma.add;

    const TextureVertex vertices[4] = {
        {static_cast<float>(destRect->left), static_cast<float>(destRect->top), 0.99999988079071044921875f, 1.0f, diffuse, specular, u0, v0},
        {static_cast<float>(destRect->right), static_cast<float>(destRect->top), 0.99999988079071044921875f, 1.0f, diffuse, specular, u1, v0},
        {static_cast<float>(destRect->right), static_cast<float>(destRect->bottom), 0.99999988079071044921875f, 1.0f, diffuse, specular, u1, v1},
        {static_cast<float>(destRect->left), static_cast<float>(destRect->bottom), 0.99999988079071044921875f, 1.0f, diffuse, specular, u0, v1}
    };

    SetTextureToDevice(0);
    (void) Graph->device->SetRenderState(D3DRS_SPECULARENABLE, gamma.add?1u:0u);
    const bool sameSize =
    static_cast<unsigned int>(destRect->right)-static_cast<unsigned int>(destRect->left) ==
    static_cast<unsigned int>(sampleRect->right)-static_cast<unsigned int>(sampleRect->left) &&
    static_cast<unsigned int>(destRect->bottom)-static_cast<unsigned int>(destRect->top) ==
    static_cast<unsigned int>(sampleRect->bottom)-static_cast<unsigned int>(sampleRect->top);
    const unsigned int filter = sameSize?D3DTEXF_POINT:D3DTEXF_LINEAR;
    (void) Graph->device->SetSamplerState(0u, D3DSAMP_MINFILTER, filter);
    (void) Graph->device->SetSamplerState(0u, D3DSAMP_MAGFILTER, filter);
    (void) Graph->device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1);
    const HRESULT hr = (Graph->device->DrawPrimitiveUP(
        D3DPT_TRIANGLEFAN, 2u, vertices, sizeof(TextureVertex)));
    if (hr != 0u)
    {
        STRING subject("DrawPrimitiveUP");
        if (Error)
        {
            STRING object("TEXTURE");
            Error->Error(object, E_ERROR, subject, hr);
        }
    }
    return static_cast<long>(hr);
}

TEXTURE::~TEXTURE() noexcept(false)
{
    ReleaseVideoMemory();
}

void TEXTURE::ReleaseVideoMemory()
{
    TEXTURE* const owner = this;

    IDirect3DSurface9* const surface = owner->m_surface;
    if (surface != 0)
    {
        (void) surface->Release();
        owner->m_surface = 0;
    }

    if (owner->m_texture != 0)
    {
        IDirect3DDevice9* const device = Graph->device;
        (void) device->SetTexture(0u, 0);

        const unsigned int remainingReferences = owner->m_texture->Release();
        if (remainingReferences != 0u)
        {
            STRING subject("Release count !=0");
            if (Error)
            {
                STRING object("TEXTURE");
                Error->Error(object, E_ERROR, subject, remainingReferences);
            }
        }
        else
        {
            const D3DFORMAT format = owner->m_format;
            const unsigned int bytes = TextureByteCount(format, owner->m_width, owner->m_height);

            memoryInUse = memoryInUse-bytes;
        }

    }

    if (owner->m_cpuData != 0)
    {
        delete[] owner->m_cpuData;
    }
    owner->m_cpuData = 0;
    owner->m_texture = 0;
}

TEXTURE::TEXTURE(const STRING& sourceFile, unsigned long colorKey, D3DXIMAGE_INFO* imageInfo)
{
    TEXTURE* const owner = this;
    owner->m_flags = 0u;
    owner->m_texture = 0;
    owner->m_surface = 0;
    owner->m_cpuData = 0;
    owner->m_height = 4u;
    owner->m_width = 4u;

    HRESULT hr = D3DXCreateTextureFromFileExA(Graph->device, sourceFile.m_value,
        static_cast<unsigned int>(-1), static_cast<unsigned int>(-1), 1u, 0u,
        D3DFMT_UNKNOWN, D3DPOOL_DEFAULT, 1u, 1u, colorKey, imageInfo, 0, &owner->m_texture);
    while (hr == D3DERR_OUTOFVIDEOMEMORY)
    {
        CADR_DATA* root = VID::lastUse;
        if (root == 0) break;
        const unsigned int initialBytes = VID::useTextureMemory;
        while (root != 0)
        {
            if (initialBytes-VID::useTextureMemory >= 1000000u) break;
            root->lruPrevious->ReleaseVideoMemory();
            root = VID::lastUse;
        }
        hr = D3DXCreateTextureFromFileExA(Graph->device, sourceFile.m_value,
            static_cast<unsigned int>(-1), static_cast<unsigned int>(-1), 1u, 0u,
            D3DFMT_UNKNOWN, D3DPOOL_DEFAULT, 1u, 1u, colorKey, imageInfo, 0, &owner->m_texture);
    }

    if (hr != 0u)
    {
        STRING subject = Printf("texture from file '%s'", sourceFile.m_value);
        if (Error)
        {
            STRING objectFormat("TEXTURE");
            Error->Error(objectFormat, E_CREATE, subject, hr);
        }
        return;
    }

    IDirect3DTexture9* const texture = owner->m_texture;
    D3DSURFACE_DESC desc;
    (void) texture->GetLevelDesc(0u, &desc);
    owner->m_width = desc.Width;
    owner->m_height = desc.Height;
    owner->m_format = desc.Format;
    if (desc.Format == D3DFMT_A8R8G8B8 || desc.Format == D3DFMT_A4R4G4B4 || desc.Format == D3DFMT_A1R5G5B5 ||
        desc.Format == D3DFMT_DXT3 || desc.Format == D3DFMT_DXT5)
    owner->m_flags |= 8u;
    memoryInUse = memoryInUse+
        TextureByteCount(desc.Format, desc.Width, desc.Height);
    return;
}

