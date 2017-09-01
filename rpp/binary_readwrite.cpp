#include "binary_readwrite.h"

namespace rpp
{
    ////////////////////////////////////////////////////////////////////////////

    stream_buffer::stream_buffer() noexcept : Ptr(Buf)
    {
    }

    stream_buffer::stream_buffer(int capacity) noexcept : Ptr(Buf)
    {
        if (capacity > MAX)
        {
            Ptr = (char*)malloc(capacity);
        }
        Cap = capacity;
    }

    stream_buffer::~stream_buffer() noexcept
    {
        if (Cap > MAX) free(Ptr);
    }

    void stream_buffer::reserve_buffer(int capacity) noexcept
    {
        if (capacity == 0)
        {
            if (Cap > MAX)
            {
                free(Ptr); Ptr = Buf;
            }
        }
        else if (capacity > MAX)
        {
            if (Cap > MAX) // realloc dynamic buffer
            {
                Ptr = (char*)realloc(Ptr, capacity);
            }
            else // change from local buffer to 
            {
                Ptr = (char*)malloc(capacity);
                if (Pos) memcpy(Ptr, Buf, Pos);
            }
        }
        Cap = capacity;
    }

    ////////////////////////////////////////////////////////////////////////////

    binary_writer::binary_writer() noexcept : stream_buffer()
    {
    }

    binary_writer::binary_writer(int capacity) noexcept : stream_buffer(capacity)
    {
    }

    binary_writer::~binary_writer() noexcept
    {
        flush();
    }

    void binary_writer::reserve(int capacity)
    {
        flush();
        reserve_buffer(capacity);
    }

    void binary_writer::flush() 
    {
        if (int numBytes = Pos)
        {
            int written = unbuffered_write(Ptr, numBytes);
            if (written != numBytes)
            {
                // @todo Write failed. How do we recover?
            }
            Pos = 0;
        }
    }

    bool binary_writer::check_unbuffered(const void* data, uint numBytes)
    {
        if (uint(Cap - Pos) < numBytes)
        {
            flush(); // forced flush
        }
        if (numBytes >= (uint)Cap)
        {
            unbuffered_write(data, numBytes);
            return false;
        }
        return true;
    }

    binary_writer& binary_writer::write(const void* data, uint numBytes)
    {
        if (check_unbuffered(data, numBytes))
        {
            memcpy(&Buf[Pos], data, (size_t)numBytes);
            Pos += numBytes;
        }
        return *this;
    }

    ////////////////////////////////////////////////////////////////////////////

    binary_reader::binary_reader() noexcept : stream_buffer()
    {
    }

    binary_reader::binary_reader(int capacity) noexcept : stream_buffer(capacity)
    {
    }

    binary_reader::~binary_reader() noexcept
    {
        flush();
    }

    void binary_reader::reserve(int capacity)
    {
        flush();
        reserve_buffer(capacity);
    }

    void binary_reader::flush()
    {
        Pos = 0;
        Rem = 0;
        if (stream_available()) stream_flush();
    }

    void binary_reader::buf_fill()
    {
        int n = stream_read(Buf, Cap);
        Pos = 0;
        Rem = (n > 0) ? n : 0;
    }

    uint binary_reader::buf_read(void* dst, uint cnt)
    {
        uint n = min<uint>(cnt, Rem);
        memcpy(dst, &Buf[Pos], cnt);
        Pos += n;
        Rem -= n;
        return n;
    }

    uint binary_reader::read(void* dst, uint cnt)
    {
        uint n = Rem;
        if (n >= cnt) return buf_read(dst, cnt); // best case, all from buffer
        return _read(&dst, cnt, n);              // fallback partial read
    }

    uint binary_reader::_read(void* dst, uint cnt, uint bufn)
    {
        uint rem = cnt - buf_read(dst, bufn); // read some from buffer; won't read all(!)
        buf_fill();
        return bufn + buf_read((char*)dst + bufn, rem); // refill and read from buf
    }

    uint binary_reader::peek(void* dst, uint cnt)
    {
        if (Rem <= 0) buf_fill();  // fill before peek if possible
        memcpy(dst, &Buf[Pos], cnt);
        return cnt;
    }

    void binary_reader::skip(uint n)
    {
        uint nskip = min<uint>(n, Rem); // max skippable: Rem
        Pos += nskip;
        Rem -= nskip;
        if (nskip < n)
            stream_skip(n - nskip); // skip remaining from storage
    }

    void binary_reader::undo(uint n)
    {
        uint nundo = min<uint>(n, Pos); // max undoable:  Pos
        Pos -= nundo;
        Rem += nundo;
    }

    ////////////////////////////////////////////////////////////////////////////

#ifndef RPP_BINARY_READWRITE_NO_SOCKETS

    int socket_writer::unbuffered_write(const void* data, uint numBytes)
    {
        if (good())
            return Sock->send(data, numBytes);
        return -1;
    }
    int socket_reader::stream_available() const noexcept
    {
        if (good())
            return Sock->available();
        return -1;
    }
    int socket_reader::stream_read(void* dst, int max) noexcept
    {
        if (good())
            return Sock->recv(dst, max);
        return -1;
    }
    int socket_reader::stream_peek(void* dst, int max) noexcept
    {
        if (good())
            return Sock->peek(dst, max);
        return -1;
    }
    void socket_reader::stream_flush() noexcept
    {
        if (good())
            Sock->flush();
    }
    void socket_reader::stream_skip(int n) noexcept
    {
        if (good())
            Sock->skip(n);
    }

#endif

    ////////////////////////////////////////////////////////////////////////////

#ifndef RPP_BINARY_READWRITE_NO_FILE_IO

    int file_writer::unbuffered_write(const void* data, uint numBytes)
    {
        if (good())
            return File->write(data, numBytes);
        return -1;
    }
    int file_reader::stream_available() const noexcept
    {
        if (good())
        {
            int pos  = File->tell();
            int size = File->size();
            return size - pos;
        }
        return -1;
    }
    int file_reader::stream_read(void* dst, int max) noexcept
    {
        if (good())
            return File->read(dst, max);
        return -1;
    }
    int file_reader::stream_peek(void* dst, int max) noexcept
    {
        if (good())
        {
            int pos = File->tell();
            int ret = File->read(dst, max);
            File->seek(pos, SEEK_SET);
            return ret;
        }
        return -1;
    }
    void file_reader::stream_flush() noexcept
    {
        if (good())
            File->flush();
    }
    void file_reader::stream_skip(int n) noexcept
    {
        if (good())
            File->seek(n, SEEK_CUR);
    }

#endif

    ////////////////////////////////////////////////////////////////////////////
}