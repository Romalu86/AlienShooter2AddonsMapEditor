#pragma once
#include "mylib.h"
#include "basetype.h"
#include "sprite.h"

class VID;
struct HWND__;
class COLOR
{
public:
    COLOR()
{
}
    COLOR(const COLOR& source);
    COLOR(int red, int green, int blue);
    COLOR(int alpha, int red, int green, int blue);
    COLOR(const GAMMA& gamma, const COLOR& baseColor);

    unsigned int value;
};

class REGION : public SPRITE
{
    friend class SPRITE;
public:
    enum { VidConversionCount = 6 };

    REGION(VID* vid, const VECTOR& position, ANGLE direction, SPRITE* parent);
    virtual ~REGION();
    virtual int Action(int actionCode, int firstParameter, int secondParameter, int thirdParameter);
    virtual void Draw();
    virtual void DrawSecondaryInfo();

    VECTOR2 ScreenLeftTop() const;
    VECTOR2 ScreenRightBottom() const;
    static VID* ConvertVid(VID* sourceVid, const VECTOR& position);
    void SetFogParameters(int fogStart, int fogEnd, COLOR fogColor);
    int DialogRegionProperty(HWND__*, unsigned int, unsigned int, long);

    void SetSize(float width, float height) { m_halfWidth=width; m_halfHeight=height; }
    float HalfWidth() const
    {
        return m_halfWidth;
    }
    float HalfHeight() const
    {
        return m_halfHeight;
    }

private:
    int m_fogPhase;
    unsigned int m_lastFogPhase;
    unsigned short* m_fogRamp;
    unsigned int m_fogFlags;
    int m_fogEnd;
    int m_fogStart;
    COLOR m_fogColor;
    unsigned int m_084;
    float m_halfWidth;
    float m_halfHeight;
    VID* m_regionVid;
    unsigned int m_094;
    VID* m_sourceVid[VidConversionCount];
    VID* m_targetVid[VidConversionCount];
};

inline COLOR::COLOR(int red, int green, int blue)
{
    if (red < 0) red = 0;
    if (red > 255) red = 255;
    if (green < 0) green = 0;
    if (green > 255) green = 255;
    if (blue < 0) blue = 0;
    if (blue > 255) blue = 255;
    value = 0xFF000000u | (static_cast<unsigned int>(red) << 16) |
            (static_cast<unsigned int>(green) << 8) | static_cast<unsigned int>(blue);
}

inline COLOR::COLOR(int alpha, int red, int green, int blue)
{
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;
    if (red < 0) red = 0;
    if (red > 255) red = 255;
    if (green < 0) green = 0;
    if (green > 255) green = 255;
    if (blue < 0) blue = 0;
    if (blue > 255) blue = 255;
    value = (static_cast<unsigned int>(alpha) << 24) |
            (static_cast<unsigned int>(red) << 16) |
            (static_cast<unsigned int>(green) << 8) | static_cast<unsigned int>(blue);
}

inline COLOR::COLOR(const COLOR& source)
    : value(source.value)
{
}

inline COLOR::COLOR(const GAMMA& gamma, const COLOR& baseColor)
{
    if (gamma.modulate == 0u && gamma.add == 0u)
    {
        value = baseColor.value;
        return;
    }

    const unsigned int inverse = ~gamma.modulate;
    const unsigned int add = gamma.add;
    const unsigned int base = baseColor.value;
    int blue = ((static_cast<int>(inverse&0xFFu)+1)*static_cast<int>(base&0xFFu)>>8)+static_cast<int>(add&0xFFu);
    int green = ((static_cast<int>((inverse>>8)&0xFFu)+1)*static_cast<int>((base>>8)&0xFFu)>>8)+static_cast<int>((add>>8)&0xFFu);
    int red = ((static_cast<int>((inverse>>16)&0xFFu)+1)*static_cast<int>((base>>16)&0xFFu)>>8)+static_cast<int>((add>>16)&0xFFu);
    if (blue < 0) blue = 0;
    if (blue > 255) blue = 255;
    if (green < 0) green = 0;
    if (green > 255) green = 255;
    if (red < 0) red = 0;
    if (red > 255) red = 255;
    value = 0xFF000000u|(static_cast<unsigned int>(red)<<16)|
        (static_cast<unsigned int>(green)<<8)|static_cast<unsigned int>(blue);
}
