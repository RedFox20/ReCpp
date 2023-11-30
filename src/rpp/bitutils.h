#pragma once
#include "config.h"
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
}
