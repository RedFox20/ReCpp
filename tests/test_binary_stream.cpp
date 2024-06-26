#include <rpp/binary_stream.h>
#include <rpp/minmax.h>
#include <rpp/tests.h>
using namespace std::literals;

TestImpl(test_binary_stream)
{
    TestInit(test_binary_stream)
    {

    }

    TestCase(empty_buffer)
    {
        rpp::binary_buffer buf;

        AssertThat(buf.good(), false);
        AssertThat(buf.available(), 0);

        AssertThat(buf.read_int32(), 0);
        AssertThat(buf.read_int64(), 0LL);
        AssertThat(buf.peek_string(), "");
        Assert(buf.peek_wstring().empty());
        AssertThat(buf.peek_strview(), "");
        AssertThat(buf.read_string(), "");

        std::vector<std::string> noStrings;
        buf.read(noStrings);
        AssertThat(noStrings.empty(), true);
    }

    TestCase(integers)
    {
        rpp::binary_buffer buf;

        buf.write_byte(42);
        AssertThat(buf.available(), 1);
        AssertThat(buf.peek_byte(), 42);
        AssertThat(buf.available(), 1);
        AssertThat(buf.read_byte(), 42);
        AssertThat(buf.available(), 0);

        buf.write_int16(32000);
        AssertThat(buf.available(), 2);
        AssertThat(buf.peek_int16(), 32000);
        AssertThat(buf.available(), 2);
        AssertThat(buf.read_int16(), 32000);
        AssertThat(buf.available(), 0);

        buf.write_int32(42000000);
        AssertThat(buf.available(), 4);
        AssertThat(buf.peek_int32(), 42000000);
        AssertThat(buf.available(), 4);
        AssertThat(buf.read_int32(), 42000000);
        AssertThat(buf.available(), 0);

        buf.write_uint64(42000000000LL);
        AssertThat(buf.available(), 8);
        AssertThat(buf.peek_int64(), 42000000000LL);
        AssertThat(buf.available(), 8);
        AssertThat(buf.read_int64(), 42000000000LL);
        AssertThat(buf.available(), 0);
    }

    TestCase(strings)
    {
        rpp::binary_buffer buf;

        std::string s = "test string";
        int n = int(s.size());
        int lensz = sizeof(rpp::binary_buffer::strlen_t);
        buf.write(s);
        AssertThat(buf.available(), lensz + n);
        AssertThat(buf.peek_string(), "test string");
        AssertThat(buf.available(), lensz + n);
        AssertThat(buf.read_string(), "test string");
        AssertThat(buf.available(), 0);

        buf.write(std::string{});
        AssertThat(buf.available(), lensz);
        AssertThat(buf.peek_string(), "");
        AssertThat(buf.available(), lensz);
        AssertThat(buf.read_string(), "");
        AssertThat(buf.available(), 0);
    }

    TestCase(vectors)
    {
        rpp::binary_buffer buf;

        std::vector<int> intvec = { 10, 20, 30, 40, 50 };
        int n = int(intvec.size());
        buf.write(intvec);
        AssertThat(buf.available(), 4 + 4 * n);

        std::vector<int> intvec2;
        buf.read(intvec2);
        AssertThat(intvec2.size(), intvec.size());
        AssertThat(buf.available(), 0);
        for (size_t i = 0; i < intvec.size(); ++i)
            AssertThat(intvec2[i], intvec[i]);

        std::vector<std::string> strvec = { "test", "string", "vector" };
        buf.write(strvec);
        AssertNotEqual(buf.available(), 0);
        AssertNotEqual(buf.available(), 4);

        std::vector<std::string> strvec2;
        buf.read(strvec2);
        AssertThat(strvec2.size(), strvec.size());
        AssertThat(buf.available(), 0);
        for (size_t i = 0; i < strvec.size(); ++i)
            AssertThat(strvec2[i], strvec[i]);
    }

    template<int END, int CHUNK>
    class mock_source : public rpp::binary_stream, protected rpp::stream_source
    {
        int BytesServed = 0;
    public:
        mock_source() noexcept : binary_stream(this) {}
        bool stream_good() const noexcept override { return BytesServed < END; }
        int stream_write(const void* data, int numBytes) noexcept override { (void)data; (void)numBytes; return 0; }
        int stream_available() const noexcept override { return rpp::min(CHUNK, END - BytesServed); }
        void stream_flush() noexcept override { }
        int stream_read(void* dst, int max) noexcept override
        {
            int n = 0;
            for (; n < max && BytesServed < END && n < CHUNK; ++n)
                ((uint8_t*)dst)[n] = ++BytesServed;
            return n;
        }
        int stream_peek(void* dst, int max) noexcept override
        {
            int n = 0;
            int bytesServed = BytesServed;
            for (; n < max && bytesServed < END && n < CHUNK; ++n)
                ((uint8_t*)dst)[n] = ++bytesServed;
            return n;
        }
        void stream_skip(int n) noexcept override
        {
            BytesServed += n;
        }
    };

    TestCase(partial_read)
    {
        mock_source<1024, 16> buf;
        uint8_t tmp[128];

        buf.read(tmp, 40);
        for (int i = 0; i < 40; ++i)
            AssertThat(tmp[i], (uint8_t)(i+1));

        buf.read(tmp + 40, 40);
        for (int i = 40; i < 80; ++i)
            AssertThat(tmp[i], (uint8_t)(i + 1));
    }

    TestCase(file_read_write)
    {
        int count = 512;
        std::string file = rpp::temp_dir() + "/test.rpp.binary_stream.tmp";
        {
            rpp::file_writer out { file };
            for (int i = 0; i < count; ++i)
            {
                out.write_uint16(10);
                out.write(20.0f);
                out.write("test_string"s);
            }
        }

        rpp::file_reader in { file };
        for (int i = 0; i < count; ++i)
        {
            #define FileReaderAssertThat(expr, expected) do { \
                const auto& __expr   = expr;           \
                const auto& __expect = expected;       \
                if (!(__expr == __expect)) {           \
                    print_error("file_reader failed at index=%d  %s\n", i, #expr); \
                    assumption_failed(__FILE__, __LINE__, #expr, __expr, "but expected", __expect); \
                } \
            } while (false)

            FileReaderAssertThat(in.read_uint16(), 10);
            FileReaderAssertThat(in.read_float(), 20.0f);
            FileReaderAssertThat(in.read_string(), "test_string"s);
        }
        rpp::delete_file(file);
    }
};