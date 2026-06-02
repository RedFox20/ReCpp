#include <rpp/endian.h>
#include <rpp/tests.h>
#include <cstdint>

TestImpl(test_endian)
{
    TestInit(test_endian)
    {
    }

    TestCase(big_endian_byte_order)
    {
        uint8_t buf[8] = {0};

        rpp::writeBEU16(buf, 0x1122);
        AssertEqual(buf[0], uint8_t(0x11));
        AssertEqual(buf[1], uint8_t(0x22));

        rpp::writeBEU32(buf, 0x11223344u);
        AssertEqual(buf[0], uint8_t(0x11));
        AssertEqual(buf[1], uint8_t(0x22));
        AssertEqual(buf[2], uint8_t(0x33));
        AssertEqual(buf[3], uint8_t(0x44));

        rpp::writeBEU64(buf, 0x1122334455667788ull);
        AssertEqual(buf[0], uint8_t(0x11));
        AssertEqual(buf[7], uint8_t(0x88));
    }

    TestCase(little_endian_byte_order)
    {
        uint8_t buf[8] = {0};

        rpp::writeLEU16(buf, 0x1122);
        AssertEqual(buf[0], uint8_t(0x22));
        AssertEqual(buf[1], uint8_t(0x11));

        rpp::writeLEU32(buf, 0x11223344u);
        AssertEqual(buf[0], uint8_t(0x44));
        AssertEqual(buf[3], uint8_t(0x11));

        rpp::writeLEU64(buf, 0x1122334455667788ull);
        AssertEqual(buf[0], uint8_t(0x88));
        AssertEqual(buf[7], uint8_t(0x11));
    }

    TestCase(roundtrip_all_widths)
    {
        uint8_t buf[8];

        rpp::writeBEU16(buf, 0xABCD);
        AssertEqual(rpp::readBEU16(buf), uint16_t(0xABCD));
        rpp::writeBEU32(buf, 0xABCD1234u);
        AssertEqual(rpp::readBEU32(buf), 0xABCD1234u);
        rpp::writeBEU64(buf, 0xABCD1234DEADBEEFull);
        AssertEqual(rpp::readBEU64(buf), 0xABCD1234DEADBEEFull);

        rpp::writeLEU16(buf, 0xABCD);
        AssertEqual(rpp::readLEU16(buf), uint16_t(0xABCD));
        rpp::writeLEU32(buf, 0xABCD1234u);
        AssertEqual(rpp::readLEU32(buf), 0xABCD1234u);
        rpp::writeLEU64(buf, 0xABCD1234DEADBEEFull);
        AssertEqual(rpp::readLEU64(buf), 0xABCD1234DEADBEEFull);
    }

    // The reason these helpers go through memcpy: they must work at unaligned offsets.
    // A reinterpret_cast<uint16_t*> store/load at an odd offset is undefined behaviour and
    // faults on strict-alignment targets (ARM/AVR32). Exercise every offset 1..7.
    TestCase(unaligned_offsets_roundtrip)
    {
        alignas(8) uint8_t buf[16] = {0};

        for (int off = 1; off <= 7; ++off)
        {
            uint8_t* p = buf + off;

            rpp::writeBEU16(p, 0x1234);
            AssertEqual(rpp::readBEU16(p), uint16_t(0x1234));
            AssertEqual(p[0], uint8_t(0x12)); // still big-endian at the offset
            AssertEqual(p[1], uint8_t(0x34));

            rpp::writeLEU32(p, 0xDEADBEEFu);
            AssertEqual(rpp::readLEU32(p), 0xDEADBEEFu);

            rpp::writeBEU64(p, 0x0102030405060708ull);
            AssertEqual(rpp::readBEU64(p), 0x0102030405060708ull);
        }
    }
};
