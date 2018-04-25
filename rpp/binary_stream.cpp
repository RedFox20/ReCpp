#include "binary_stream.h"

namespace rpp
{
    ////////////////////////////////////////////////////////////////////////////

    // ReSharper disable CppPossiblyUninitializedMember
    binary_stream::binary_stream(stream_source* src) noexcept : Ptr(Buf), Src(src) // NOLINT
    {
    }
    binary_stream::binary_stream(int capacity, stream_source* src) noexcept : Ptr(Buf), Src(src) // NOLINT
    {
        if (capacity > SBSize)
            Ptr = (char*)malloc(capacity);
        Cap = capacity;
    }
    // ReSharper restore CppPossiblyUninitializedMember

    binary_stream::~binary_stream() noexcept
    {
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

    int binary_stream::available() const noexcept
    {
        return (End - ReadPos) + (Src ? Src->stream_available() : 0);
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
        if (Src)
            return Src->stream_good();
        return (End - ReadPos) > 0;
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
        flush_buffer();
        Src->stream_flush(); // now ask the source itself to flush the stuff
    }

    void binary_stream::flush_buffer()
    {
        if (WritePos) // were we writing something?
        {
            (void)Src->stream_write(Ptr, End);
            clear();
        }
    }

    void binary_stream::ensure_space(int numBytes)
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

    binary_stream& binary_stream::write(const void* data, int numBytes)
    {
        ensure_space(numBytes);
        memcpy(&Ptr[WritePos], data, (size_t)numBytes);
        WritePos += numBytes;
        End      += numBytes;
        return *this;
    }

    void binary_stream::unsafe_write(const void* data, int numBytes)
    {
        memcpy(&Ptr[WritePos], data, (size_t)numBytes);
        WritePos += numBytes;
        End      += numBytes;
    }

    ////////////////////////////////////////////////////////////////////////////

    int binary_stream::unsafe_buffer_fill()
    {
        int n = Src->stream_read(Ptr, Cap);
        ReadPos = 0;
        End = WritePos = (n > 0) ? n : 0;
        return End;
    }


    int binary_stream::unsafe_buffer_read(void* dst, int bytesToRead)
    {
        memcpy(dst, &Ptr[ReadPos], (size_t)bytesToRead);
        ReadPos += bytesToRead;
        return bytesToRead;
    }

    int binary_stream::fragmented_read(void* dst, int bytesToRead)
    {
        int total = 0;

        // first use everything we can from buffer, n is assumed to always be < bytesToRead
        if (int n = size())
        {
            memcpy(dst, &Ptr[ReadPos], (size_t)n);
            clear();
            total += n;
            bytesToRead -= n; // and this will never be 0
        }

        // if there's no underlying source, we're done reading
        if (!Src)
            return total;

        // try to fill the buffer with some data if it's less than 2/3 of max capacity
        if (bytesToRead < (Cap*2)/3)
        {
            // buffer_fill might not give Cap bytes, so we still need to loop
            while (bytesToRead > 0)
            {
                int n = min<int>(unsafe_buffer_fill(), bytesToRead);
                if (n <= 0) break;
                total += unsafe_buffer_read((char*)dst + total, n);
                bytesToRead -= n;
            }
            return total;
        }

        // there's no point trying to buffer the data, read as much as we can from stream
        while (bytesToRead > 0)
        {
            int n = Src->stream_read((char*)dst + total, bytesToRead);
            if (n <= 0) break;
            total += n;
            bytesToRead -= n;
        }
        return total;
    }

    int binary_stream::read(void* dst, int bytesToRead)
    {
        if (size() >= bytesToRead)
            return unsafe_buffer_read(dst, bytesToRead); // best case, all from buffer
        return fragmented_read(dst, bytesToRead);
    }

    int binary_stream::peek(void* dst, int bytesToPeek)
    {
        int avail = size();
        if (avail <= 0)
        {
            if (!Src) return 0;
            avail = unsafe_buffer_fill(); // fill before peek if possible
        }
        if (avail < bytesToPeek)
            return 0;
        memcpy(dst, &Ptr[ReadPos], bytesToPeek);
        return bytesToPeek;
    }

    strview binary_stream::peek_strview()
    {
        strview s;
        int avail = size();
        if (avail <= 0)
        {
            if (!Src) return s;
            avail = unsafe_buffer_fill();
        }
        if (avail < (int)sizeof(strlen_t))
            return s;
        s.len = min<int>(peek<strlen_t>(), avail - sizeof(strlen_t));
        s.str = &Ptr[ReadPos + sizeof(strlen_t)];
        undo(sizeof(strlen_t));
        return s;
    }

    void binary_stream::skip(int n)
    {
        int nskip = min<int>(n, size()); // max skippable: Size
        ReadPos += nskip;
        if (Src && nskip < n)
            Src->stream_skip(n - nskip); // skip remaining from storage
    }

    void binary_stream::undo(int n)
    {
        int nundo = min<int>(n, ReadPos); // max undoable:  Pos
        ReadPos -= nundo;
    }


    ////////////////////////////////////////////////////////////////////////////


#ifndef RPP_BINARY_READWRITE_NO_SOCKETS

    //// -- SOCKET WRITER -- ////

    socket_writer::~socket_writer() noexcept
    {
        try { flush(); } catch (...) {}
    }

    int socket_writer::stream_write(const void* data, int numBytes) noexcept
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

    //// -- SOCKET READER -- ////
    
    socket_reader::~socket_reader() noexcept = default;

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

    file_writer::~file_writer() noexcept
    {
        try { flush(); } catch (...) {}
    }

    int file_writer::stream_write(const void* data, int numBytes) noexcept
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
    
    file_reader::~file_reader() noexcept = default;

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
