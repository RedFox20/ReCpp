#pragma once
#include "config.h" // RPP_BUILTIN_MEMSET
#include <cstdint> // uint8_t

namespace rpp
{
    ////////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Fixed-length array of bits, providing a simple interface for setting and checking bits
     *        with overflow guard checks [errors suppressed]
     */
    class bit_array
    {
        uint8_t* data = nullptr;
        uint32_t maxBits = 0; // maximum bits [to avoid garbage bits at the end]
        uint32_t capacity = 0; // capacity in bytes
    public:

        /**
         * @brief Creates an empty bit array with 0 capacity
         */
        bit_array() noexcept = default;
        /**
         * @brief Creates a bit array with a fixed number of bits
         */
        explicit bit_array(uint32_t numBits) noexcept;
        /**
         * @brief Initializes this bit array from a buffer of len*8 bits
         */
        bit_array(const uint8_t* buf, uint32_t len) noexcept;
        /**
         * @brief Initializes this array with numBits copied from buffer of len bytes
         */
        bit_array(const uint8_t* buf, uint32_t len, uint32_t numBits) noexcept;
        ~bit_array() noexcept;

        // COPY
        bit_array(const bit_array& other) noexcept;
        bit_array& operator=(const bit_array& other) noexcept;
        // MOVE
        bit_array(bit_array&& other) noexcept;
        bit_array& operator=(bit_array&& other) noexcept;


        /** @return Total number of bytes stored in this bit array */
        uint32_t sizeBytes() const noexcept { return capacity; }
        /** @return Total number of bits stored in this bit array */
        uint32_t sizeBits() const noexcept { return maxBits;  }
        /** @return Internal buffer */
        const uint8_t* getBuffer() const noexcept { return data; }
        explicit operator bool() const noexcept { return maxBits != 0; }

        /**
         * @brief Clears all bits and sets a new length
         */
        void reset(uint32_t numBits) noexcept;

        /**
         * @brief Sets the specified bit
         * @param bit Index of the bit to set
         */
        void set(uint32_t bit) noexcept;

        /**
         * @brief Sets the specified bit to the specified value
         * @param bit Index of the bit to set
         * @param value true to set the bit, false to unset it
         */
        void set(uint32_t bit, bool value) noexcept;

        /**
         * @brief Unsets the specified bit
         * @param bit Index of the bit to set
         */
        void unset(uint32_t bit) noexcept;

        /**
         * @return true if the specified bit is set
         */
        bool isSet(uint32_t bit) const noexcept;

        /**
         * Checks if the specified bit is set and sets it if it's not
         * @return TRUE if the bit was just set, FALSE if it was already set
         */
        bool checkAndSet(uint32_t bit) noexcept;

        /**
         * @return 8 bits packet into a single byte
         */
        uint8_t getByte(uint32_t byteIndex) const noexcept;

        /**
         * @brief Copies bit bytes from this bit array into the specified buffer
         * @param startByteIndex Index of the first byte to copy
         * @param buffer Destination buffer
         * @param count Number of bytes to copy
         * @return Number of bytes filled in dest buffer
         */
        uint32_t copy(uint32_t startByteIndex, uint8_t* buffer, uint32_t count) const noexcept;

        /**
         * @brief Copies and negates bit bytes from this bit array into the specified buffer
         * @param startByteIndex Index of the first byte to copy
         * @param buffer Destination buffer
         * @param count Number of bytes to copy
         * @return Number of bytes filled in dest buffer
         */
        uint32_t copyNegated(uint32_t startByteIndex, uint8_t* buffer, uint32_t count) const noexcept;

        /**
         * @brief Safely copies negated bits, even if the bit indices are unaligned
         * @param startBit Index of the first bit to copy
         * @param dest Destination buffer
         * @param numBits Number of bits to copy
         * @return Number of ! BYTES ! filled in dest
         */
        uint32_t copyNegatedBits(uint32_t startBit, uint8_t* dest, uint32_t numBits) const noexcept;
    };

    ////////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Fixed-size array of bits in inline storage, a light port of std::bitset<N>.
     *        The bytes use the bit_array layout, and the bitset ignores an index past the end.
     */
    template<uint32_t NBits>
    class bitset
    {
        static_assert(NBits > 0, "rpp::bitset needs at least one bit");
        static constexpr uint32_t num_bytes = (NBits + 7) / 8;
        uint8_t bytes[num_bytes] = {};
    public:

        /** @brief Creates a bitset with every bit clear */
        bitset() noexcept = default;

        /** @brief Copies the low NBits bits of an integer, as std::bitset<N>(unsigned long long) does */
        constexpr explicit bitset(uint64_t bits) noexcept
        {
            constexpr uint32_t count = num_bytes < 8 ? num_bytes : 8;
            for (uint32_t i = 0; i < count; ++i)
                bytes[i] = uint8_t(bits >> (i * 8));
            if constexpr (NBits % 8 != 0)
                bytes[num_bytes - 1] &= uint8_t((1u << (NBits % 8)) - 1);
        }

        /** @return Number of bits in this bitset */
        constexpr uint32_t size_bits() const noexcept { return NBits; }
        /** @return Number of bytes in the inline buffer */
        constexpr uint32_t size_bytes() const noexcept { return num_bytes; }
        /** @return The inline buffer, with the least significant bit first in each byte */
        const uint8_t* get_buffer() const noexcept { return bytes; }

        /** @brief Clears every bit */
        void reset() noexcept { RPP_BUILTIN_MEMSET(bytes, 0, num_bytes); }

        // FINLINE on each writer stops a false -Warray-bounds when gcc 13 merges the bodies of two sizes
        /** @brief Sets the bit, or does nothing when the bit is past the end */
        FINLINE void set(uint32_t bit) noexcept
        {
            if (bit < NBits)
                bytes[bit / 8] |= uint8_t(1u << (bit % 8));
        }

        /** @brief Sets the bit to the value, or does nothing when the bit is past the end */
        void set(uint32_t bit, bool value) noexcept
        {
            if (value)
                set(bit);
            else
                unset(bit);
        }

        /** @brief Clears the bit, or does nothing when the bit is past the end */
        FINLINE void unset(uint32_t bit) noexcept
        {
            if (bit < NBits)
                bytes[bit / 8] &= uint8_t(~(1u << (bit % 8)));
        }

        /** @return true if the bit is set, false if it is clear or past the end */
        bool is_set(uint32_t bit) const noexcept
        {
            return bit < NBits && (bytes[bit / 8] & (1u << (bit % 8))) != 0;
        }

        /** @return true if this call set the bit, false if the bit was set already or is past the end */
        FINLINE bool check_and_set(uint32_t bit) noexcept
        {
            if (bit >= NBits)
                return false;
            const uint8_t mask = uint8_t(1u << (bit % 8));
            if ((bytes[bit / 8] & mask) != 0)
                return false;
            bytes[bit / 8] |= mask;
            return true;
        }
    };

    ////////////////////////////////////////////////////////////////////////////////
}
