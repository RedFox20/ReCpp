#include "binary_readwrite.h"

namespace rpp
{
    ////////////////////////////////////////////////////////////////////////////

    binary_stream::binary_stream(stream_source* src) noexcept : Ptr(Buf), Src(src)
    {
    }

    binary_stream::binary_stream(int capacity, stream_source* src) noexcept : Ptr(Buf), Src(src)
    {
        if (capacity > SBSize)
            Ptr = (char*)malloc(capacity);
        Cap = capacity;
    }

    binary_stream::~binary_stream() noexcept
    {
        try { flush(); } catch (...) {}
        if (Cap > SBSize)
            free(Ptr);
    }

    void binary_stream::disable_buffering()
    {
        flush();
        if (Cap > SBSize)
            free(Ptr);
        Cap = 0;
    }

    void binary_stream::clear()
    {
        Pos  = 0;
        Size = 0;
    }

    void binary_stream::rewind(int pos)
    {
        int streamEnd = end();
        Pos  = pos < 0 ? 0 : pos <= streamEnd ? pos : streamEnd;
        Size = streamEnd - Pos;
    }

    bool binary_stream::good() const
    {
        return !Src || Src->stream_good();
    }

    void binary_stream::reserve(int capacity)
    {
        if (capacity == 0)
        {
            if (Cap > SBSize)
            {
                free(Ptr);
                Ptr = Buf;
            }
            Pos  = 0;
            Size = 0;
        }
        else if (capacity > SBSize)
        {
            if (Cap > SBSize) // realloc dynamic buffer
            {
                Ptr = (char*)realloc(Ptr, capacity);
            }
            else // change from local buffer to dynamic
            {
                Ptr = (char*)malloc(capacity);
                if (Size) {
                    memcpy(Ptr, Buf, Size);
                }
            }
        }
        Cap = capacity;
    }

    void binary_stream::flush() 
    {
        if (int numBytes = Pos && Src)
        {
            int written = Src->stream_write(Ptr, numBytes);
            if (written != numBytes)
            {
                // @todo Write failed. How do we recover?
            }
            Pos = 0;
        }
    }

    void binary_stream::ensure_space(uint numBytes)
    {
        int newlen = Pos + Size + numBytes;
        if (newlen > Cap)
        {
            int align = Cap;
            int newcap = newlen + align;
            if (int rem = newcap % align)
                newcap += align - rem;
            reserve(newcap);
        }
    }

    binary_stream& binary_stream::write(const void* data, uint numBytes)
    {
        ensure_space(numBytes);
        memcpy(&Ptr[Pos], data, (size_t)numBytes);
        Pos  += numBytes;
        Size += numBytes;
        return *this;
    }

    ////////////////////////////////////////////////////////////////////////////

    void binary_stream::unsafe_buffer_fill()
    {
        int n = Src->stream_read(Ptr, Cap);
        Pos  = 0;
        Size = (n > 0) ? n : 0;
    }


    uint binary_stream::unsafe_buffer_read(void* dst, uint bytesToRead)
    {
        memcpy(dst, &Ptr[Pos], (size_t)bytesToRead);
        Pos  += bytesToRead;
        Size -= bytesToRead;
        return bytesToRead;
    }

    uint binary_stream::fragmented_read(void* dst, uint bytesToRead, uint bufferBytes)
    {
        uint remainingBytes = bytesToRead;
        if (bufferBytes) // available < bytesToRead,  buffer has less data, so it's a fragmented/partial fill
        {
            memcpy(dst, &Ptr[Pos], (size_t)bufferBytes);
            Pos  = 0;
            Size = 0;
            remainingBytes -= bufferBytes; // and this will never be 0
        }

        // if there's no underlying source, we're done reading
        if (!Src)
            return bufferBytes;

        unsafe_buffer_fill();
        return bufferBytes + Src->stream_read((char*)dst + bufferBytes, remainingBytes);
    }

    uint binary_stream::read(void* dst, uint bytesToRead)
    {
        uint available = Size;
        if (available >= bytesToRead)
            return unsafe_buffer_read(dst, bytesToRead); // best case, all from buffer
        return fragmented_read(dst, bytesToRead, available);
    }

    uint binary_stream::peek(void* dst, uint cnt)
    {
        if (Size <= 0)
        {
            if (!Src) return 0;
            unsafe_buffer_fill(); // fill before peek if possible
        }
        if ((uint)Size <= cnt)
            return 0;
        memcpy(dst, &Ptr[Pos], cnt);
        return cnt;
    }

    void binary_stream::skip(uint n)
    {
        uint nskip = min<uint>(n, Size); // max skippable: Size
        Pos  += nskip;
        Size -= nskip;
        if (Src && nskip < n)
            Src->stream_skip(n - nskip); // skip remaining from storage
    }

    void binary_stream::undo(uint n)
    {
        uint nundo = min<uint>(n, Pos); // max undoable:  Pos
        Pos  -= nundo;
        Size += nundo;
    }

    ////////////////////////////////////////////////////////////////////////////

#ifndef RPP_BINARY_READWRITE_NO_SOCKETS

    int socket_stream::stream_write(const void* data, uint numBytes) noexcept
    {
        if (stream_good())
            return Sock->send(data, numBytes);
        return -1;
    }
    void socket_stream::stream_flush() noexcept
    {
        if (stream_good())
            Sock->flush();
    }
    int socket_stream::stream_read(void* dst, int max) noexcept
    {
        if (stream_good())
            return Sock->recv(dst, max);
        return -1;
    }
    int socket_stream::stream_peek(void* dst, int max) noexcept
    {
        if (stream_good())
            return Sock->peek(dst, max);
        return -1;
    }
    void socket_stream::stream_skip(int n) noexcept
    {
        if (stream_good())
            Sock->skip(n);
    }

#endif

    ////////////////////////////////////////////////////////////////////////////

#ifndef RPP_BINARY_READWRITE_NO_FILE_IO

    int file_stream::stream_write(const void* data, uint numBytes) noexcept
    {
        if (stream_good())
            return File->write(data, numBytes);
        return -1;
    }
    void file_stream::stream_flush() noexcept
    {
        if (stream_good())
            File->flush();
    }
    int file_stream::stream_read(void* dst, int max) noexcept
    {
        if (stream_good())
            return File->read(dst, max);
        return -1;
    }
    int file_stream::stream_peek(void* dst, int max) noexcept
    {
        if (stream_good())
        {
            int pos = File->tell();
            int ret = File->read(dst, max);
            File->seek(pos, SEEK_SET);
            return ret;
        }
        return -1;
    }
    void file_stream::stream_skip(int n) noexcept
    {
        if (stream_good())
            File->seek(n, SEEK_CUR);
    }

#endif

    ////////////////////////////////////////////////////////////////////////////
}