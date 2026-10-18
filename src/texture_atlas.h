#pragma once
#include "texture.h"
#include "myerror.h"

class VID_SURFACE;
class SPRITE;
struct CADR_DATA;

struct ATLAS_VERTEX
{
    VECTOR position;
    unsigned int diffuse;
    unsigned int specular;
    VECTOR2 uv;
};

class TEXTURE_ATLAS
{
public:
    enum { MaxVids = 16, MaxFrames = 8192, MaxQuads = 8192, AtlasSize = 512 };

    TEXTURE_ATLAS();
    ~TEXTURE_ATLAS() noexcept(false);
    int AddVid(VID_SURFACE* vid);
    void ReleaseVideoMemory();
    void Clear();
    int AddSprite(SPRITE* sprite);
    void DrawArrayObjects();
    TEXTURE* GetTexture();
    int IsAllocated() const;

    TEXTURE texture;
    int noVid;
    VID_SURFACE* vids[MaxVids];
    ATLAS_VERTEX* vertices;
    int noVertex;

private:
    void Error(TYPE_ERROR type, const STRING& subject, unsigned long code) const;
    static int Intersects(const CADR_DATA& frame, int x, int y, int width, int height);
    static unsigned short* indices;
};
