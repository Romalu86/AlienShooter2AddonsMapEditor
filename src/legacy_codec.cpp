#include "legacy_codec.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace AS2Codec
{
RangeCoder::RangeCoder()
    : low(0), range(0), pending(0), bufferedByte(0), byteCount(0), file(0)
{
}

bool RangeCoder::IsOpen() const { return file != 0; }
int RangeCoder::PutByte(int value) { return std::fputc(value, file); }
int RangeCoder::GetByte() { return std::getc(file); }

void RangeCoder::StartEncoding(unsigned char firstByte, std::uint32_t initialByteCount,
                               std::FILE* stream)
{
    if (file) return;
    file = stream;
    low = 0;
    range = 0x80000000u;
    pending = 0;
    bufferedByte = firstByte;
    byteCount = initialByteCount;
}

void RangeCoder::StartEncoding(std::FILE* stream)
{
    StartEncoding(0, 0, stream);
}

int RangeCoder::StartDecoding(std::FILE* stream)
{
    if (file) return 0;
    file = stream;
    const int firstByte = GetByte();
    if (firstByte == EOF) return -1;
    bufferedByte = static_cast<unsigned char>(GetByte());
    low = bufferedByte >> 1;
    range = 0x80;
    return firstByte == 0 ? 0 : -1;
}

void RangeCoder::NormalizeEncoder()
{
    while (range <= 0x800000u)
    {
        if (low < 0x7F800000u)
        {
            PutByte(bufferedByte);
            while (pending)
            {
                PutByte(255);
                --pending;
            }
            bufferedByte = static_cast<unsigned char>(low >> 23);
        }
        else if ((low & 0x80000000u) != 0)
        {
            PutByte(static_cast<unsigned int>(bufferedByte)+1u);
            while (pending)
            {
                PutByte(0);
                --pending;
            }
            bufferedByte = static_cast<unsigned char>(low >> 23);
        }
        else
        {
            ++pending;
        }
        range <<= 8;
        ++byteCount;
        low = (low & 0x7FFFFFu) << 8;
    }
}

void RangeCoder::NormalizeDecoder()
{
    while (range <= 0x800000u)
    {
        low = ((bufferedByte & 1u) | (low << 1)) << 7;
        bufferedByte = static_cast<unsigned char>(GetByte());
        range <<= 8;
        low |= bufferedByte >> 1;
    }
}

void RangeCoder::Encode(std::uint32_t frequency, std::uint32_t lower, std::uint32_t total)
{
    NormalizeEncoder();
    const std::uint32_t unit = range / total;
    const std::uint32_t offset = unit * lower;
    low += offset;
    if (lower+frequency < total) range = unit*frequency;
    else range -= offset;
}

void RangeCoder::EncodeShift(std::uint32_t frequency, std::uint32_t lower, unsigned int bits)
{
    NormalizeEncoder();
    bits &= 31u;
    const std::uint32_t unit = range >> bits;
    const std::uint32_t offset = unit * lower;
    low += offset;
    if (((lower+frequency) >> bits) != 0) range -= offset;
    else range = unit*frequency;
}

std::uint32_t RangeCoder::DecodeCumulative(std::uint32_t total)
{
    NormalizeDecoder();
    pending = range / total;
    const std::uint32_t value = low / pending;
    return value >= total ? total-1u : value;
}

std::uint32_t RangeCoder::DecodeCumulativeShift(unsigned int bits)
{
    NormalizeDecoder();
    bits &= 31u;
    pending = range >> bits;
    const std::uint32_t value = low / pending;
    return (value >> bits) != 0 ? (1u << bits)-1u : value;
}

void RangeCoder::DecodeUpdate(std::uint32_t frequency, std::uint32_t lower, std::uint32_t total)
{
    const std::uint32_t offset = pending*lower;
    low -= offset;
    if (lower+frequency < total) range = pending*frequency;
    else range -= offset;
}

std::uint32_t RangeCoder::DecodeByte()
{
    const std::uint32_t value = DecodeCumulativeShift(8);
    DecodeUpdate(1, value, 256);
    return value;
}

std::uint32_t RangeCoder::DecodeWord()
{
    const std::uint32_t value = DecodeCumulativeShift(16);
    DecodeUpdate(1, value, 65536);
    return value;
}

std::uint32_t RangeCoder::FinishEncoding()
{
    if (!file) return 0xFFFFFFFFu;
    NormalizeEncoder();
    byteCount += 5;
    std::uint32_t finalByte = low >> 23;
    if (((byteCount >> 1) & 0x7FFFFFu) <= (low & 0x7FFFFFu)) ++finalByte;
    if (finalByte < 256)
    {
        PutByte(bufferedByte);
        while (pending)
        {
            PutByte(255);
            --pending;
        }
    }
    else
    {
        PutByte(static_cast<unsigned int>(bufferedByte)+1u);
        while (pending)
        {
            PutByte(0);
            --pending;
        }
    }
    PutByte(finalByte & 255u);
    PutByte((byteCount >> 16) & 255u);
    PutByte((byteCount >> 8) & 255u);
    PutByte(byteCount & 255u);
    return byteCount;
}

AdaptiveModel::AdaptiveModel()
    : symbolCount(0), remainingUpdates(0), extraUpdates(0), updateInterval(0),
      maximumUpdateInterval(0), increment(0), lookupShift(0)
{
}

void AdaptiveModel::Initialize(int symbols, int bits, int maximumInterval,
                               const std::uint32_t* initialFrequencies)
{
    symbolCount = symbols;
    maximumUpdateInterval = maximumInterval;
    lookupShift = static_cast<unsigned int>(std::max(bits-7, 0));
    cumulative.resize(static_cast<unsigned int>(symbols)+1u);
    frequencies.resize(static_cast<unsigned int>(symbols)+1u);
    lookup.resize(129);
    cumulative[symbols] = static_cast<std::uint16_t>(1u << (bits & 31));
    cumulative[0] = 0;
    lookup[128] = static_cast<std::uint16_t>(symbols-1);
    Reset(initialFrequencies);
}

void AdaptiveModel::Reset(const std::uint32_t* initialFrequencies)
{
    updateInterval = (symbolCount | 0x20) >> 4;
    extraUpdates = 0;
    if (initialFrequencies)
    {
        for (int i=0; i<symbolCount; ++i)
            frequencies[i] = static_cast<std::uint16_t>(initialFrequencies[i]);
    }
    else
    {
        const unsigned int quotient = cumulative[symbolCount] / symbolCount;
        const unsigned int remainder = cumulative[symbolCount] % symbolCount;
        for (int i=0; i<symbolCount; ++i)
            frequencies[i] = static_cast<std::uint16_t>(quotient+(static_cast<unsigned int>(i)<remainder));
    }
    Rescale();
}

void AdaptiveModel::Rescale()
{
    if (extraUpdates)
    {
        ++increment;
        remainingUpdates = extraUpdates;
        extraUpdates = 0;
        return;
    }
    if (updateInterval < maximumUpdateInterval)
        updateInterval = std::min(updateInterval*2, maximumUpdateInterval);
    unsigned int oldTotal = cumulative[symbolCount];
    unsigned int available = oldTotal;
    for (int i=symbolCount-1; i>0; --i)
    {
        const unsigned int previous = frequencies[i];
        oldTotal -= previous;
        cumulative[i] = static_cast<std::uint16_t>(oldTotal);
        const unsigned int reduced = (previous | 2u) >> 1;
        frequencies[i] = static_cast<std::uint16_t>(reduced);
        available -= reduced;
    }
    if (oldTotal != frequencies[0])
    {
        const int address = static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(cumulative.data()));
        std::fprintf(stderr, "BUG: rescaling left %d total frequency\n", address);
        std::_Exit(1);
    }
    frequencies[0] = static_cast<std::uint16_t>((frequencies[0] | 2u) >> 1);
    const int difference = static_cast<int>(available-frequencies[0]);
    increment = difference/updateInterval;
    extraUpdates = difference%updateInterval;
    remainingUpdates = updateInterval-extraUpdates;
    for (int i=symbolCount-1; i>=0; --i)
    {
        const int last = (static_cast<int>(cumulative[i+1])-1) >> lookupShift;
        for (int bucket=cumulative[i] >> lookupShift; bucket<=last; ++bucket)
            lookup[bucket] = static_cast<std::uint16_t>(i);
    }
}

void AdaptiveModel::GetRange(unsigned int symbol, std::uint32_t* frequency,
                             std::uint32_t* lower) const
{
    *lower = cumulative[symbol];
    *frequency = cumulative[symbol+1]-*lower;
}

unsigned int AdaptiveModel::FindSymbol(std::uint32_t cumulativeFrequency) const
{
    const unsigned int bucket = cumulativeFrequency >> lookupShift;
    unsigned int lowSymbol = lookup[bucket];
    unsigned int highSymbol = lookup[bucket+1]+1u;
    while (lowSymbol+1u < highSymbol)
    {
        const unsigned int middle = (lowSymbol+highSymbol) >> 1;
        if (cumulativeFrequency < cumulative[middle]) highSymbol = middle;
        else lowSymbol = middle;
    }
    return lowSymbol;
}

std::uint16_t* AdaptiveModel::Observe(unsigned int symbol)
{
    if (remainingUpdates <= 0) Rescale();
    --remainingUpdates;
    frequencies[symbol] = static_cast<std::uint16_t>(frequencies[symbol]+increment);
    return &frequencies[symbol];
}

ByteCoder::ByteCoder(int maximumInterval)
{
    model.Initialize(257, 12, maximumInterval);
}

void ByteCoder::StartEncoding(std::FILE* stream) { coder.StartEncoding(stream); }
int ByteCoder::StartDecoding(std::FILE* stream) { return coder.StartDecoding(stream); }

std::uint16_t* ByteCoder::EncodeByte(unsigned int value)
{
    std::uint32_t frequency, lower;
    model.GetRange(value, &frequency, &lower);
    coder.EncodeShift(frequency, lower, 12);
    return model.Observe(value);
}

int ByteCoder::DecodeByte()
{
    const unsigned int symbol = model.FindSymbol(coder.DecodeCumulativeShift(12));
    if (symbol == 256) return -1;
    std::uint32_t frequency, lower;
    model.GetRange(symbol, &frequency, &lower);
    coder.DecodeUpdate(frequency, lower, 4096);
    model.Observe(symbol);
    return static_cast<int>(symbol);
}

std::uint32_t ByteCoder::FinishEncoding()
{
    std::uint32_t frequency, lower;
    model.GetRange(256, &frequency, &lower);
    coder.EncodeShift(frequency, lower, 12);
    return coder.FinishEncoding();
}

void ByteCoder::FinishDecoding()
{
    std::uint32_t frequency, lower;
    model.GetRange(256, &frequency, &lower);
    coder.DecodeUpdate(frequency, lower, 4096);
    coder.NormalizeDecoder();
}

std::uint32_t ByteCoder::Encode(const void* data, std::uint32_t size, std::FILE* stream)
{
    StartEncoding(stream);
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    for (std::uint32_t i=0; i<size; ++i) EncodeByte(bytes[i]);
    return FinishEncoding();
}

ContextByteCoder::ContextByteCoder(int mode, int maximumInterval) : byteOrderMode(mode)
{
    for (unsigned int i=0; i<256; ++i) contexts[i].Initialize(257, 12, maximumInterval);
}

void ContextByteCoder::Reset()
{
    for (unsigned int i=0; i<256; ++i) contexts[i].Reset();
}

std::uint32_t ContextByteCoder::Encode(const void* data, std::uint32_t size, std::FILE* stream)
{
    if (!stream || size == 0) return 0;
    if (size < 10) return static_cast<std::uint32_t>(std::fwrite(data, 1, size, stream));
    RangeCoder coder;
    coder.StartEncoding(stream);
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    const std::uint32_t firstHalf = (size+1u) >> 1;
    unsigned int previous = 0;
    for (std::uint32_t i=0; i<size; ++i)
    {
        const std::uint32_t index = byteOrderMode == 2 ?
            (i<firstHalf ? i*2u : (i-firstHalf)*2u+1u) : i;
        const unsigned int value = bytes[index];
        std::uint32_t frequency, lower;
        contexts[previous].GetRange(value, &frequency, &lower);
        coder.EncodeShift(frequency, lower, 12);
        contexts[previous].Observe(value);
        previous = value;
    }
    std::uint32_t frequency, lower;
    contexts[previous].GetRange(256, &frequency, &lower);
    coder.EncodeShift(frequency, lower, 12);
    return coder.FinishEncoding();
}

std::uint32_t ContextByteCoder::Decode(void* data, std::uint32_t size, std::FILE* stream)
{
    if (!stream || size == 0) return 0;
    if (size < 10) return static_cast<std::uint32_t>(std::fread(data, 1, size, stream));
    RangeCoder coder;
    if (coder.StartDecoding(stream) < 0) return 0;
    unsigned char* bytes = static_cast<unsigned char*>(data);
    const std::uint32_t firstHalf = (size+1u) >> 1;
    std::uint32_t written = 0;
    unsigned int previous = 0;
    for (;;)
    {
        AdaptiveModel& model = contexts[previous];
        const unsigned int value = model.FindSymbol(coder.DecodeCumulativeShift(12));
        if (value == 256) break;
        if (written >= size)
        {
            std::fprintf(stderr, "!!!ERROR!!! decode");
            break;
        }
        const std::uint32_t index = byteOrderMode == 2 ?
            (written<firstHalf ? written*2u : (written-firstHalf)*2u+1u) : written;
        bytes[index] = static_cast<unsigned char>(value);
        ++written;
        std::uint32_t frequency, lower;
        model.GetRange(value, &frequency, &lower);
        coder.DecodeUpdate(frequency, lower, 4096);
        model.Observe(value);
        previous = value;
    }
    std::uint32_t frequency, lower;
    contexts[previous].GetRange(256, &frequency, &lower);
    coder.DecodeUpdate(frequency, lower, 4096);
    coder.NormalizeDecoder();
    return written;
}

MoveToFront::MoveToFront() { Reset(); }

void MoveToFront::Reset()
{
    for (unsigned int i=0; i<256; ++i) values[i] = static_cast<unsigned char>(i);
}

unsigned int MoveToFront::Encode(unsigned char value)
{
    const unsigned char* found = static_cast<const unsigned char*>(std::memchr(values, value, 256));
    const unsigned int index = static_cast<unsigned int>(found-values);
    std::memmove(values+1, values, index);
    values[0] = value;
    return index;
}

unsigned int MoveToFront::Decode(unsigned int index)
{
    const unsigned char value = values[index];
    std::memmove(values+1, values, index);
    values[0] = value;
    return value;
}

std::uint32_t EncodeRle(const void* data, std::uint32_t size, std::FILE* stream)
{
    ByteCoder coder(2000);
    coder.StartEncoding(stream);
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    std::uint32_t position = 0;
    unsigned int previous = 0;
    while (position < size)
    {
        unsigned int value = bytes[position++];
        coder.EncodeByte(value);
        if (value == previous)
        {
            unsigned int count = 0;
            while (position < size)
            {
                value = bytes[position++];
                if (value != previous) break;
                if (++count == 255) break;
            }
            coder.EncodeByte(count);
            if (count != 255 && position <= size) coder.EncodeByte(value);
        }
        previous = value;
    }
    return coder.FinishEncoding();
}

std::uint32_t DecodeRle(void* data, std::uint32_t size, std::FILE* stream)
{
    ByteCoder coder(2000);
    coder.StartDecoding(stream);
    unsigned char* bytes = static_cast<unsigned char*>(data);
    std::uint32_t written = 0;
    int previous = 0;
    while (written < size)
    {
        const int value = coder.DecodeByte();
        if (value < 0)
        {
            std::fprintf(stderr, "RLE::Can't read %i bytes \n\r", static_cast<int>(size-written));
            coder.FinishDecoding();
            return written;
        }
        bytes[written++] = static_cast<unsigned char>(value);
        if (value == previous)
        {
            const int count = coder.DecodeByte();
            if (count > 0)
            {
                std::memset(bytes+written, value, static_cast<unsigned int>(count));
                written += static_cast<unsigned int>(count);
            }
        }
        previous = value;
    }
    while (coder.DecodeByte() >= 0) {}
    coder.FinishDecoding();
    return written;
}

std::uint32_t EncodePermutation(const void* data, std::uint32_t size, std::FILE* stream)
{
    const unsigned char* bytes = static_cast<const unsigned char*>(data);
    const int length = static_cast<int>(size);
    std::vector<unsigned char> transformed(size+1u);
    std::vector<int> next(size+2u), previous(size+2u);
    int first[257];
    std::fill(first, first+256, -1);
    first[256] = length;
    next[length] = next[length+1] = length;
    previous[length] = previous[length+1] = length+1;
    for (int i=length-1; i>=0; --i)
    {
        const unsigned int value = bytes[i];
        const int group = first[value];
        if (group < 0)
        {
            unsigned int larger = value+1;
            while (larger < 256 && first[larger] < 0) ++larger;
            next[i] = first[larger];
            previous[i] = previous[next[i]];
            next[previous[i]] = i;
            previous[next[i]] = i;
            first[value] = i;
        }
        else if (group+1 < length && bytes[group+1] > bytes[i+1])
        {
            next[i] = group;
            previous[i] = previous[group];
            next[previous[i]] = i;
            previous[group] = i;
            first[value] = i;
        }
        else
        {
            int after = next[i+1];
            int before = previous[i+1];
            for (;;)
            {
                if (after > 0 && bytes[after-1] == value)
                {
                    next[i] = after-1;
                    previous[i] = previous[after-1];
                    next[previous[i]] = i;
                    previous[after-1] = i;
                    if (first[value] == after-1) first[value] = i;
                    break;
                }
                if (before > 0 && before <= length && bytes[before-1] == value)
                {
                    previous[i] = before-1;
                    next[i] = next[before-1];
                    previous[next[i]] = i;
                    next[before-1] = i;
                    break;
                }
                if (before == length+1)
                {
                    next[i] = first[value];
                    previous[i] = previous[next[i]];
                    next[previous[i]] = i;
                    previous[next[i]] = i;
                    first[value] = i;
                    break;
                }
                if (after == length)
                {
                    unsigned int larger = value+1;
                    while (larger < 256 && first[larger] < 0) ++larger;
                    next[i] = first[larger];
                    previous[i] = previous[next[i]];
                    next[previous[i]] = i;
                    previous[next[i]] = i;
                    break;
                }
                after = next[after];
                before = previous[before];
            }
        }
    }
    std::uint32_t primary = 0;
    int suffix = next[length+1];
    for (std::uint32_t i=0; i<=size; ++i)
    {
        if (suffix != 0) transformed[i] = bytes[suffix-1];
        else
        {
            primary = i;
            transformed[i] = '?';
        }
        suffix = next[suffix];
    }
    std::fwrite(&primary, 4, 1, stream);
    MoveToFront order;
    for (std::uint32_t i=0; i<=size; ++i)
        transformed[i] = static_cast<unsigned char>(order.Encode(transformed[i]));
    return EncodeRle(transformed.data(), size+1u, stream);
}

std::uint32_t DecodePermutation(void* data, std::uint32_t size, std::FILE* stream)
{
    std::vector<std::uint32_t> next(size+1u);
    std::vector<unsigned char> transformed(size+1u);
    std::uint32_t primary = 0;
    std::fread(&primary, 4, 1, stream);
    DecodeRle(transformed.data(), size+1u, stream);
    MoveToFront order;
    for (std::uint32_t i=0; i<=size; ++i)
        transformed[i] = static_cast<unsigned char>(order.Decode(transformed[i]));
    std::uint32_t cumulative[257] = {};
    cumulative[256] = size;
    for (std::uint32_t i=0; i<=size; ++i)
        if (i != primary) ++cumulative[transformed[i]];
    for (int i=255; i>=0; --i) cumulative[i] = cumulative[i+1]-cumulative[i];
    for (std::uint32_t i=0; i<=size; ++i)
        if (i != primary) next[cumulative[transformed[i]]++] = i;
    unsigned char* bytes = static_cast<unsigned char*>(data);
    for (std::uint32_t i=0; i<size; ++i)
    {
        primary = next[primary];
        bytes[i] = transformed[primary];
    }
    return size;
}
}
