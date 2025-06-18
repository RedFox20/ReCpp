#include "file_io.h"
#include "strview.h"
#include <cstdlib> // malloc
#include <cstdio> // fopen
#include <cstdarg> // va_list

#include "paths.inl"

#if __APPLE__
    #include <TargetConditionals.h> // TARGET_OS_IPHONE
#endif
#if __ANDROID__
    #include <jni.h>
#endif

namespace rpp /* ReCpp */
{
    load_buffer::~load_buffer()
    {
        if (str) free(str); // MEM_RELEASE
    }
    load_buffer::load_buffer(load_buffer&& mv) noexcept : str(mv.str), len(mv.len)
    {
        mv.str = nullptr;
        mv.len = 0;
    }
    load_buffer& load_buffer::operator=(load_buffer&& mv) noexcept
    {
        char* p = str;
        int   l = len;
        str = mv.str;
        len = mv.len;
        mv.str = p;
        mv.len = l;
        return *this;
    }
    // acquire the data pointer of this load_buffer, making the caller own the buffer
    char* load_buffer::steal_ptr() noexcept
    {
        char* p = str;
        str = nullptr;
        return p;
    }


    ////////////////////////////////////////////////////////////////////////////////

#if _MSC_VER
    template<StringViewType T>
    static void* OpenF(T fname, int a, int s, SECURITY_ATTRIBUTES* sa, int c, int o)
    {
        if (wchar_conv conv { fname })
            return CreateFileW(conv.wstr, a, s, sa, c, o, nullptr);
        return nullptr;
    }
#else
    template<StringViewType T>
    static void* OpenF(T fname, file::mode mode)
    {
        if (multibyte_conv conv { fname })
        {
            if (mode == file::READWRITE) {
                if (FILE* file = fopen(conv.cstr, "rb+")) // open existing file for read/write
                    return file;
                mode = file::CREATENEW;
            }
            const char* modes[] = { "rb", "", "wb+", "ab" };
            return fopen(conv.cstr, modes[static_cast<int>(mode)]);
        }
        return nullptr;
    }
#endif

    template<StringViewType T>
    static void* OpenFile(T filename, file::mode mode) noexcept
    {
    #if _MSC_VER
        int access, sharing;        // FILE_SHARE_READ, FILE_SHARE_WRITE
        int createmode, openFlags;	// OPEN_EXISTING, OPEN_ALWAYS, CREATE_ALWAYS
        switch (mode)
        {
            default:
            case file::mode::READONLY:
                access     = FILE_GENERIC_READ;
                sharing    = FILE_SHARE_READ | FILE_SHARE_WRITE;
                createmode = OPEN_EXISTING;
                openFlags  = FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN;
                break;
            case file::mode::READWRITE:
                access     = FILE_GENERIC_READ|FILE_GENERIC_WRITE;
                sharing    = FILE_SHARE_READ;
                createmode = OPEN_ALWAYS; // create file if it doesn't exist
                openFlags  = FILE_ATTRIBUTE_NORMAL;
                break;
            case file::mode::CREATENEW:
                access     = FILE_GENERIC_READ|FILE_GENERIC_WRITE|DELETE;
                sharing    = FILE_SHARE_READ;
                createmode = CREATE_ALWAYS;
                openFlags  = FILE_ATTRIBUTE_NORMAL;
                break;
            case file::mode::APPEND:
                access     = FILE_APPEND_DATA;
                sharing    = FILE_SHARE_READ;
                createmode = OPEN_ALWAYS;
                openFlags  = FILE_ATTRIBUTE_NORMAL;
                break;
        }
        SECURITY_ATTRIBUTES secu = { sizeof(secu), nullptr, TRUE };
        void* handle = OpenF<T>(filename, access, sharing, &secu, createmode, openFlags);
        return handle != INVALID_HANDLE_VALUE ? handle : nullptr;
    #else
        return OpenF<T>(filename, mode);
    #endif
    }

    template<StringViewType T>
    static void* OpenOrCreate(T filename, file::mode mode) noexcept
    {
        void* handle = OpenFile(filename, mode);
        if (!handle && (mode == file::CREATENEW || mode == file::APPEND))
        {
            // assume the directory doesn't exist
            if (create_folder(folder_path(filename))) {
                return OpenFile(filename, mode); // last chance
            }
        }
        return handle;
    }

    file::file(strview filename, mode mode) noexcept : Handle{OpenOrCreate(filename, mode)}, Mode{mode}
    {
    }
#if RPP_ENABLE_UNICODE
    file::file(ustrview filename, mode mode) noexcept : Handle{OpenOrCreate(filename, mode)}, Mode{mode}
    {
    }
#endif // RPP_ENABLE_UNICODE
    file::file(file&& f) noexcept : Handle(f.Handle), Mode(f.Mode)
    {
        f.Handle = nullptr;
    }
    file::~file()
    {
        close();
    }
    file& file::operator=(file&& f) noexcept
    {
        close();
        Handle = f.Handle;
        Mode = f.Mode;
        f.Handle = nullptr;
        return *this;
    }
    bool file::open(strview filename, mode mode) noexcept
    {
        close();
        Mode = mode;
        return (Handle = OpenOrCreate<strview>(filename, mode)) != nullptr;
    }
#if RPP_ENABLE_UNICODE
    bool file::open(ustrview filename, mode mode) noexcept
    {
        close();
        Mode = mode;
        return (Handle = OpenOrCreate<ustrview>(filename, mode)) != nullptr;
    }
#endif // RPP_ENABLE_UNICODE
    void file::close() noexcept
    {
        if (Handle)
        {
        #if _MSC_VER
            CloseHandle(reinterpret_cast<HANDLE>(Handle));
        #else
            fclose((FILE*)Handle);
        #endif
            Handle = nullptr;
        }
    }
    int file::os_handle() const noexcept
    {
    #if _MSC_VER
        return (int)(intptr_t)Handle;
    #else
        return fileno((FILE*)Handle);
    #endif
    }
    bool file::good() const noexcept
    {
        return Handle != nullptr;
    }
    bool file::bad() const noexcept
    {
        return Handle == nullptr;
    }
    int file::size() const noexcept
    {
        if (!Handle) return 0;
    #if _MSC_VER
        return GetFileSize(reinterpret_cast<HANDLE>(Handle), nullptr);
    #elif __ANDROID__ || TARGET_OS_IPHONE
        struct stat s;
        if (fstat(fileno((FILE*)Handle), &s)) {
            //fprintf(stderr, "fstat error: [%s]\n", strerror(errno));
            return 0;
        }
        return (int)s.st_size;
    #else // Linux version is more complicated. It won't report correct size unless file is flushed:
        int pos = tell();
        int size = const_cast<file*>(this)->seek(0, SEEK_END);
        const_cast<file*>(this)->seek(pos, SEEK_SET);
        return size;
    #endif
    }
    int64 file::sizel() const noexcept
    {
        if (!Handle) return 0;
    #if _MSC_VER
        LARGE_INTEGER size;
        if (!GetFileSizeEx(reinterpret_cast<HANDLE>(Handle), &size)) {
            //fprintf(stderr, "GetFileSizeEx error: [%d]\n", GetLastError());
            return 0ull;
        }
        return static_cast<int64>(size.QuadPart);
    #elif __ANDROID__ || __APPLE__
        struct stat64 s;
        if (_fstat64(fileno((FILE*)Handle), &s)) {
            //fprintf(stderr, "_fstat64 error: [%s]\n", strerror(errno));
            return 0ull;
        }
        return (int64)s.st_size;
    #else // Linux version is more complicated. It won't report correct size unless file is flushed:
        int64 pos = tell64();
        int64 size = const_cast<file*>(this)->seekl(0LL, SEEK_END);
        const_cast<file*>(this)->seekl(pos, SEEK_SET);
        return size;
    #endif
    }
    // ReSharper disable once CppMemberFunctionMayBeConst
    int file::read(void* buffer, int bytesToRead) noexcept
    {
        if (!Handle) return 0;
    #if _MSC_VER
        DWORD bytesRead;
        (void)ReadFile(reinterpret_cast<HANDLE>(Handle), buffer, bytesToRead, &bytesRead, nullptr);
        return bytesRead;
    #else
        return (int)fread(buffer, 1, (size_t)bytesToRead, (FILE*)Handle);
    #endif
    }
    load_buffer file::read_all() noexcept
    {
        int fileSize = size();
        if (fileSize > 0)
        {
            // allocate +1 bytes for null terminator; this is for legacy API-s
            if (auto buffer = static_cast<char*>(malloc(size_t(fileSize) + 1u)))
            {
                int bytesRead = read(buffer, fileSize);
                buffer[bytesRead] = '\0';
                return load_buffer{ buffer, bytesRead };
            }
        }
        return load_buffer{ nullptr, 0 };
    }
    std::string file::read_text() noexcept
    {
        std::string out;
        int pos   = tell();
        int count = size() - pos;
        if (count <= 0) return out;

        out.resize(size_t(count));

        int n = read(const_cast<char*>(out.data()), count);
        if (n != count) {
            out.resize(size_t(n));
            out.shrink_to_fit();
        }
        return out;
    }
    bool file::save_as(strview filename) noexcept
    {
        file dst { filename, CREATENEW };
        if (!dst) return false;

        int64 size = sizel();
        if (size == 0) return true;

        int64 startPos = tell64();
        seek(0);

        constexpr int64 blockSize = 64*1024;
        char buf[blockSize];
        int64 totalBytesRead    = 0;
        int64 totalBytesWritten = 0;
        for (;;)
        {
            int64 bytesToRead = std::min(size - totalBytesRead, blockSize);
            if (bytesToRead <= 0)
                break;
            int bytesRead = read(buf, static_cast<int>(bytesToRead));
            if (bytesRead <= 0)
                break;
            totalBytesRead    += bytesRead;
            totalBytesWritten += dst.write(buf, bytesRead);
        }
        
        seekl(startPos);
        return totalBytesRead == totalBytesWritten;
    }

    // ReSharper disable once CppMemberFunctionMayBeConst
    int file::write(const void* buffer, int bytesToWrite) noexcept
    {
        if (bytesToWrite <= 0)
            return 0;
        if (!Handle)
            return 0;
        
    #if _MSC_VER
        DWORD bytesWritten;
        WriteFile(reinterpret_cast<HANDLE>(Handle), buffer, bytesToWrite, &bytesWritten, nullptr);
        return bytesWritten;
    #elif WIN32
        // MSVC writes to buffer byte by byte, so to get decent performance, flip the count
        int result = (int)fwrite(buffer, bytesToWrite, 1, (FILE*)Handle);
        return result > 0 ? result * bytesToWrite : result;
    #else
        return (int)fwrite(buffer, 1, bytesToWrite, (FILE*)Handle);
    #endif
    }
    int file::writef(PRINTF_FMTSTR const char* format, ...) noexcept
    {
        if (!Handle)
            return 0;
        va_list ap; va_start(ap, format);
    #if _MSC_VER // @note This is heavily optimized
        char buf[4096];
        int n = vsnprintf(buf, sizeof(buf), format, ap);
        if (n >= static_cast<int>(sizeof(buf)))
        {
            const int n2 = n + 1;
            const bool heap = (n2 > 64 * 1024);
            auto b2 = static_cast<char*>(heap ? malloc(n2) : _alloca(n2)); // NOLINT
            n = this->write(b2, vsnprintf(b2, n2, format, ap));
            if (heap) free(b2);
            return n;
        }
        int written = this->write(buf, n);
    #else
        int written = vfprintf((FILE*)Handle, format, ap);
    #endif
        va_end(ap);
        return written;
    }

    int file::writeln() noexcept
    {
        return write("\n", 1);
    }

    void file::truncate_front(int64 newLength) noexcept
    {
        int64 len = sizel();
        if (len <= newLength)
            return;

        int64 bytesToTrunc = len - newLength;
        seekl(bytesToTrunc, SEEK_SET);
        
        std::vector<char> buf; buf.resize(static_cast<size_t>(newLength));
        int bytesRead = read(buf.data(), static_cast<int>(newLength));

        truncate(newLength);
        seek(0, SEEK_SET);
        write(buf.data(), bytesRead);
    }

    void file::truncate_end(int64 newLength) noexcept
    {
        int64 len = sizel();
        if (len <= newLength)
            return;
        truncate(newLength);
    }

    void file::truncate(int64 newLength) noexcept
    {
        if (!Handle) return;
    #if _MSC_VER
        seekl(newLength, SEEK_SET);
        SetEndOfFile(reinterpret_cast<HANDLE>(Handle));
    #elif _MSC_VER
        _chsize_s(fileno((FILE*)Handle), newLength);
    #else
        int result = ftruncate(fileno((FILE*)Handle), (off_t)newLength);
        (void)result;
    #endif
    }

    bool file::preallocate(int64 preallocSize, int64 seekPos, int seekMode) noexcept
    {
        if (!Handle || preallocSize <= 0) return false;
    #if _MSC_VER
        // TODO: implement preallocation for Windows
        (void)seekPos;
        (void)seekMode;
        return false;
    #else
        int fd = fileno((FILE*)Handle);
        if (fd < 0)
            return false; // invalid file descriptor
        if (fallocate(fd, 0, 0, preallocSize) != 0)
            return false; // preallocation failed
        seekl(seekPos, seekMode);
        return true; // preallocation succeeded
    #endif
    }

    // ReSharper disable once CppMemberFunctionMayBeConst
    void file::flush() noexcept
    {
        if (!Handle) return;
    #if _MSC_VER
        FlushFileBuffers(reinterpret_cast<HANDLE>(Handle));
    #else
        fflush((FILE*)Handle);
    #endif
    }
    int file::write_new(strview filename, const void* buffer, int bytesToWrite) noexcept
    {
        file f { filename, mode::CREATENEW };
        return f.write(buffer, bytesToWrite);
    }
    
#if RPP_ENABLE_UNICODE
    int file::write_new(ustrview filename, const void* buffer, int bytesToWrite) noexcept
    {
        file f { filename, mode::CREATENEW };
        return f.write(buffer, bytesToWrite);
    }
#endif // RPP_ENABLE_UNICODE

    // ReSharper disable once CppMemberFunctionMayBeConst
    int file::seek(int filepos, int seekmode) noexcept
    {
        if (!Handle)
            return 0;
    #if _MSC_VER
        return SetFilePointer(reinterpret_cast<HANDLE>(Handle), filepos, nullptr, seekmode);
    #else
        fseek((FILE*)Handle, filepos, seekmode);
        return (int)ftell((FILE*)Handle);
    #endif
    }

    // ReSharper disable once CppMemberFunctionMayBeConst
    uint64 file::seekl(int64 filepos, int seekmode) noexcept
    {
        if (!Handle)
            return 0LL;
    #if _MSC_VER
        LARGE_INTEGER newpos, nseek;
        nseek.QuadPart = filepos;
        SetFilePointerEx(reinterpret_cast<HANDLE>(Handle), nseek, &newpos, seekmode);
        return newpos.QuadPart;
    #else
        fseeki64((FILE*)Handle, filepos, seekmode);
        return (uint64)ftelli64((FILE*)Handle);
    #endif
    }
    int file::tell() const noexcept
    {
        if (!Handle)
            return 0;
    #if _MSC_VER
        return SetFilePointer(reinterpret_cast<HANDLE>(Handle), 0, nullptr, FILE_CURRENT);
    #else
        return (int)ftell((FILE*)Handle);
    #endif
    }

    int64 file::tell64() const noexcept
    {
        if (!Handle)
            return 0LL;
    #if _MSC_VER
        LARGE_INTEGER current;
        SetFilePointerEx(reinterpret_cast<HANDLE>(Handle), { {0, 0} }, &current, FILE_CURRENT);
        return current.QuadPart;
    #else
        return (int64)ftelli64((FILE*)Handle);
    #endif
    }

    bool file::time_info(time_t* outCreated, time_t* outAccessed, time_t* outModified) const noexcept
    {
        intptr_t fd = intptr_t(Handle);
        #if !_MSC_VER
            fd = fileno((FILE*)Handle);
        #endif
        return rpp::file_info(fd, nullptr, outCreated, outAccessed, outModified);
    }
    time_t file::time_created()  const noexcept { time_t t; return time_info(&t, nullptr, nullptr) ? t : time_t(0); }
    time_t file::time_accessed() const noexcept { time_t t; return time_info(nullptr, &t, nullptr) ? t : time_t(0); }
    time_t file::time_modified() const noexcept { time_t t; return time_info(nullptr, nullptr, &t) ? t : time_t(0); }

    int file::size_and_time_modified(time_t* outModified) const noexcept
    {
        if (!Handle) return 0;
        #if _MSC_VER
            *outModified = time_modified();
            return size();
        #else
            struct stat s;
            if (fstat(fileno((FILE*)Handle), &s))
                return 0;
            *outModified = s.st_mtime;
            return (int)s.st_size;
        #endif
    }

    ////////////////////////////////////////////////////////////////////////////////
} // namespace rpp
