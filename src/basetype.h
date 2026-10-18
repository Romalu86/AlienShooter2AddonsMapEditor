#pragma once
#include <cmath>

class STREAM;

struct VECTOR2
{
    float x;
    float y;

    VECTOR2()
    {
    }
    VECTOR2(int xValue, int yValue) : x(static_cast<float>(xValue)), y(static_cast<float>(yValue))
    {
    }
    VECTOR2(float xValue, float yValue) : x(xValue), y(yValue)
    {
    }
    VECTOR2(const VECTOR2& value) : x(value.x), y(value.y)
    {
    }
    VECTOR2& operator=(const VECTOR2& value) = default;

    VECTOR2 operator*(const VECTOR2& value) const;
    VECTOR2 operator*(float scale) const;
    VECTOR2 operator-(const VECTOR2& value) const;
    VECTOR2 operator+(const VECTOR2& value) const;
    VECTOR2 operator/(float divisor) const;
    float NearLength() const;
    float Length() const;
    void Read(STREAM* stream);
    void Save(STREAM* stream) const;
    int operator<(const VECTOR2& value) const;
    int operator>(const VECTOR2& value) const;
    int operator>=(const VECTOR2& value) const;
    int operator<=(const VECTOR2& value) const;
    int operator!=(const VECTOR2& value) const
    {
        return x != value.x || y != value.y;
    }
};

struct VECTOR
{
    float x;
    float y;
    float z;

    VECTOR()
    {
    }
    VECTOR(int xValue,int yValue,int zValue);
    VECTOR(float xValue,float yValue,float zValue);
    VECTOR(const VECTOR& value);
    VECTOR& operator=(const VECTOR& value) = default;
    VECTOR(const VECTOR2& xyValue,float zValue);

    VECTOR operator*(float scale) const;
    VECTOR operator*(const VECTOR& value) const;
    VECTOR operator-(const VECTOR& value) const;
    VECTOR operator+(const VECTOR& value) const;
    VECTOR operator/(float divisor) const;
    VECTOR& operator+=(const VECTOR& value);
    VECTOR& operator/=(float divisor);
    VECTOR2& XY();
    const VECTOR2& XY() const;
    VECTOR2 ViewCoor() const;
    float NearLength() const;
    void Read(STREAM* stream);
    void Save(STREAM* stream) const;
    int operator==(const VECTOR& value) const
    {
        return x == value.x && y == value.y && z == value.z;
    }
    int operator!=(const VECTOR& value) const
    {
        return !(*this == value);
    }
};

class RGB16
{
public:
    static void SetChannelMasks(bool rgb565);
    static unsigned short Pack(unsigned int color);
    static unsigned int Expand(unsigned short color);

private:
    static unsigned int rMask;
    static unsigned int gMask;
    static int rShift;
    static int gShift;
};

class GAMMA
{
public:
    unsigned int modulate;
    unsigned int add;

    enum GAMMA_CREATE : int;

    GAMMA() : modulate(0u), add(0u)
    {
    }
    GAMMA(const GAMMA& source) : modulate(source.modulate), add(source.add)
    {
    }
    GAMMA(GAMMA_CREATE create,unsigned long packed);
    GAMMA(int red,int green,int blue);
    GAMMA(int alpha,int red,int green,int blue);
    GAMMA(GAMMA left,GAMMA right);
    const GAMMA& operator=(const GAMMA& source);
    const GAMMA& SetAlpha(int value);
    const GAMMA& SetRed(int value);
    const GAMMA& SetGreen(int value);
    const GAMMA& SetBlue(int value);
    int Alpha() const;
    int Red() const;
    int Green() const;
    int Blue() const;
    unsigned int EncodeToDword() const;
    void Read(STREAM* stream);
    void Save(STREAM* stream) const;
};

class ANGLE
{
public:
    unsigned char value;

    ANGLE()
    {
    }
    explicit ANGLE(unsigned char angleValue) : value(angleValue)
    {
    }
    ANGLE(const ANGLE& angleValue);
    explicit ANGLE(const VECTOR2& vectorValue);
    ANGLE(float x, float y, float* length);
    ANGLE(const VECTOR2& vectorValue, float* length);

    const ANGLE& operator=(const ANGLE& angleValue) { value=angleValue.value; return *this; }
    const ANGLE operator+(const ANGLE& angleValue) const;
    const ANGLE operator-(const ANGLE& angleValue) const;
    const ANGLE GetInversed() const;
    void Read(STREAM* stream);
    void Save(STREAM* stream) const;
    float Sin() const;
    float Cos() const;
    float SinY() const;
    float CosY() const;
    static void InitializeYTables();
    VECTOR2 RotateY(const VECTOR2& vectorValue) const;

private:
    static float SinTable[256];
    static float CosTable[256];
    static float SinYTable[256];
    static float CosYTable[256];
};

extern float FSin[256];

VECTOR2 VAbs(const VECTOR2& value);

ANGLE Decart2Polar(float x,float y);
ANGLE Decart2Polar(float x,float y,float* length);

#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <io.h>

#include <cstring>
#include <cctype>
#include <ctime>

static inline unsigned long AddressToUIntPtr(unsigned int addressBits)
{
    return static_cast<unsigned long>(addressBits);
}
static inline unsigned int PtrToU32(const void* pointer){
 return reinterpret_cast<unsigned int>(pointer);
}
static inline unsigned char* U32ToPtr(unsigned int addressBits)
{
    return reinterpret_cast<unsigned char*>(AddressToUIntPtr(addressBits));
}

namespace StringGlobals
{
extern char EmptyTextCharacter;
const char* EmptyText();

char* EmptySentinel();
}

static inline unsigned int FloatBitsU32(float value)
{
    union Bits
    {
        float f;
        unsigned int u;
    }
    bits;
    bits.f = value;
    return bits.u;
}
static inline void CopyBytes(void* destination, const void* source, unsigned int count)
{
    unsigned char* destinationBytes = static_cast<unsigned char*>(destination);
    const unsigned char* sourceBytes = static_cast<const unsigned char*>(source);
    for (unsigned int byteIndex = 0;byteIndex<count;++byteIndex)destinationBytes[byteIndex] = sourceBytes[byteIndex];
}

static inline unsigned int ReadU32(const void* source, unsigned int byteOffset)
{
    return *reinterpret_cast<const unsigned int*>(static_cast<const unsigned char*>(source)+byteOffset);
}
static inline unsigned short ReadU16(const void* source, unsigned int byteOffset)
{
    return *reinterpret_cast<const unsigned short*>(static_cast<const unsigned char*>(source)+byteOffset);
}

static inline unsigned char ReadU8(const void* source, unsigned int byteOffset)
{
    return *reinterpret_cast<const unsigned char*>(static_cast<const unsigned char*>(source)+byteOffset);
}

static inline unsigned int AbsDiffU8(unsigned char left, unsigned char right)
{
    unsigned int forwardDistance = static_cast<unsigned int>(static_cast<unsigned char>(left-right)), reverseDistance = static_cast<unsigned int>(static_cast<unsigned char>(right-left));
    return forwardDistance<reverseDistance?forwardDistance:reverseDistance;
}

static inline int BitsToSigned32(unsigned int value)
{
    union Bits
    {
        unsigned int u;
        int s;
    }
    bits;
    bits.u = value;
    return bits.s;
}
static inline int TruncateDoubleToInt32(double value)
{
    const double minimum = -9223372036854775808.0;
    const double maximum = 9223372036854775808.0;
    if (!(value >= minimum && value < maximum))
        return 0;
    const std::int64_t integer = static_cast<std::int64_t>(value);
    return BitsToSigned32(static_cast<std::uint32_t>(integer));
}
static inline int TruncateFloatToInt32(float value)
{
    return TruncateDoubleToInt32(static_cast<double>(value));
}
// CVTTSS2SI returns INT_MIN for unordered or out-of-range inputs.
static inline int TruncateFloatToInt32OrMin(float value)
{
    if (!(value >= -2147483648.0f && value < 2147483648.0f))
        return (-2147483647 - 1);
    return static_cast<int>(value);
}
static inline bool FloatEqualOrUnordered(float left, float right)
{
    return left == right || left != left || right != right;
}
static inline bool FloatLessOrUnordered(float left, float right)
{
    return !(left >= right);
}
static inline bool FloatGreaterOrdered(float left, float right)
{
    return left > right;
}
static inline unsigned int Signed32ToBits(int value)
{
    union Bits
    {
        int s;
        unsigned int u;
    }
    bits;
    bits.s = value;
    return bits.u;
}
static inline int AddSigned32Wrap(int left, int right)
{
    return BitsToSigned32(static_cast<unsigned int>(left)+static_cast<unsigned int>(right));
}
static inline int SubSigned32Wrap(int left, int right)
{
    return BitsToSigned32(static_cast<unsigned int>(left)-static_cast<unsigned int>(right));
}
static inline int MultiplySigned32Wrap(int left, int right)
{
    return BitsToSigned32(static_cast<unsigned int>(left)*static_cast<unsigned int>(right));
}


static inline int AbsInt(int value)
{
    return value<0?-value:value;
}
static inline float AbsFloat(float value)
{
    return value<0.0f?-value:value;
}
static inline int ClampByteInt(int value)
{
    return value<0?0:(value>255?255:value);
}

inline const GAMMA& GAMMA::operator = (const GAMMA& source)
{
    modulate = source.modulate;
    add = source.add;
    return *this;
}

inline VECTOR VECTOR::operator-(const VECTOR& value) const
{
    return VECTOR(x-value.x, y-value.y, z-value.z);
}

inline VECTOR VECTOR::operator*(float scale) const
{
    return VECTOR(scale*x, scale*y, scale*z);
}

inline VECTOR VECTOR::operator*(const VECTOR& value) const
{
    return VECTOR(x*value.x, y*value.y, z*value.z);
}

inline float VECTOR2::Length() const
{
    return static_cast<float>(sqrt(static_cast<double>(x*x+y*y)));
}

inline VECTOR::VECTOR(const VECTOR& vectorValue) : x(vectorValue.x), y(vectorValue.y), z(vectorValue.z)
{
}

inline VECTOR::VECTOR(int xValue, int yValue, int zValue)
    : x(static_cast<float>(xValue)), y(static_cast<float>(yValue)), z(static_cast<float>(zValue))
{
}

inline VECTOR VECTOR::operator+(const VECTOR& vectorValue) const
{
    return VECTOR(x+vectorValue.x, y+vectorValue.y, z+vectorValue.z);
}

inline VECTOR& VECTOR::operator+=(const VECTOR& value)
{
    x += value.x;
    y += value.y;
    z += value.z;
    return *this;
}

inline VECTOR& VECTOR::operator/=(float divisor)
{
    const float scale = 1.0f/divisor;
    x = scale*x;
    y = scale*y;
    z = scale*z;
    return *this;
}

inline VECTOR VECTOR::operator/(float divisor) const
{
    const float scale = 1.0f/divisor;
    return VECTOR(scale*x, scale*y, scale*z);
}

inline VECTOR2 VECTOR::ViewCoor() const
{
    return VECTOR2(x, y-z);
}

inline VECTOR2 VECTOR2::operator+(const VECTOR2& value) const
{
    return VECTOR2(x+value.x, y+value.y);
}

inline VECTOR2 VECTOR2::operator*(const VECTOR2& value) const
{
    return VECTOR2(x*value.x, y*value.y);
}

inline ANGLE::ANGLE(const ANGLE& angleValue)
{
    value = angleValue.value;
}

inline const ANGLE ANGLE::operator+(const ANGLE& angleValue) const
{
    return ANGLE(static_cast<unsigned char>(value+angleValue.value));
}

inline VECTOR::VECTOR(float xValue, float yValue, float zValue) : x(xValue), y(yValue), z(zValue)
{
}

inline VECTOR2 VECTOR2::operator-(const VECTOR2& vectorValue) const
{
    return VECTOR2(x-vectorValue.x, y-vectorValue.y);
}

inline float VECTOR2::NearLength() const
{
    const float absX = fabsf(x);
    const float absY = fabsf(y);
    const float axialScale = 0.961000025f;
    const float diagonalScale = (0.398000002f);
    return absX>absY ? absX*axialScale+absY*diagonalScale : absY*axialScale+absX*diagonalScale;
}

inline VECTOR2 VECTOR2::operator/(float divisor) const
{
    const float scale = (1.0f)/divisor;
    return VECTOR2(scale*x, scale*y);
}

inline VECTOR::VECTOR(const VECTOR2& xyValue, float zValue)
    : x(xyValue.x), y(xyValue.y), z(zValue)
{
}

inline int GAMMA::Alpha() const
{
    const unsigned int first=modulate>>24;
    return first ? -static_cast<int>(first) : static_cast<int>(add>>24);
}

inline int GAMMA::Red() const
{
    const unsigned int first = (modulate >> 16) & 0xFFu;
    return first ? -static_cast<int>(first) : static_cast<int>((add >> 16) & 0xFFu);
}

inline int GAMMA::Green() const
{
    const unsigned int first = (modulate >> 8) & 0xFFu;
    return first ? -static_cast<int>(first) : static_cast<int>((add >> 8) & 0xFFu);
}

inline int GAMMA::Blue() const
{
    const unsigned int first = modulate&0xFFu;
    return first? -static_cast<int>(first):static_cast<int>(add&0xFFu);
}

inline GAMMA::GAMMA(int red, int green, int blue) : modulate(0u), add(0u)
{
    SetRed(red);
    SetGreen(green);
    SetBlue(blue);
}

inline unsigned int GAMMA::EncodeToDword() const
{
    unsigned int encoded = (modulate >> 1) & 0x7F7F7F7Fu;
    for (unsigned int shift = 0; shift < 32; shift += 8)
        if ((add & (0xFFu << shift)) != 0u)
            encoded |= ((~add >> 1) & (0x7Fu << shift)) | (0x80u << shift);
    return encoded;
}

inline GAMMA::GAMMA(int alpha, int red, int green, int blue)
    : modulate(0u), add(0u)
{
    SetAlpha(alpha);
    SetRed(red);
    SetGreen(green);
    SetBlue(blue);
}

inline VECTOR2 VECTOR2::operator*(float scale) const
{
    return VECTOR2(x*scale, y*scale);
}

inline int VECTOR2::operator>(const VECTOR2& value) const
{
    return (x>value.x && y>value.y)?1:0;
}

inline VECTOR2& VECTOR::XY()
{
    return *reinterpret_cast<VECTOR2*>(this);
}

inline const VECTOR2& VECTOR::XY() const
{
    return *reinterpret_cast<const VECTOR2*>(this);
}

inline const ANGLE ANGLE::operator-(const ANGLE& otherAngle) const
{
    ANGLE differenceAngle;
    differenceAngle.value = static_cast<unsigned char>(value-otherAngle.value);
    return differenceAngle;
}

inline int VECTOR2::operator>=(const VECTOR2& vectorValue) const
{ return (x>=vectorValue.x && y>=vectorValue.y)?1:0; }

inline int VECTOR2::operator<=(const VECTOR2& vectorValue) const
{ return (x<=vectorValue.x && y<=vectorValue.y)?1:0; }

inline const ANGLE ANGLE::GetInversed() const
{
    return ANGLE(static_cast<unsigned char>(value+128u));
}

inline int VECTOR2::operator<(const VECTOR2& rhs) const
{
    return (x<rhs.x && y<rhs.y)?1:0;
}

inline float VECTOR::NearLength() const
{
    const float axialScale = 0.961000025f;
    const float diagonalScale = (0.398000002f);
    const float absoluteX = AbsFloat(x);
    const float absoluteY = AbsFloat(y);
    const float absoluteZ = AbsFloat(z);
    const float horizontalLength = (absoluteX>absoluteY)?(absoluteX*axialScale+absoluteY*diagonalScale):(absoluteY*axialScale+absoluteX*diagonalScale);
    return (horizontalLength>absoluteZ)?(horizontalLength*axialScale+absoluteZ*diagonalScale):(absoluteZ*axialScale+horizontalLength*diagonalScale);
}

inline GAMMA::GAMMA(GAMMA_CREATE create, unsigned long packed)
{
    (void)create;
    modulate = 0u;
    add = 0u;
    const unsigned int encoded = static_cast<unsigned int>(packed);

    const unsigned int magnitudeMasks[4] = {0x0000007Fu, 0x00007F80u, 0x007F8000u, 0xFF800000u};
    const unsigned int signMasks[4] = {0x00000080u, 0x00008000u, 0x00800000u, 0x80000000u};
    for (int channel = 0;channel<4;++channel)
    {
        if ((encoded&signMasks[channel]) != 0u)
            add |= ((~encoded)&magnitudeMasks[channel])<<1;
        else
            modulate |= (encoded&magnitudeMasks[channel])<<1;
    }
}

inline GAMMA::GAMMA(GAMMA left, GAMMA right)
{
    const unsigned char* leftBytes = reinterpret_cast<const unsigned char*>(&left);
    const unsigned char* rightBytes = reinterpret_cast<const unsigned char*>(&right);
    unsigned char* resultBytes = reinterpret_cast<unsigned char*>(this);
    for (unsigned int channel = 0; channel < 8; ++channel)
    {
        const unsigned int sum = leftBytes[channel] + rightBytes[channel];
        resultBytes[channel] = static_cast<unsigned char>(sum > 255 ? 255 : sum);
    }
}

inline const GAMMA& GAMMA::SetAlpha(int component)
{
    if (component < -255) component = -255;
    if (component > 255) component = 255;
    modulate &= 0x00FFFFFFu;
    add &= 0x00FFFFFFu;
    if (component < 0) modulate |= static_cast<unsigned int>(-component) << 24;
    else add |= static_cast<unsigned int>(component) << 24;
    return *this;
}

inline const GAMMA& GAMMA::SetRed(int component)
{
    if (component < -255) component = -255;
    if (component > 255) component = 255;
    modulate &= 0xFF00FFFFu;
    add &= 0xFF00FFFFu;
    if (component < 0) modulate |= static_cast<unsigned int>(-component) << 16;
    else add |= static_cast<unsigned int>(component) << 16;
    return *this;
}

inline const GAMMA& GAMMA::SetGreen(int component)
{
    if (component < -255) component = -255;
    if (component > 255) component = 255;
    modulate &= 0xFFFF00FFu;
    add &= 0xFFFF00FFu;
    if (component < 0) modulate |= static_cast<unsigned int>(-component) << 8;
    else add |= static_cast<unsigned int>(component) << 8;
    return *this;
}

inline const GAMMA& GAMMA::SetBlue(int component)
{
    if (component < -255) component = -255;
    if (component > 255) component = 255;
    modulate &= 0xFFFFFF00u;
    add &= 0xFFFFFF00u;
    if (component < 0) modulate |= static_cast<unsigned int>(-component);
    else add |= static_cast<unsigned int>(component);
    return *this;
}
