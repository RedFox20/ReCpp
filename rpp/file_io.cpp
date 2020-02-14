#include "file_io.h"
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <cerrno>
#include <array>
#include <locale>  // wstring_convert
#include <sys/stat.h> // stat,fstat
#if _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define _CRT_DISABLE_PERFCRIT_LOCKS 1 // we're running single-threaded I/O only
    #include <Windows.h>
    #include <direct.h> // mkdir, getcwd
    #define USE_WINAPI_IO 1

    #define stat64 _stat64
    #define fseeki64 _fseeki64
    #define ftelli64 _ftelli64
    #undef min
#else
    #include <unistd.h>
    #include <dirent.h> // opendir()

    #define _fstat64 fstat
    #define stat64   stat
    #define fseeki64 fseeko
    #define ftelli64 ftello
#endif
#if !_WIN32
    #define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING 1
    #include <codecvt> // codecvt_utf8
#elif !USE_WINAPI_IO
    #include <io.h>     // _chsize
#endif
#if __APPLE__
    #include <TargetConditionals.h>
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

#if !_WIN32
    static std::string to_string(const wchar_t* ws)
    {
        std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
        return cvt.to_bytes(ws);
    }
#endif

#if USE_WINAPI_IO
    static void* OpenF(const char* f, int a, int s, SECURITY_ATTRIBUTES* sa, int c, int o)
    { return CreateFileA(f, a, s, sa, c, o, nullptr); }
    static void* OpenF(const wchar_t* f, int a, int s, SECURITY_ATTRIBUTES* sa, int c, int o)
    { return CreateFileW(f, a, s, sa, c, o, nullptr); }
#else
    static void* OpenF(const char* f, file::mode mode) {
        if (mode == file::READWRITE) {
            if (FILE* file = fopen(f, "rb+")) // open existing file for read/write
                return file;
            mode = file::CREATENEW;
        }
        const char* modes[] = { "rb", "", "wb+", "ab" };
        return fopen(f, modes[static_cast<int>(mode)]);
    }
    static void* OpenF(const wchar_t* f, file::mode mode) {
    #if _WIN32
        if (mode == file::READWRITE) {
            if (FILE* file = _wfopen(f, L"rb+")) // open existing file for read/write
                return file;
            mode = file::CREATENEW;
        }
        const wchar_t* modes[] = { L"rb", L"", L"wb+", L"ab" };
        return _wfopen(f, modes[mode]); 
    #else
        return OpenF(to_string(f).c_str(), mode);
    #endif
    }
#endif

    template<class TChar> static void* OpenFile(const TChar* filename, file::mode mode) noexcept
    {
    #if USE_WINAPI_IO
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
        void* handle = OpenF(filename, access, sharing, &secu, createmode, openFlags);
        return handle != INVALID_HANDLE_VALUE ? handle : nullptr;
    #else
        return OpenF(filename, mode);
    #endif
    }

    template<class TChar> static void* OpenOrCreate(const TChar* filename, file::mode mode) noexcept
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
    static void* OpenOrCreate(const strview& filename, file::mode mode) noexcept
    {
        char buf[512];
        return OpenOrCreate(filename.to_cstr(buf), mode);
    }

    file::file(const char* filename, mode mode) noexcept : Handle{OpenOrCreate(filename, mode)}, Mode{mode}
    {
    }
    file::file(const strview& filename, mode mode) noexcept : Handle{OpenOrCreate(filename, mode)}, Mode{mode}
    {
    }
    file::file(const wchar_t* filename, mode mode) noexcept : Handle{OpenOrCreate(filename, mode)}, Mode{mode}
    {
    }
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
    bool file::open(const char* filename, mode mode) noexcept
    {
        close();
        Mode = mode;
        return (Handle = OpenOrCreate(filename, mode)) != nullptr;
    }
    bool file::open(strview filename, mode mode) noexcept
    {
        char buf[512];
        return open(filename.to_cstr(buf), mode);
    }
    bool file::open(const wchar_t* filename, mode mode) noexcept
    {
        close();
        Mode = mode;
        return (Handle = OpenOrCreate(filename, mode)) != nullptr;
    }
    void file::close() noexcept
    {
        if (Handle)
        {
        #if USE_WINAPI_IO
            CloseHandle(reinterpret_cast<HANDLE>(Handle));
        #else
            fclose((FILE*)Handle);
        #endif
            Handle = nullptr;
        }
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
    #if USE_WINAPI_IO
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
    #if USE_WINAPI_IO
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
    #if USE_WINAPI_IO
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
    bool file::save_as(const strview& filename) noexcept
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
    load_buffer file::read_all(const char* filename) noexcept
    {
        return file{filename, READONLY}.read_all();
    }
    load_buffer file::read_all(const strview& filename) noexcept
    {
        char buf[512];
        return read_all(filename.to_cstr(buf));
    }
    load_buffer file::read_all(const wchar_t* filename) noexcept
    {
        return file{filename, READONLY}.read_all();
    }

    std::unordered_map<std::string, std::string> file::read_map(const strview& filename) noexcept
    {
        load_buffer buf = read_all(filename);
        strview key, value;
        keyval_parser parser = { buf };
        std::unordered_map<std::string, std::string> map;
        while (parser.read_next(key, value))
            map[key] = value.to_string();
        return map;
    }

    std::unordered_map<strview, strview> file::parse_map(const load_buffer& buf) noexcept
    {
        strview key, value;
        keyval_parser parser = { buf };
        std::unordered_map<strview, strview> map;
        while (parser.read_next(key, value))
            map[key] = value;
        return map;
    }
    // ReSharper disable once CppMemberFunctionMayBeConst
    int file::write(const void* buffer, int bytesToWrite) noexcept
    {
        if (bytesToWrite <= 0)
            return 0;
        if (!Handle)
            return 0;
        
    #if USE_WINAPI_IO
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
    int file::writef(const char* format, ...) noexcept
    {
        if (!Handle)
            return 0;
        va_list ap; va_start(ap, format);
    #if USE_WINAPI_IO // @note This is heavily optimized
        char buf[4096];
        int n = vsnprintf(buf, sizeof(buf), format, ap);
        if (n >= static_cast<int>(sizeof(buf)))
        {
            const int n2 = n + 1;
            const bool heap = (n2 > 64 * 1024);
            auto b2 = static_cast<char*>(heap ? malloc(n2) : _alloca(n2)); // NOLINT
            n = write(b2, vsnprintf(b2, n2, format, ap));
            if (heap) free(b2);
            return n;
        }
        return write(buf, n);
    #else
        return vfprintf((FILE*)Handle, format, ap);
    #endif
    }

    int file::writeln(const strview& str) noexcept
    {
        return write(str.str, str.len) + write("\n", 1);
    }

    int file::writeln() noexcept
    {
        return write("\n", 1);
    }

    void file::truncate_front(int64 newLength)
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

    void file::truncate_end(int64 newLength)
    {
        int64 len = sizel();
        if (len <= newLength)
            return;
        truncate(newLength);
    }

    void file::truncate(int64 newLength)
    {
        if (!Handle) return;
    #if USE_WINAPI_IO
        seekl(newLength, SEEK_SET);
        SetEndOfFile(reinterpret_cast<HANDLE>(Handle));
    #elif _MSC_VER
        _chsize_s(fileno((FILE*)Handle), newLength);
    #else
        int result = ftruncate(fileno((FILE*)Handle), (off_t)newLength);
        (void)result;
    #endif
    }

    // ReSharper disable once CppMemberFunctionMayBeConst
    void file::flush() noexcept
    {
        if (!Handle) return;
    #if USE_WINAPI_IO
        FlushFileBuffers(reinterpret_cast<HANDLE>(Handle));
    #else
        fflush((FILE*)Handle);
    #endif
    }
    int file::write_new(const char* filename, const void* buffer, int bytesToWrite) noexcept
    {
        file f{ filename, mode::CREATENEW };
        return f.write(buffer, bytesToWrite);
    }
    int file::write_new(const strview& filename, const void* buffer, int bytesToWrite) noexcept
    {
        char buf[512];
        return write_new(filename.to_cstr(buf), buffer, bytesToWrite);
    }

    // ReSharper disable once CppMemberFunctionMayBeConst
    int file::seek(int filepos, int seekmode) noexcept
    {
        if (!Handle)
            return 0;
    #if USE_WINAPI_IO
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
    #if USE_WINAPI_IO
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
    #if USE_WINAPI_IO
        return SetFilePointer(reinterpret_cast<HANDLE>(Handle), 0, nullptr, FILE_CURRENT);
    #else
        return (int)ftell((FILE*)Handle);
    #endif
    }

    int64 file::tell64() const noexcept
    {
        if (!Handle)
            return 0LL;
    #if USE_WINAPI_IO
        LARGE_INTEGER current;
        SetFilePointerEx(reinterpret_cast<HANDLE>(Handle), { {0, 0} }, &current, FILE_CURRENT);
        return current.QuadPart;
    #else
        return (int64)ftelli64((FILE*)Handle);
    #endif
    }

#if USE_WINAPI_IO
    static time_t to_time_t(const FILETIME& ft)
    {
        ULARGE_INTEGER ull = { { ft.dwLowDateTime, ft.dwHighDateTime } };
        return ull.QuadPart / 10000000ULL - 11644473600ULL;
    }
#endif

    bool file::time_info(time_t* outCreated, time_t* outAccessed, time_t* outModified) const noexcept
    {
        if (!Handle)
            return false;
    #if USE_WINAPI_IO
        FILETIME c, a, m;
        if (GetFileTime((HANDLE)Handle, outCreated?&c:nullptr,outAccessed?&a: nullptr, outModified?&m: nullptr)) {
            if (outCreated)  *outCreated  = to_time_t(c);
            if (outAccessed) *outAccessed = to_time_t(a);
            if (outModified) *outModified = to_time_t(m);
            return true;
        }
        return false;
    #else
        struct stat s;
        if (fstat(fileno((FILE*)Handle), &s)) {
            //fprintf(stderr, "fstat error: [%s]\n", strerror(errno));
            if (outCreated)  *outCreated  = s.st_ctime;
            if (outAccessed) *outAccessed = s.st_atime;
            if (outModified) *outModified = s.st_mtime;
            return true;
        }
        return false;
    #endif
    }
    time_t file::time_created()  const noexcept { time_t t; return time_info(&t, nullptr, nullptr) ? t : time_t(0); }
    time_t file::time_accessed() const noexcept { time_t t; return time_info(nullptr, &t, nullptr) ? t : time_t(0); }
    time_t file::time_modified() const noexcept { time_t t; return time_info(nullptr, nullptr, &t) ? t : time_t(0); }

    int file::size_and_time_modified(time_t* outModified) const noexcept
    {
        if (!Handle) return 0;
        #if USE_WINAPI_IO
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


    bool file_exists(const char* filename) noexcept
    {
        #if USE_WINAPI_IO
            DWORD attr = GetFileAttributesA(filename);
            return attr != DWORD(-1) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
        #else
            struct stat s;
            return stat(filename, &s) ? false : (s.st_mode & S_IFDIR) == 0;
        #endif
    }

    bool file_exists(const wchar_t* filename) noexcept
    {
        #if USE_WINAPI_IO
            DWORD attr = GetFileAttributesW(filename);
            return attr != DWORD(-1) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
        #elif _MSC_VER
            struct _stat64 s;
            return _wstat64(filename, &s) ? false : (s.st_mode & S_IFDIR) == 0;
        #else
            return file_exists(to_string(filename).c_str());
        #endif
    }

    bool folder_exists(const char* folder) noexcept
    {
        #if USE_WINAPI_IO
            DWORD attr = GetFileAttributesA(folder);
            return attr != DWORD(-1) && (attr & FILE_ATTRIBUTE_DIRECTORY);
        #else
            struct stat s;
            return ::stat(folder, &s) ? false : (s.st_mode & S_IFDIR) != 0;
        #endif
    }

    bool file_or_folder_exists(const char* fileOrFolder) noexcept
    {
        #if USE_WINAPI_IO
            DWORD attr = GetFileAttributesA(fileOrFolder);
            return attr != DWORD(-1);
        #else
            struct stat s;
            return stat(fileOrFolder, &s) == 0;
        #endif
    }

    bool file_info(const char* filename, int64*  filesize, time_t* created, 
                                         time_t* accessed, time_t* modified) noexcept
    {
    #if USE_WINAPI_IO
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (GetFileAttributesExA(filename, GetFileExInfoStandard, &data)) {
            if (filesize) *filesize = LARGE_INTEGER{ {data.nFileSizeLow,(LONG)data.nFileSizeHigh} }.QuadPart;
            if (created)  *created  = to_time_t(data.ftCreationTime);
            if (accessed) *accessed = to_time_t(data.ftLastAccessTime);
            if (modified) *modified = to_time_t(data.ftLastWriteTime);
            return true;
        }
    #else
        struct stat64 s;
        if (stat64(filename, &s) == 0/*OK*/) {
            if (filesize) *filesize = (int64)s.st_size;
            if (created)  *created  = s.st_ctime;
            if (accessed) *accessed = s.st_atime;
            if (modified) *modified = s.st_mtime;
            return true;
        }
    #endif
        return false;
    }

    int file_size(const char* filename) noexcept
    {
        int64 s; 
        return file_info(filename, &s, nullptr, nullptr, nullptr) ? static_cast<int>(s) : 0;
    }
    int64 file_sizel(const char* filename) noexcept
    {
        int64 s; 
        return file_info(filename, &s, nullptr, nullptr, nullptr) ? s : 0ll;
    }
    time_t file_created(const char* filename) noexcept
    {
        time_t t; 
        return file_info(filename, nullptr, &t, nullptr, nullptr) ? t : time_t(0);
    }
    time_t file_accessed(const char* filename) noexcept
    {
        time_t t; 
        return file_info(filename, nullptr, nullptr, &t, nullptr) ? t : time_t(0);
    }
    time_t file_modified(const char* filename) noexcept
    {
        time_t t; 
        return file_info(filename, nullptr, nullptr, nullptr, &t) ? t : time_t(0);
    }
    bool delete_file(const char* filename) noexcept
    {
        return ::remove(filename) == 0;
    }

    bool copy_file(const char* sourceFile, const char* destinationFile) noexcept
    {
#if USE_WINAPI_IO
        return CopyFileA(sourceFile, destinationFile, /*failIfExists:*/false) == TRUE;
#else
        file src{sourceFile, file::READONLY};
        if (!src) return false;

        file dst{destinationFile, file::CREATENEW};
        if (!dst) return false;

        int64 size = src.sizel();
        if (size == 0) return true;

        constexpr int64 blockSize = 64*1024;
        char buf[blockSize];
        int64 totalBytesRead    = 0;
        int64 totalBytesWritten = 0;
        for (;;)
        {
            int64 bytesToRead = std::min(size - totalBytesRead, blockSize);
            if (bytesToRead <= 0)
                break;
            int bytesRead = src.read(buf, (int)bytesToRead);
            if (bytesRead <= 0)
                break;
            totalBytesRead    += bytesRead;
            totalBytesWritten += dst.write(buf, bytesRead);
        }
        return totalBytesRead == totalBytesWritten;
#endif
    }

    bool copy_file(const strview sourceFile, const strview destinationFile) noexcept
    {
        char buf1[1024];
        char buf2[1024];
        return copy_file(sourceFile.to_cstr(buf1), destinationFile.to_cstr(buf2));
    }

    bool copy_file_if_needed(const strview sourceFile, const strview destinationFile) noexcept
    {
        if (file_exists(destinationFile))
            return true;
        return copy_file(sourceFile, destinationFile);
    }

    bool copy_file_into_folder(const strview sourceFile, const strview destinationFolder) noexcept
    {
        char buf1[1024];
        std::string destFile = path_combine(destinationFolder, file_nameext(sourceFile));
        return copy_file(sourceFile.to_cstr(buf1), destFile.c_str());
    }
    
    // on failure, check errno for details, if folder (or file) exists, we consider it a success
    // @note Might not be desired behaviour for all cases, so use file_exists or folder_exists.
    static bool sys_mkdir(const strview foldername) noexcept
    {
        char buf[512];
    #if _WIN32
        return _mkdir(foldername.to_cstr(buf)) == 0 || errno == EEXIST;
    #else
        return mkdir(foldername.to_cstr(buf), 0755) == 0 || errno == EEXIST;
    #endif
    }
    static bool sys_mkdir(const wchar_t* foldername) noexcept
    {
    #if _WIN32
        return _wmkdir(foldername) == 0 || errno == EEXIST;
    #else
        std::string s = { foldername, foldername + wcslen(foldername) };
        return sys_mkdir(s);
    #endif
    }

    bool create_folder(strview foldername) noexcept
    {
        if (!foldername.len) // fail on empty strings purely for catching bugs
            return false;
        if (foldername == "./") // current folder already exists
            return true;
        if (sys_mkdir(foldername)) // best case, no recursive mkdir required
            return true;

        // ugh, need to work our way upward to find a root dir that exists:
        // @note heavily optimized to minimize folder_exists() and mkdir() syscalls
        const char* fs = foldername.begin();
        const char* fe = foldername.end();
        const char* p = fe;
        while ((p = strview{fs,p}.rfindany("/\\")) != nullptr)
        {
            if (folder_exists(strview{fs,p}))
                break;
        }

        // now create all the parent dir between:
        p = p ? p + 1 : fs; // handle /dir/ vs dir/ case

        while (const char* e = strview{p,fe}.findany("/\\"))
        {
            if (!sys_mkdir(strview{fs,e}))
                return false; // ugh, something went really wrong here...
            p = e + 1;
        }
        return sys_mkdir(foldername); // and now create the final dir
    }
    bool create_folder(const wchar_t* foldername) noexcept
    {
        if (!foldername || !*foldername || wcscmp(foldername, L"./") == 0)
            return false;
        return sys_mkdir(foldername);
    }
    bool create_folder(const std::wstring& foldername) noexcept
    {
        if (foldername.empty() || foldername == L"./")
            return false;
        return sys_mkdir(foldername.c_str());
    }


    static bool sys_rmdir(const strview foldername) noexcept
    {
        char buf[512];
    #if _WIN32
        return _rmdir(foldername.to_cstr(buf)) == 0;
    #else
        return rmdir(foldername.to_cstr(buf)) == 0;
    #endif
    }

    bool delete_folder(strview foldername, delete_mode mode) noexcept
    {
        // these would delete the root dir...
        if (foldername.empty() || foldername == "/"_sv)
            return false;
        if (mode == delete_mode::non_recursive)
            return sys_rmdir(foldername); // easy path, just gently try to delete...

        std::vector<std::string> folders;
        std::vector<std::string> files;
        bool deletedChildren = true;

        if (list_alldir(folders, files, foldername))
        {
            for (const std::string& folder : folders)
                deletedChildren |= delete_folder(path_combine(foldername, folder), delete_mode::recursive);

            for (const std::string& file : files)
                deletedChildren |= delete_file(path_combine(foldername, file));
        }

        if (deletedChildren)
            return sys_rmdir(foldername); // should be okay to remove now

        return false; // no way to delete, since some subtree files are protected
    }


    std::string full_path(const char* path) noexcept
    {
        char buf[4096];
        #if _WIN32
            size_t len = GetFullPathNameA(path, sizeof(buf), buf, nullptr);
            if (len) normalize(buf);
            return len ? std::string{ buf,len } : std::string{};
        #else
            char* res = realpath(path, buf);
            return res ? std::string{ res } : std::string{};
        #endif
    }


    std::string merge_dirups(strview path) noexcept
    {
        strview pathstr = path;
        const bool isDirPath = path.back() == '/' || path.back() == '\\';
        std::vector<strview> folders;
        while (strview folder = pathstr.next("/\\")) {
            folders.push_back(folder);
        }

        for (int i = 0; i < static_cast<int>(folders.size()); ++i)
        {
            if (i > 0 && folders[i] == ".." && folders[i-1] != "..") 
            {
                auto it = folders.begin() + i;
                folders.erase(it - 1, it + 1);
                i -= 2;
            }
        }

        std::string result;
        for (const strview& folder : folders) {
            result += folder;
            result += '/';
        }
        if (!isDirPath) { // it's a filename? so pop the last /
            result.pop_back();
        }
        return result;
    }


    strview file_name(strview path) noexcept
    {
        strview nameext = file_nameext(path);

        if (auto* ptr = nameext.substr(nameext.len - 8).rfind('.'))
            return strview{ nameext.str, ptr };
        return nameext;
    }


    strview file_nameext(const strview path) noexcept
    {
        if (auto* str = path.rfindany("/\\"))
            return strview{ str + 1, path.end() };
        return path; // assume it's just a file name
    }


    strview file_ext(strview path) noexcept
    {
        if (auto* ptr = path.substr(path.len - 8).rfindany("./\\")) {
            if (*ptr == '.') return { ptr + 1, path.end() };
        }
        return strview{};
    }


    std::string file_replace_ext(strview path, strview ext)
    {
        if (strview oldext = file_ext(path))
        {
            auto len = int(oldext.str - path.str);
            return strview{ path.str, len } + ext;
        }
        if (path && path.back() != '/' && path.back() != '\\')
        {
            return path + "." + ext;
        }
        return path;
    }
    
    std::string file_name_append(strview path, strview add)
    {
        std::string result = folder_path(path);
        result += file_name(path);
        result += add;
        if (strview ext = file_ext(path)) {
            result += '.';
            result += ext;
        }
        return result;
    }
    
    std::string file_name_replace(strview path, strview newFileName)
    {
        std::string result = folder_path(path) + newFileName;
        if (strview ext = file_ext(path)) {
            result += '.';
            result += ext;
        }
        return result;
    }
    
    std::string file_nameext_replace(strview path, strview newFileNameAndExt)
    {
        return folder_path(path) + newFileNameAndExt;
    }

    strview folder_name(strview path) noexcept
    {
        strview folder = folder_path(path);
        if (folder)
        {
            if (const char* str = folder.chomp_last().rfindany("/\\"))
                return strview{ str + 1, folder.end() };
        }
        return folder;
    }


    strview folder_path(strview path) noexcept
    {
        if (const char* end = path.rfindany("/\\"))
            return strview{ path.str, end + 1 };
        return strview{};
    }
    std::wstring folder_path(const wchar_t* path) noexcept
    {
        auto* end = path + wcslen(path);
        for (; path < end; --end)
            if (*end == '/' || *end == '\\')
                break;
        return path == end ? std::wstring{} : std::wstring{path, end + 1};
    }
    std::wstring folder_path(const std::wstring& path) noexcept
    {
        auto* ptr = path.c_str();
        auto* end  = ptr + path.size();
        for (; ptr < end; --end)
            if (*end == '/' || *end == '\\')
                break;
        return ptr == end ? std::wstring{} : std::wstring{ptr, end + 1};
    }


    std::string& normalize(std::string& path, char sep) noexcept
    {
        if (sep == '/') {
            for (char& ch : path) if (ch == '\\') ch = '/';
        }
        else if (sep == '\\') {
            for (char& ch : path) if (ch == '/')  ch = '\\';
        }
        // else: ignore any other separators
        return path;
    }
    char* normalize(char* path, char sep) noexcept
    {
        if (sep == '/') {
            for (char* s = path; *s; ++s) if (*s == '\\') *s = '/';
        }
        else if (sep == '\\') {
            for (char* s = path; *s; ++s) if (*s == '/')  *s = '\\';
        }
        // else: ignore any other separators
        return path;
    }

    std::string normalized(strview path, char sep) noexcept
    {
        std::string res = path.to_string();
        normalize(res, sep);
        return res;
    }

    template<size_t N> std::string slash_combine(const std::array<strview, N>& args)
    {
        size_t res = args[0].size();
        for (size_t i = 1; i < N; ++i) {
            if (auto n = static_cast<size_t>(args[i].size())) {
                if (res != 0)
                    res += 1;
                res += n;
            }
        }
        
        std::string result; result.reserve(res);
        result.append(args[0].c_str(), args[0].size());

        for (size_t i = 1; i < N; ++i) {
            if (auto n = static_cast<size_t>(args[i].size())) {
                if (!result.empty())
                    result.append(1, '/');
                result.append(args[i].c_str(), n);
            }
        }
        return result;
    }

    std::string path_combine(strview path1, strview path2) noexcept
    {
        path1.trim_end("/\\");
        path2.trim("/\\");
        return slash_combine(std::array<strview, 2>{{path1, path2}});
    }

    std::string path_combine(strview path1, strview path2, strview path3) noexcept
    {
        path1.trim_end("/\\");
        path2.trim("/\\");
        path3.trim("/\\");
        return slash_combine(std::array<strview, 3>{{path1, path2, path3}});
    }

    std::string path_combine(strview path1, strview path2, strview path3, strview path4) noexcept
    {
        path1.trim_end("/\\");
        path2.trim("/\\");
        path3.trim("/\\");
        path4.trim("/\\");
        return slash_combine(std::array<strview, 4>{{path1, path2, path3, path4}});
    }

    ////////////////////////////////////////////////////////////////////////////////



    struct dir_iterator::impl {
        #if _WIN32
            HANDLE hFind;
            WIN32_FIND_DATAA ffd;
        #else
            DIR* d;
            dirent* e;
        #endif
    };
    dir_iterator::impl* dir_iterator::dummy::operator->()
    {
#if _WIN32
        static_assert(sizeof(ffd) == sizeof(impl::ffd), "dir_iterator::dummy size mismatch");
#endif
        return reinterpret_cast<impl*>(this);
    }
    const dir_iterator::impl* dir_iterator::dummy::operator->() const
    {
        return reinterpret_cast<const impl*>(this);
    }

#if _WIN32
    // ReSharper disable CppSomeObjectMembersMightNotBeInitialized
    dir_iterator::dir_iterator(std::string&& dir) : dir{std::move(dir)} {  // NOLINT
        char path[512];
        if (this->dir.empty()) { // handle dir=="" special case
            snprintf(path, 512, "./*");
        }
        else {
            snprintf(path, 512, "%.*s/*", static_cast<int>(this->dir.length()), this->dir.c_str());
        }
        if ((s->hFind = FindFirstFileA(path, &s->ffd)) == INVALID_HANDLE_VALUE)
            s->hFind = nullptr;
    // ReSharper restore CppSomeObjectMembersMightNotBeInitialized
    }
    dir_iterator::~dir_iterator() { if (s->hFind) FindClose(s->hFind); }
    dir_iterator::operator bool()  const { return s->hFind != nullptr; }
        bool    dir_iterator::next()         { return s->hFind && FindNextFileA(s->hFind, &s->ffd) != 0; }
        bool    dir_iterator::is_dir() const { return s->hFind && (s->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
        strview dir_iterator::name()   const { return s->hFind ? s->ffd.cFileName : ""_sv; }
#else
        dir_iterator::dir_iterator(string&& dir) : dir{move(dir)} {
            const char* path = this->dir.empty() ? "." : this->dir.c_str();
            s->e = (s->d=opendir(path)) != nullptr ? readdir(s->d) : nullptr;
        }
        dir_iterator::~dir_iterator() { if (s->d) closedir(s->d); }
        dir_iterator::operator bool()  const { return s->d && s->e; }
        bool    dir_iterator::next()         { return s->d && (s->e = readdir(s->d)) != nullptr; }
        bool    dir_iterator::is_dir() const { return s->e && s->e->d_type == DT_DIR; }
        strview dir_iterator::name()   const { return s->e ? strview{s->e->d_name} : strview{}; }
#endif



    ////////////////////////////////////////////////////////////////////////////////



    // @param queryRoot The original path passed to the query.
    //                  For abs listing, this must be an absolute path value!
    //                  For rel listing, this is only used for opening the directory
    // @param relPath Relative path from search root, ex: "src", "src/session/util", etc.
    template<class Func> 
    static void traverse_dir2(strview queryRoot, strview relPath, 
                              bool dirs, bool files, bool rec, bool abs, const Func& func)
    {
        std::string currentDir = path_combine(queryRoot, relPath);
        for (dir_entry e : dir_iterator{ currentDir })
        {
            bool validDir = e.is_dir && e.name != "." && e.name != "..";
            if ((validDir && dirs) || (!e.is_dir && files)) 
            {
                strview dir = abs ? strview{currentDir} : relPath;
                func(path_combine(dir, e.name), e.is_dir);
            }
            if (validDir && rec)
            {
                traverse_dir2(queryRoot, path_combine(relPath, e.name), dirs, files, rec, abs, func);
            }
        }
    }
    template<class Func> 
    static void traverse_dir(strview dir, bool dirs, bool files, bool rec, bool abs, const Func& func)
    {
        if (abs)
        {
            std::string fullpath = full_path(dir.empty() ? "." : dir);
            if (fullpath.empty())
                return; // folder does not exist
            
            traverse_dir2(fullpath, strview{}, dirs, files, rec, abs, func);
            return;
        }
        traverse_dir2(dir, strview{}, dirs, files, rec, abs, func);
    }

    ////////////////////////////////////////////////////////////////////////////////

    int list_dirs(std::vector<std::string>& out, strview dir, bool recursive, bool fullpath) noexcept
    {
        traverse_dir(dir, true, false, recursive, fullpath, [&](std::string&& path, bool) {
            out.emplace_back(std::move(path));
        });
        return static_cast<int>(out.size());
    }

    int list_dirs_relpath(std::vector<std::string>& out, strview dir, bool recursive) noexcept
    {
        traverse_dir(dir, true, false, recursive, false, [&](std::string&& path, bool) {
            out.emplace_back(path_combine(dir, path));
        });
        return static_cast<int>(out.size());
    }

    int list_files(std::vector<std::string>& out, strview dir, strview suffix, bool recursive, bool fullpath) noexcept
    {
        traverse_dir(dir, false, true, recursive, fullpath, [&](std::string&& path, bool) {
            if (suffix.empty() || strview{path}.ends_withi(suffix))
                out.emplace_back(std::move(path));
        });
        return static_cast<int>(out.size());
    }

    int list_files_relpath(std::vector<std::string>& out, strview dir, strview suffix, bool recursive) noexcept
    {
        traverse_dir(dir, false, true, recursive, false, [&](std::string&& path, bool) {
            if (suffix.empty() || strview{path}.ends_withi(suffix))
                out.emplace_back(path_combine(dir, path));
        });
        return static_cast<int>(out.size());
    }

    int list_alldir(std::vector<std::string>& outDirs, std::vector<std::string>& outFiles, strview dir, bool recursive, bool fullpath) noexcept
    {
        traverse_dir(dir, true, true, recursive, fullpath, [&](std::string&& path, bool isDir) {
            auto& out = isDir ? outDirs : outFiles;
            out.emplace_back(std::move(path));
        });
        return static_cast<int>(outDirs.size() + outFiles.size());
    }

    int list_alldir_relpath(std::vector<std::string>& outDirs, std::vector<std::string>& outFiles, strview dir, bool recursive) noexcept
    {
        traverse_dir(dir, true, true, recursive, false, [&](std::string&& path, bool isDir) {
            auto& out = isDir ? outDirs : outFiles;
            out.emplace_back(path_combine(dir, path));
        });
        return static_cast<int>(outDirs.size() + outFiles.size());
    }

    std::vector<std::string> list_files(strview dir, const std::vector<strview>& suffixes, bool recursive, bool fullpath) noexcept
    {
        std::vector<std::string> out;
        traverse_dir(dir, false, true, recursive, fullpath, [&](std::string&& path, bool) {
            for (const strview& suffix : suffixes) {
                if (strview{ path }.ends_withi(suffix)) {
                    out.emplace_back(std::move(path));
                    return;
                }
            }
        });
        return out;
    }

    ////////////////////////////////////////////////////////////////////////////////

    static void append_slash(char* path, int& len)
    {
        if (path[len-1] != '/')
            path[len++] = '/';
    }

    #if _WIN32
    static void win32_fixup_path(char* path, int& len)
    {
        normalize(path);
        append_slash(path, len);
    }
    #endif

    std::string working_dir() noexcept
    {
        char path[512];
        #if _WIN32
            char* p = _getcwd(path, sizeof(path));
        #else
            char* p = getcwd(path, sizeof(path));
        #endif

        if (p) {
            int len = static_cast<int>(strlen(p));
            #if _WIN32
                win32_fixup_path(p, len);
            #else
                append_slash(p, len);
            #endif
            return {p, p + len};
        }
        return {};
    }

    std::string module_dir(void* moduleObject) noexcept
    {
        #if _WIN32
            char path[512];
            int len = (int)GetModuleFileNameA(reinterpret_cast<HMODULE>(moduleObject), path, 512);
            normalize(path);
            return folder_path(strview{path, len});
        #elif __APPLE__
            extern std::string apple_module_dir(void* moduleObject) noexcept;
            return apple_module_dir(moduleObject);
        #else
            // @todo Implement missing platforms: __linux__, __ANDROID__, RASPI
            return working_dir();
        #endif
    }

    std::string module_path(void* moduleObject) noexcept
    {
        #if _WIN32
            char path[512];
            int len = (int)GetModuleFileNameA(reinterpret_cast<HMODULE>(moduleObject), path, 512);
            normalize(path);
            return { path, path+len };
        #elif __APPLE__
            extern std::string apple_module_path(void* moduleObject) noexcept;
            return apple_module_path(moduleObject);
        #else
            // @todo Implement missing platforms: __linux__, __ANDROID__, RASPI
            return working_dir();
        #endif
    }

    bool change_dir(const char* new_wd) noexcept
    {
        #if _WIN32
            return _chdir(new_wd) == 0;
        #else
            return chdir(new_wd) == 0;
        #endif
    }

    std::string temp_dir() noexcept
    {
        #if _WIN32
            char path[512];
            int len = (int)GetTempPathA(sizeof(path), path);
            win32_fixup_path(path, len);
            return { path, path + len };
        #elif __APPLE__
            extern std::string apple_temp_dir() noexcept;
            return apple_temp_dir();
        #elif __ANDROID__
            // return getContext().getExternalFilesDir(null).getPath();
            return "/data/local/tmp";
        #else
            if (auto path = getenv("TMP"))     return path;
            if (auto path = getenv("TEMP"))    return path;
            if (auto path = getenv("TMPDIR"))  return path;
            if (auto path = getenv("TEMPDIR")) return path;
            return "/tmp/";
        #endif
    }
    
    std::string home_dir() noexcept
    {
        #if _MSC_VER
            size_t len = 0;
            char path[512];
            getenv_s(&len, path, sizeof(path), "USERPROFILE");
            if (len == 0)
                return {};
            int slen = static_cast<int>(len) - 1;
            win32_fixup_path(path, slen);
            return { path, path + len };
        #else
            char* p = getenv("HOME");
            if (p) {
                int len = (int)strlen(p);
                append_slash(p, len);
                return { p, p + len };
            }
            return {};
        #endif
    }
    
    ////////////////////////////////////////////////////////////////////////////////


} // namespace rpp
