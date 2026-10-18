#pragma once
#include "basetype.h"
#include "mylib.h"
#define INPUT WIN32_INPUT
#include <d3dx9.h>
#undef INPUT
struct IDirect3DDevice9;
struct IDirect3DTexture9;
struct IDirect3DSurface9;
class COLOR;
class RESOURCE;

class TEXTURE
{
public:
    static int memoryInUse;
    static int capsSquare;
    static int capsDynamic;
    static int capsPalette;
    static int capsBump;
    static int capsDXT;
    static int capsAlphaPalette;
    static int capsCondSizePow2;
    static int capsSizePower2;
    static int capsMaxSizeX;
    static int capsMaxSizeY;

    unsigned char* m_cpuData;
    IDirect3DTexture9* m_texture;
    D3DFORMAT     m_format;
    unsigned int  m_width;
    unsigned int  m_height;
    unsigned int  m_paletteIndex;
    unsigned int  m_flags;
    IDirect3DSurface9* m_surface;

    TEXTURE();
    TEXTURE(const STRING& sourceFile, unsigned long colorKey, D3DXIMAGE_INFO* imageInfo);
    TEXTURE(int width, int height, D3DFORMAT format, unsigned long flags);
    TEXTURE(int width, int height, D3DFORMAT format, unsigned long flags,
        COLOR* palette, RESOURCE* resource);
    virtual ~TEXTURE() noexcept(false);
    void ReleaseVideoMemory();
    static void SetDeviceCaps(IDirect3DDevice9* device);
    void Create(int width, int height, D3DFORMAT format, unsigned long flags);
    IDirect3DSurface9* GetSurface();
    unsigned char* Lock(int* pitch, RECT* rect) const;
    void UnLock();
    void SetTextureToDevice(int stage);
    static int MemoryInUse();
    int SizeX() const;
    int SizeY() const;
    void SetPalette(PALETTEENTRY* palette);
    void SetPalette(COLOR* palette);
    void Draw(unsigned long fvf, const void* vertices, unsigned int stride);
    void Draw(float topZ, float bottomZ, RECT* destRect, RECT* sourceRect, const GAMMA& gamma);
    long Draw(RECT* destRect, RECT* sourceRect, const GAMMA& gamma);
    long CopyFromSurface(IDirect3DSurface9* sourceSurface, RECT* sourceRect, POINT* destPoint);
    long CopyFromSameSurface(IDirect3DSurface9* sourceSurface, RECT* sourceRect, RECT* destRect);
private:
    static int noPalettes;
};
