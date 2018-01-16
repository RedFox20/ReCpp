#include "binary_stream.h"
#include <stdlib.h>

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
        ReadPos  = 0;
        WritePos = 0;
        End      = 0;
    }

    void binary_stream::rewind(int pos)
    {
        ReadPos = WritePos = pos < 0 ? 0 : pos <= End ? pos : End;
    }

    bool binary_stream::good() const
    {
        return !Src || Src->stream_good();
    }

    void binary_stream::reserve(int capacity)
    {
        if (capacity == 0)
        {
            if (Cap > SBSize) // revert back to local buffer
            {
                free(Ptr);
                Ptr = Buf;
            }
            clear();
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
                if (uint bytes = size()) {
                    memcpy(Ptr, Buf, (size_t)bytes);
                }
            }
        }
        Cap = capacity;
    }

    void binary_stream::flush() 
    {
        if (!Src) return;
        if (uint numBytes = size())
        {
            int written = Src->stream_write(data(), numBytes);
            if (written != (int)numBytes)
            {
                // @todo Write failed. How do we recover?
            }
            clear();
        }
        // now ask the source itself to flush the stuff

    }

    void binary_stream::ensure_space(uint numBytes)
    {
        int newlen = size() + numBytes;
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
        memcpy(&Ptr[WritePos], data, (size_t)numBytes);
        WritePos += numBytes;
        End      += numBytes;
        return *this;
    }

    ////////////////////////////////////////////////////////////////////////////

    void binary_stream::unsafe_buffer_fill()
    {
        int n = Src->stream_read(Ptr, Cap);
        ReadPos = 0;
        End = WritePos = (n > 0) ? n : 0;
    }


    uint binary_stream::unsafe_buffer_read(void* dst, uint bytesToRead)
    {
        memcpy(dst, &Ptr[ReadPos], (size_t)bytesToRead);
        ReadPos += bytesToRead;
        return bytesToRead;
    }

    uint binary_stream::fragmented_read(void* dst, uint bytesToRead, uint bufferBytes)
    {
        uint remainingBytes = bytesToRead;
        if (bufferBytes) // bufferBytes < bytesToRead,  buffer has less data, so it's a fragmented/partial fill
        {
            memcpy(dst, &Ptr[ReadPos], (size_t)bufferBytes);
            clear();
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
        uint bufferBytes = size();
        if (bufferBytes >= bytesToRead)
            return unsafe_buffer_read(dst, bytesToRead); // best case, all from buffer
        return fragmented_read(dst, bytesToRead, bufferBytes);
    }

    uint binary_stream::peek(void* dst, uint cnt)
    {
        if (size() <= 0)
        {
            if (!Src) return 0;
            unsafe_buffer_fill(); // fill before peek if possible
        }
        if (size() <= cnt)
            return 0;
        memcpy(dst, &Ptr[ReadPos], cnt);
        return cnt;
    }

    void binary_stream::skip(uint n)
    {
        uint nskip = min<uint>(n, size()); // max skippable: Size
        ReadPos += nskip;
        if (Src && nskip < n)
            Src->stream_skip(n - nskip); // skip remaining from storage
    }

    void binary_stream::undo(uint n)
    {
        uint nundo = min<uint>(n, ReadPos); // max undoable:  Pos
        ReadPos -= nundo;
    }


    ////////////////////////////////////////////////////////////////////////////


#ifndef RPP_BINARY_READWRITE_NO_SOCKETS

    //// -- SOCKET WRITER -- ////

    int socket_writer::stream_write(const void* data, uint numBytes) noexcept
    {
        if (stream_good())
            return Sock->send(data, numBytes);
        return -1;
    }
    void socket_writer::stream_flush() noexcept
    {
        if (stream_good())
            Sock->flush();
    }

    //// -- FILE READER -- ////

    void socket_reader::stream_flush() noexcept
    {
        if (stream_good())
            Sock->flush();
    }
    int socket_reader::stream_read(void* dst, int max) noexcept
    {
        if (stream_good())
        {
            if (Sock->type() != ST_Stream)
                return Sock->recvfrom(Addr, dst, max);
            return Sock->recv(dst, max);
        }
        return -1;
    }
    int socket_reader::stream_peek(void* dst, int max) noexcept
    {
        if (stream_good())
            return Sock->peek(dst, max);
        return -1;
    }
    void socket_reader::stream_skip(int n) noexcept
    {
        if (stream_good())
            Sock->skip(n);
    }

#endif


    ////////////////////////////////////////////////////////////////////////////


#ifndef RPP_BINARY_READWRITE_NO_FILE_IO

    //// -- FILE WRITER -- ////

    int file_writer::stream_write(const void* data, uint numBytes) noexcept
    {
        if (stream_good())
            return File->write(data, numBytes);
        return -1;
    }
    void file_writer::stream_flush() noexcept
    {
        if (stream_good())
            File->flush();
    }

    //// -- FILE READER -- ////

    void file_reader::stream_flush() noexcept
    {
        if (stream_good())
            File->flush();
    }
    int file_reader::stream_read(void* dst, int max) noexcept
    {
        if (stream_good())
            return File->read(dst, max);
        return -1;
    }
    int file_reader::stream_peek(void* dst, int max) noexcept
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
    void file_reader::stream_skip(int n) noexcept
    {
        if (stream_good())
            File->seek(n, SEEK_CUR);
    }

#endif

    ////////////////////////////////////////////////////////////////////////////
}