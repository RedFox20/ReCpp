#include "bitutils.h"
#include "math.h" // min
#include <cstring> // memcpy
#include <stdlib.h> // malloc, free
#include <utility> // std::swap

namespace rpp
{
    ////////////////////////////////////////////////////////////////////////////////

    static int num_bytes_for_bits(uint32_t numBits) noexcept
    {
        int numBytes = numBits / 8;
        if (numBits % 8 != 0)
            ++numBytes;
        return numBytes;
    }

    ////////////////////////////////////////////////////////////////////////////////

    bit_array::bit_array(uint32_t numBits) noexcept
    {
        maxBits = numBits;
        capacity = num_bytes_for_bits(numBits);
        if (capacity == 0)
            return;
        data = (uint8_t*)malloc(capacity);
        memset(data, 0, capacity);
    }

    bit_array::bit_array(const uint8_t* buf, uint32_t len) noexcept
    {
        maxBits = len * 8;
        capacity = len;
        if (capacity == 0)
            return;
        data = (uint8_t*)malloc(capacity);
        memcpy(data, buf, len);
    }

    bit_array::bit_array(const uint8_t* buf, uint32_t len, uint32_t numBits) noexcept
    {
        maxBits = numBits;
        capacity = num_bytes_for_bits(numBits);
        if (capacity == 0)
            return;
        data = (uint8_t*)malloc(capacity);

        uint32_t bitsToCopy = numBits;
        if (bitsToCopy > len*8)
            bitsToCopy = len*8;
        uint32_t bytesToCopy = bitsToCopy / 8;
        if (bitsToCopy % 8 != 0)
            ++bytesToCopy;
        memcpy(data, buf, bytesToCopy);

        // zero the bits at the end
        int trailingBits = 8 - (bitsToCopy % 8);
        if (trailingBits)
        {
            data[bytesToCopy - 1] &= ~uint8_t(0xFF << trailingBits);
        }
    }

    bit_array::~bit_array() noexcept
    {
        if (data) free(data);
    }

    bit_array::bit_array(const bit_array& other) noexcept
    {
        reset(other.maxBits);
        if (data) memcpy(data, other.data, capacity);
    }
    bit_array& bit_array::operator=(const bit_array& other) noexcept
    {
        if (this != &other)
        {
            reset(other.maxBits);
            if (data) memcpy(data, other.data, capacity);
        }
        return *this;
    }

    bit_array::bit_array(bit_array&& other) noexcept
    {
        std::swap(data, other.data);
        std::swap(maxBits, other.maxBits);
        std::swap(capacity, other.capacity);
    }
    bit_array& bit_array::operator=(bit_array&& other) noexcept
    {
        std::swap(data, other.data);
        std::swap(maxBits, other.maxBits);
        std::swap(capacity, other.capacity);
        return *this;
    }

    void bit_array::reset(uint32_t numBits) noexcept
    {
        maxBits = numBits;
        const uint32_t newCapacity = num_bytes_for_bits(numBits);
        if (newCapacity == 0)
        {
            capacity = 0;
            if (data) { free(data); data = nullptr; }
            return;
        }
        if (newCapacity != capacity)
        {
            capacity = newCapacity;
            data = (uint8_t*)realloc(data, capacity);
        }
        memset(data, 0, capacity);
    }

    void bit_array::set(uint32_t bit) noexcept
    {
        if (bit >= maxBits)
            return;
        const uint32_t index = bit / 8;
        const uint32_t shift = bit % 8;
        const uint8_t mask = uint8_t(1 << shift);
        data[index] |= mask;
    }

    void bit_array::set(uint32_t bit, bool value) noexcept
    {
        if (bit >= maxBits)
            return;
        const uint32_t index = bit / 8;
        const uint32_t shift = bit % 8;
        const uint8_t mask = uint8_t(1 << shift);
        if (value) data[index] |= mask;
        else       data[index] &= ~mask;
    }

    void bit_array::unset(uint32_t bit) noexcept
    {
        if (bit >= maxBits)
            return;
        const uint32_t index = bit / 8;
        const uint32_t shift = bit % 8;
        const uint8_t mask = uint8_t(1 << shift);
        data[index] |= mask;
    }

    bool bit_array::isSet(uint32_t bit) const noexcept
    {
        if (bit >= maxBits)
            return false;
        const size_t index = bit / 8;
        const size_t shift = bit % 8;
        const uint8_t mask = uint8_t(1 << shift);
        return (data[index] & mask) != 0;
    }

    bool bit_array::checkAndSet(uint32_t bit) noexcept
    {
        if (bit >= maxBits)
            return false;
        const size_t index = bit / 8;
        const size_t shift = bit % 8;
        const uint8_t mask = uint8_t(1 << shift);
        if ((data[index] & mask) != 0)
            return false; // already set
        data[index] |= mask;
        return true;
    }

    uint8_t bit_array::getByte(uint32_t byteIndex) const noexcept
    {
        if (byteIndex >= capacity)
            return 0;
        return data[byteIndex];
    }

    uint32_t bit_array::copy(uint32_t startByteIndex, uint8_t* buffer, uint32_t count) const noexcept
    {
        if (startByteIndex >= capacity)
            return 0;
        uint32_t bytesToCopy = rpp::min(count, capacity - startByteIndex);
        memcpy(buffer, data + startByteIndex, bytesToCopy);
        return bytesToCopy;
    }

    uint32_t bit_array::copyNegated(uint32_t startByteIndex, uint8_t* buffer, uint32_t count) const noexcept
    {
        if (startByteIndex >= capacity)
            return 0;
        uint32_t bytesToCopy = rpp::min(count, capacity - startByteIndex);
        const uint8_t* src = data + startByteIndex;
        for (uint32_t i = 0; i < bytesToCopy; i++)
            buffer[i] = ~src[i];
        return bytesToCopy;
    }

    /**
     * @brief Safely copies negated bits, even if the bit indices are unaligned
     * @return Number of BYTES filled in dest
     */
    uint32_t bit_array::copyNegatedBits(uint32_t startBit, uint8_t* dest, uint32_t numBits) const noexcept
    {
        const uint32_t maxBits = sizeBits();
        if (startBit >= maxBits)
            return 0;

        const uint32_t bitsToCopy = rpp::min(numBits, maxBits - startBit);

        // fast path
        if (startBit % 8 == 0 && bitsToCopy % 8 == 0)
        {
            const uint32_t startByte = startBit / 8;
            const uint32_t numBytes = bitsToCopy / 8;
            return copyNegated(startByte, dest, numBytes);
        }

        // bitwise copy
        for (uint32_t bit = 0; bit < bitsToCopy; ++bit)
        {
            bool is_set = isSet(startBit + bit);
            const uint32_t destByte = bit / 8;
            const uint32_t destShift = bit % 8;
            if (is_set)
                dest[destByte] &= ~uint8_t(1 << destShift);
            else
                dest[destByte] |= uint8_t(1 << destShift);
        }

        return num_bytes_for_bits(bitsToCopy);
    }

    ////////////////////////////////////////////////////////////////////////////////
}
