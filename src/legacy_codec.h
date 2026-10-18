#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>

namespace AS2Codec
{
class RangeCoder
{
public:
    RangeCoder();
    bool IsOpen() const;
    int PutByte(int value);
    int GetByte();
    void StartEncoding(unsigned char firstByte, std::uint32_t initialByteCount, std::FILE* stream);
    void StartEncoding(std::FILE* stream);
    int StartDecoding(std::FILE* stream);
    void NormalizeEncoder();
    void NormalizeDecoder();
    void Encode(std::uint32_t frequency, std::uint32_t lower, std::uint32_t total);
    void EncodeShift(std::uint32_t frequency, std::uint32_t lower, unsigned int bits);
    std::uint32_t DecodeCumulative(std::uint32_t total);
    std::uint32_t DecodeCumulativeShift(unsigned int bits);
    void DecodeUpdate(std::uint32_t frequency, std::uint32_t lower, std::uint32_t total);
    std::uint32_t DecodeByte();
    std::uint32_t DecodeWord();
    std::uint32_t FinishEncoding();

private:
    std::uint32_t low;
    std::uint32_t range;
    std::uint32_t pending;
    unsigned char bufferedByte;
    std::uint32_t byteCount;
    std::FILE* file;
};

class AdaptiveModel
{
public:
    AdaptiveModel();
    void Initialize(int symbols, int bits, int maximumInterval,
                    const std::uint32_t* initialFrequencies = 0);
    void Reset(const std::uint32_t* initialFrequencies = 0);
    void Rescale();
    void GetRange(unsigned int symbol, std::uint32_t* frequency, std::uint32_t* lower) const;
    unsigned int FindSymbol(std::uint32_t cumulativeFrequency) const;
    std::uint16_t* Observe(unsigned int symbol);

private:
    int symbolCount;
    int remainingUpdates;
    int extraUpdates;
    int updateInterval;
    int maximumUpdateInterval;
    int increment;
    unsigned int lookupShift;
    std::vector<std::uint16_t> cumulative;
    std::vector<std::uint16_t> frequencies;
    std::vector<std::uint16_t> lookup;
};

class ByteCoder
{
public:
    explicit ByteCoder(int maximumInterval);
    void StartEncoding(std::FILE* stream);
    int StartDecoding(std::FILE* stream);
    std::uint16_t* EncodeByte(unsigned int value);
    int DecodeByte();
    std::uint32_t FinishEncoding();
    void FinishDecoding();
    std::uint32_t Encode(const void* data, std::uint32_t size, std::FILE* stream);

private:
    AdaptiveModel model;
    RangeCoder coder;
};

class ContextByteCoder
{
public:
    ContextByteCoder(int mode, int maximumInterval);
    void Reset();
    std::uint32_t Encode(const void* data, std::uint32_t size, std::FILE* stream);
    std::uint32_t Decode(void* data, std::uint32_t size, std::FILE* stream);

private:
    int byteOrderMode;
    AdaptiveModel contexts[256];
};

class MoveToFront
{
public:
    MoveToFront();
    void Reset();
    unsigned int Encode(unsigned char value);
    unsigned int Decode(unsigned int index);

private:
    unsigned char values[256];
};

std::uint32_t EncodeRle(const void* data, std::uint32_t size, std::FILE* stream);
std::uint32_t DecodeRle(void* data, std::uint32_t size, std::FILE* stream);
std::uint32_t EncodePermutation(const void* data, std::uint32_t size, std::FILE* stream);
std::uint32_t DecodePermutation(void* data, std::uint32_t size, std::FILE* stream);
}
