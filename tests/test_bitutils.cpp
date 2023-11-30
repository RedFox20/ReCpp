#include <rpp/bitutils.h>
#include <rpp/tests.h>

TestImpl(test_bitutils)
{
    TestInit(test_bitutils)
    {
    }

    TestCase(bitarray_DefaultConstructor)
    {
        rpp::bit_array bitarray;
        AssertEqual(bitarray.sizeBytes(), 0U);
        AssertEqual(bitarray.sizeBits(), 0U);
        AssertEqual(bitarray.getBuffer(), nullptr);

        // check for safeties
        for (uint32_t bit = 0; bit < 32; ++bit)
            bitarray.set(bit);
        for (uint32_t bit = 0; bit < 32; ++bit)
            AssertMsg(!bitarray.isSet(bit), "bit %d was not 0", bit);
        for (uint32_t bit = 0; bit < 32; ++bit)
            bitarray.unset(bit);
    }

    TestCase(bitarray_CopySemantics)
    {
        constexpr uint32_t BITS = 28U;
        rpp::bit_array bitarray{BITS};
        for (uint32_t bit = 0; bit < BITS; ++bit)
            bitarray.set(bit, bit % 2 == 0);

        rpp::bit_array copy1{bitarray};
        rpp::bit_array copy2;
        copy2 = bitarray;

        AssertEqual(copy1.sizeBits(), BITS);
        AssertEqual(copy2.sizeBits(), BITS);
        AssertEqual(copy1.sizeBytes(), 4U);
        AssertEqual(copy2.sizeBytes(), 4U);
        AssertNotEqual(copy1.getBuffer(), nullptr);
        AssertNotEqual(copy2.getBuffer(), nullptr);

        for (uint32_t bit = 0; bit < BITS; ++bit)
        {
            AssertMsg(copy1.isSet(bit) == bitarray.isSet(bit), "bit %d was not copied correctly", bit);
            AssertMsg(copy2.isSet(bit) == bitarray.isSet(bit), "bit %d was not copied correctly", bit);
        }

        rpp::bit_array emptyCopy;
        rpp::bit_array emptyCopy2{emptyCopy};
        AssertEqual(emptyCopy2.sizeBits(), 0U);
        AssertEqual(emptyCopy2.sizeBytes(), 0U);
        AssertEqual(emptyCopy2.sizeBits(), 0U);
    }

    TestCase(bitarray_MoveSemantics)
    {
        rpp::bit_array move2;
        {
            constexpr uint32_t BITS = 28U;
            rpp::bit_array bitarray{BITS};
            for (uint32_t bit = 0; bit < BITS; ++bit)
                bitarray.set(bit, bit % 2 == 0);

            rpp::bit_array move1{std::move(bitarray)};
            AssertEqual(move1.sizeBits(), BITS);
            AssertEqual(move1.sizeBytes(), 4U);
            AssertNotEqual(move1.getBuffer(), nullptr);
            AssertEqual(bitarray.sizeBits(), 0U);
            AssertEqual(bitarray.sizeBytes(), 0U);
            AssertEqual(bitarray.getBuffer(), nullptr);

            // now check scoping effects
            move2 = std::move(move1);
        }

        AssertEqual(move2.sizeBits(), 28U);
        AssertEqual(move2.sizeBytes(), 4U);
        for (uint32_t bit = 0; bit < move2.sizeBits(); ++bit)
        {
            AssertMsg(move2.isSet(bit) == (bit % 2 == 0), "bit %d was not copied correctly", bit);
        }
    }

    TestCase(bitarray_BitEmptyOnCreation)
    {
        rpp::bit_array bitarray{256};
        AssertEqual(bitarray.sizeBytes(), 32U);
        AssertEqual(bitarray.sizeBytes() * 8, bitarray.sizeBits()); // only true for aligned bitarrays
        for (uint32_t bit = 0; bit < bitarray.sizeBits(); ++bit)
        {
            AssertMsg(!bitarray.isSet(bit), "bit %d was not 0", bit);
        }
    }

    TestCase(bitarray_BitsEmptyOnCreation_Unaligned)
    {
        rpp::bit_array bitarray{55};
        AssertEqual(bitarray.sizeBytes(), 7U);
        AssertEqual(bitarray.sizeBits(), 55U);
        AssertNotEqual(bitarray.getBuffer(), nullptr);
        for (uint32_t bit = 0; bit < bitarray.sizeBits(); ++bit)
        {
            AssertMsg(!bitarray.isSet(bit), "bit %d was not 0", bit);
        }
    }

    TestCase(bitarray_CorrectlyCalculatesInitSize)
    {
        rpp::bit_array zerosize{0U};
        AssertEqual(zerosize.sizeBytes(), 0U);
        AssertEqual(zerosize.sizeBits(), 0U);
        AssertEqual(zerosize.getBuffer(), nullptr);

        for (uint32_t numBits = 1; numBits < 100; numBits += 9)
        {
            uint32_t expectedSizeBytes = numBits / 8 + (numBits % 8 ? 1 : 0);
            rpp::bit_array bitarray{numBits};
            AssertEqual(bitarray.sizeBytes(), expectedSizeBytes);
            AssertEqual(bitarray.sizeBits(), numBits);
        }
    }

    TestCase(bitarray_CorrectlyInitsFromBufferBytes)
    {
        // create reference buffer
        rpp::bit_array ref{100};
        AssertEqual(ref.sizeBytes(), 13U);
        AssertEqual(ref.sizeBits(), 100U);
        for (uint32_t bit = 0; bit < ref.sizeBits(); ++bit)
            ref.set(bit, bit % 2 == 0);

        // initialized copy
        rpp::bit_array bitarray{ref.getBuffer(), ref.sizeBytes()};

        // check that it matches
        AssertEqual(bitarray.sizeBytes(), ref.sizeBytes());
        AssertEqual(bitarray.sizeBits(), 13*8U); // the new size is aligned to 8 bits
        for (uint32_t bit = 0; bit < 13*8U; ++bit)
            AssertMsg(bitarray.isSet(bit) == ref.isSet(bit), "bit %d was not copied correctly", bit);
        // the trailing bits must be zero
        for (uint32_t bit = 13*8U; bit < bitarray.sizeBits(); ++bit)
            AssertMsg(!bitarray.isSet(bit), "trailing bit %d was not zero", bit);
    }

    TestCase(bitarray_CorrectlyInitsFromBufferBits)
    {
        // create reference buffer
        rpp::bit_array ref{100};
        for (uint32_t bit = 0; bit < ref.sizeBits(); ++bit)
            ref.set(bit, bit % 3 == 0);

        // initialized copy
        rpp::bit_array bitarray{ref.getBuffer(), ref.sizeBytes(), ref.sizeBits()};

        // check that it matches
        AssertEqual(bitarray.sizeBytes(), ref.sizeBytes());
        AssertEqual(bitarray.sizeBits(), ref.sizeBits());
        for (uint32_t bit = 0; bit < bitarray.sizeBits(); ++bit)
            AssertMsg(bitarray.isSet(bit) == ref.isSet(bit), "bit %d was not copied correctly", bit);
    }

    TestCase(bitarray_ResetClearsBits)
    {
        // create a bitarray
        rpp::bit_array ref{100};
        for (uint32_t bit = 0; bit < ref.sizeBits(); ++bit)
            ref.set(bit, bit % 3 == 0);
        AssertEqual(ref.sizeBytes(), 13U);
        AssertEqual(ref.sizeBits(), 100U);
        for (uint32_t bit = 0; bit < ref.sizeBits(); ++bit)
            AssertMsg(ref.isSet(bit) == (bit % 3 == 0), "bit %d was not copied correctly", bit);

        // reset it to a smaller one
        ref.reset(50);
        AssertEqual(ref.sizeBytes(), 7U);
        AssertEqual(ref.sizeBits(), 50U);
        // ensure all bits are zero
        for (uint32_t bit = 0; bit < ref.sizeBits(); ++bit)
            AssertMsg(!ref.isSet(bit), "bit %d was not reset to 0", bit);

        ref.reset(0);
        AssertEqual(ref.sizeBytes(), 0U);
        AssertEqual(ref.sizeBits(), 0U);
        AssertEqual(ref.getBuffer(), nullptr);
    }

    TestCase(bitarray_BitArray_CorrectlyAssignsBits)
    {
        rpp::bit_array bitarray{20};
        for (uint32_t bit = 0; bit < bitarray.sizeBits(); bit += 3)
        {
            bitarray.set(bit);
        }
        for (uint32_t bit = 0; bit < bitarray.sizeBits(); bit++)
        {
            AssertMsg(bitarray.isSet(bit) == (bit % 3 == 0), 
                      "Bit is set incorrect: %d %d", bit, (bit % 3 == 0));
        }
    }

    TestCase(bitarray_BitArray_ReturnsFalseWhenBitIsGetOutOfBounds)
    {
        rpp::bit_array bitarray{8};
        AssertFalse(bitarray.isSet(bitarray.sizeBits() + 1));
    }

    TestCase(bitarray_BitArray_CopiesToBufferCorrectly)
    {
        rpp::bit_array bitarray{8 * 8};

        // We are setting 1 bit for each byte increasing shift by 1 each time
        // Inside bitarray it will look like:
        // 00000001 00000010 00000100 00001000...
        for (uint32_t i = 0; i < bitarray.sizeBytes(); i++)
        {
            bitarray.set(i * 8 + i);
        }

        struct {
            uint32_t padding1 = 0xBAADF00D;
            char space[8];
            uint32_t padding2 = 0xCAFEF00D;
        } paddedBuf;

        uint8_t* buf = reinterpret_cast<uint8_t*>(paddedBuf.space);
        const uint32_t sizeBytes = sizeof(paddedBuf.space);

        bitarray.copy(0, (uint8_t*)buf, sizeBytes);
        for (uint32_t i = 0; i < sizeBytes; i++)
        {
            AssertEqual(buf[i], uint8_t(std::pow(2, i)));
        }

        bitarray.copy(6, (uint8_t*)buf, sizeBytes);

        for (uint32_t i = 0; i < sizeBytes; i++)
        {
            if (i < 2)
                AssertEqual(buf[i], uint8_t(std::pow(2, i + 6)));
            else
                AssertMsg(buf[i] == uint8_t(std::pow(2, i)), "Buffer must not be overriden out of len");
        }

        bitarray.copyNegated(0, (uint8_t*)buf, sizeBytes);
        for (uint32_t i = 0; i < sizeBytes; ++i)
        {
            AssertEqual(buf[i], uint8_t(255 - std::pow(2, i)));
        }

        // check for overflow
        bitarray.copy(1024, (uint8_t*)buf, sizeBytes);
        bitarray.copyNegated(1024, (uint8_t*)buf, sizeBytes);

        AssertMsg(paddedBuf.padding1 == 0xBAADF00D, "Underflow check failed, stack is smashed");
        AssertMsg(paddedBuf.padding2 == 0xCAFEF00D, "Overflow check failed, stack is smashed");
    }

    TestCase(bitarray_BitArray_CorrectlyCreatesFromBuffer)
    {
        uint8_t buf[8];
        // buf looks like this:
        // map: 00000001 00000010 00000100...
        // idx: 76543210 54321098 32109876
        // so every 9th bit is set
        for (uint32_t i = 0; i < 8; i++)
        {
            buf[i] = (uint8_t)std::pow(2,i);
        }
        rpp::bit_array bitarray{buf, 8};
        AssertEqual(bitarray.sizeBytes(), 8U);
        for (uint32_t bit = 0; bit < bitarray.sizeBits(); ++bit)
        {
            AssertMsg(bitarray.isSet(bit) == (bit % 9 == 0), "Bit %d was copied wrong", bit);
        }
    }
};
