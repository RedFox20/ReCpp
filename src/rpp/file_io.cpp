#include "file_io.h"
#include "strview.h"
#include "debugging.h"
#include <cstdlib> // malloc
#include <cstdio> // fopen
#include <cstdarg> // va_list
#include <memory> // std::unique_ptr
#include <new> // std::nothrow

#include "paths.inl"

#if __APPLE__
    #include <TargetConditionals.h> // TARGET_OS_IPHONE
#endif
#if RPP_ANDROID
    #include <jni.h>
#endif
#if !_MSC_VER
    #include <sys/file.h> // flock
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
    #elif RPP_ANDROID || TARGET_OS_IPHONE
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
    #elif RPP_ANDROID || __APPLE__
        struct stat64 s;
        if (_fstat64(fileno((FILE*)Handle), &s)) {
            //fprintf(stderr, "_fstat64 error: [%s]\n", strerror(errno));
            return 0ull;
        }
        return (int64)s.st_size;
    #else // Linux version is more complicated. It won't report correct size unless file is flushed:
        int64 pos = tell64();
        int64 size = (int64)const_cast<file*>(this)->seekl(0LL, SEEK_END);
        const_cast<file*>(this)->seekl(pos, SEEK_SET);
        return size;
    #endif
    }
    // ReSharper disable once CppMemberFunctionMayBeConst
    int file::read(void* buffer, int bytesToRead) noexcept // NOLINT(readability-make-member-function-const)
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
            if (auto* buffer = static_cast<char*>(malloc(size_t(fileSize) + 1u)))
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
        try {
            out.resize(size_t(count));
            int n = read(const_cast<char*>(out.data()), count);
            if (n != count) {
                out.resize(size_t(n));
                out.shrink_to_fit();
            }
        } catch (...) {
            LogError("file::read_text %d bytes failed", count);
            return {};
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

        constexpr int64 blockSize = 64LL * 1024LL;
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
    int file::write(const void* buffer, int bytesToWrite) noexcept // NOLINT(readability-make-member-function-const)
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
    int file::writef(PRINTF_FMTSTR const char* format, ...) noexcept // NOLINT(readability-make-member-function-const)
    {
        if (!Handle)
            return 0;
        va_list ap;
        va_start(ap, format);
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
        int written = vfprintf((FILE*)Handle, format, ap); // NOLINT(clang-analyzer-valist.Uninitialized)
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

        constexpr int64 MAX_SINGLE_READ_BUFSIZE = (32LL * 1024LL * 1024LL); // max MB buffer size
        bool useSmallBufTruncate = newLength > MAX_SINGLE_READ_BUFSIZE;
        try
        {
            if (useSmallBufTruncate)
            {
                // small buf version reads in relatively tiny but safe chunks
                // it will be slow, but will not run out of memory
                truncate_front_sb(newLength);
            }
            else
            {
                // attempt to read entire file from new offset to memory
                // this is generally the fastest approach on modern NVMe SSD systems
                std::vector<char> buf;
                buf.resize(static_cast<size_t>(newLength)); // throws
                int64 bytesToTrunc = len - newLength;
                seekl(bytesToTrunc, SEEK_SET);
                int bytesRead = read(buf.data(), static_cast<int>(newLength));
                truncate(newLength);
                seek(0, SEEK_SET);
                write(buf.data(), bytesRead);
            }
        }
        catch (...) // resize failed to allocate enough memory
        {
            truncate_front_sb(newLength);
        }
    }

    void file::truncate_front_sb(int64 newLength) noexcept
    {
        int64 len = sizel();
        if (len <= newLength)
            return;

    #if YOCTO_LINUX
        // a much smaller buffer on embedded systems
        constexpr int64 SMALL_BLOCK_SIZE = 64LL * 1024LL;
    #else
        constexpr int64 SMALL_BLOCK_SIZE = 512LL * 1024LL;
    #endif
        // on the heap: an Emscripten thread stack is only 64 KB
        // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks): false positive with the NDK r29 libc++ unique_ptr<T[]>
        std::unique_ptr<uint8_t[]> buf { new (std::nothrow) uint8_t[SMALL_BLOCK_SIZE] };
        if (!buf)
        {
            LogError("file::truncate_front_sb failed: no memory for the copy buffer");
            return;
        }
        int64 readPos = len - newLength;
        int64 writePos = 0;
        while (readPos < len)
        {
            seekl(readPos, SEEK_SET);
            int bytesRead = read(buf.get(), static_cast<int>(SMALL_BLOCK_SIZE));
            if (bytesRead <= 0) break; // EOF or error

            seekl(writePos, SEEK_SET);
            if (write(buf.get(), bytesRead) != bytesRead)
                break; // disk error?

            readPos += bytesRead;
            writePos += bytesRead;
        }

        // Only truncate if we successfully copied the full newLength bytes
        if (writePos == newLength)
            truncate(newLength);
        else
            LogError("file::truncate_front_sb failed: readPos=%lld, writePos=%lld", readPos, writePos);
    }

    void file::truncate_end(int64 newLength) noexcept
    {
        int64 len = sizel();
        if (len <= newLength)
            return;
        truncate(newLength);
    }

    void file::truncate(int64 newLength) noexcept // NOLINT(readability-make-member-function-const)
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
    #if __EMSCRIPTEN__ // posix_fallocate only, the same as fallocate mode 0
        if (posix_fallocate(fd, 0, preallocSize) != 0)
    #else
        if (fallocate(fd, 0, 0, preallocSize) != 0)
    #endif
            return false; // preallocation failed
        seekl(seekPos, seekMode);
        return true; // preallocation succeeded
    #endif
    }

    // ReSharper disable once CppMemberFunctionMayBeConst
    void file::flush() noexcept // NOLINT(readability-make-member-function-const)
    {
        if (!Handle) return;
    #if _MSC_VER
        FlushFileBuffers(reinterpret_cast<HANDLE>(Handle));
    #else
        fflush((FILE*)Handle);
    #endif
    }

    // ReSharper disable once CppMemberFunctionMayBeConst
    bool file::sync() noexcept // NOLINT(readability-make-member-function-const)
    {
        if (!Handle) return false;
    #if _MSC_VER
        return FlushFileBuffers(reinterpret_cast<HANDLE>(Handle)) != 0;
    #elif __APPLE__ // fsync leaves the data in the drive cache there
        return fflush((FILE*)Handle) == 0 && fcntl(fileno((FILE*)Handle), F_FULLFSYNC) != -1;
    #elif __EMSCRIPTEN__ // MEMFS holds the file in memory, and only the async FS.syncfs() persists IDBFS
        fflush((FILE*)Handle);
        return false;
    #else
        return fflush((FILE*)Handle) == 0 && fsync(fileno((FILE*)Handle)) == 0;
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
    int file::seek(int filepos, int seekmode) noexcept // NOLINT(readability-make-member-function-const)
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
    uint64 file::seekl(int64 filepos, int seekmode) noexcept // NOLINT(readability-make-member-function-const)
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
        intptr_t fd;
        #if !_MSC_VER
            fd = fileno((FILE*)Handle);
        #else
            fd = intptr_t(Handle);
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

    file_lock& file_lock::operator=(file_lock&& other) noexcept
    {
        if (this != &other)
        {
            unlock();
            handle = other.handle;
            other.handle = -1;
        }
        return *this;
    }

    template<StringViewType T>
    static file_lock TryLockFile(T filename) noexcept
    {
        file_lock lock;
    #if _MSC_VER
        // the lock handle is not inheritable, so a child process never keeps the lock alive
        wchar_conv conv { filename };
        if (!conv) return lock;
        HANDLE h = CreateFileW(conv.wstr, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
                               nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) return lock;
        OVERLAPPED whole_file = {};
        if (LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY, 0, MAXDWORD, MAXDWORD, &whole_file))
            lock.handle = reinterpret_cast<intptr_t>(h);
        else
            CloseHandle(h);
    #else
        // O_CLOEXEC keeps the lock out of a child process, which would hold it after this process ends
        multibyte_conv conv { filename };
        if (!conv) return lock;
        int fd = ::open(conv.cstr, O_RDWR|O_CREAT|O_CLOEXEC, 0644);
        if (fd == -1) return lock;
        if (flock(fd, LOCK_EX|LOCK_NB) == 0)
            lock.handle = fd;
        else
            ::close(fd);
    #endif
        return lock;
    }

    file_lock file_lock::try_lock(strview filename) noexcept { return TryLockFile(filename); }
#if RPP_ENABLE_UNICODE
    file_lock file_lock::try_lock(ustrview filename) noexcept { return TryLockFile(filename); }
#endif // RPP_ENABLE_UNICODE

    void file_lock::unlock() noexcept
    {
        if (handle == -1) return;
    #if _MSC_VER
        HANDLE h = reinterpret_cast<HANDLE>(handle);
        OVERLAPPED whole_file = {};
        UnlockFileEx(h, 0, MAXDWORD, MAXDWORD, &whole_file);
        CloseHandle(h);
    #else
        ::close(int(handle)); // closing the only descriptor releases the flock()
    #endif
        handle = -1;
    }

    ////////////////////////////////////////////////////////////////////////////////
} // namespace rpp
