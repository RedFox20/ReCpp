#include "file_io.h"
#include <cstdlib>
#include <cstdio>
#include <cerrno>
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
#else
    #include <unistd.h>
    #include <dirent.h> // opendir()

    #define fseeki64 fseeko
    #define ftelli64 ftello

    #define _fstat64 fstat
    #define stat64   stat
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
        mv.str = 0;
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
        str = 0;
        return p;
    }


    ////////////////////////////////////////////////////////////////////////////////

#if USE_WINAPI_IO
    static void* OpenF(const char* f, int a, int s, SECURITY_ATTRIBUTES* sa, int c, int o)
    { return CreateFileA(f, a, s, sa, c, o, 0); }
    static void* OpenF(const wchar_t* f, int a, int s, SECURITY_ATTRIBUTES* sa, int c, int o)
    { return CreateFileW(f, a, s, sa, c, o, 0); }
#else
    static void* OpenF(const char* f, IOFlags mode) {
        const char* modes[] = { "rb", "wbx", "wb", "ab" };
        return fopen(f, modes[mode]);
    }
    static void* OpenF(const wchar_t* f, IOFlags mode) {
    #if _WIN32
        const wchar_t* modes[] = { L"rb", L"wbx", L"wb", L"ab" };
        return _wfopen(f, modes[mode]); 
    #else
        string s = { f, f + wcslen(f) }; // @todo Add proper UCS2 --> UTF8 conversion
        return OpenF(s.c_str(), mode);
    #endif
    }
#endif

    template<class TChar> static void* OpenFile(const TChar* filename, IOFlags mode) noexcept
    {
    #if USE_WINAPI_IO
        int access, sharing;        // FILE_SHARE_READ, FILE_SHARE_WRITE
        int createmode, openFlags;	// OPEN_EXISTING, OPEN_ALWAYS, CREATE_ALWAYS
        switch (mode)
        {
            default:
            case READONLY:
                access     = FILE_GENERIC_READ;
                sharing    = FILE_SHARE_READ | FILE_SHARE_WRITE;
                createmode = OPEN_EXISTING;
                openFlags  = FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN;
                break;
            case READWRITE:
                access     = FILE_GENERIC_READ|FILE_GENERIC_WRITE;
                sharing	   = FILE_SHARE_READ;
                createmode = OPEN_EXISTING; // if not exists, fail
                openFlags  = FILE_ATTRIBUTE_NORMAL;
                break;
            case CREATENEW:
                access     = FILE_GENERIC_READ|FILE_GENERIC_WRITE|DELETE;
                sharing    = FILE_SHARE_READ;
                createmode = CREATE_ALWAYS;
                openFlags  = FILE_ATTRIBUTE_NORMAL;
                break;
            case APPEND:
                access     = FILE_APPEND_DATA;
                sharing    = FILE_SHARE_READ;
                createmode = OPEN_ALWAYS;
                openFlags  = FILE_ATTRIBUTE_NORMAL;
                break;
        }
        SECURITY_ATTRIBUTES secu = { sizeof(secu), NULL, TRUE };
        void* handle = OpenF(filename, access, sharing, &secu, createmode, openFlags);
        return handle != INVALID_HANDLE_VALUE ? handle : 0;
    #else
        return OpenF(filename, mode);
    #endif
    }

    template<class TChar> static void* OpenOrCreate(const TChar* filename, IOFlags mode) noexcept
    {
        void* handle = OpenFile(filename, mode);
        if (!handle && mode == CREATENEW)
        {
            // assume the directory doesn't exist
            if (create_folder(folder_path(filename))) {
                return OpenFile(filename, mode); // last chance
            }
        }
        return handle;
    }
    static void* OpenOrCreate(const strview& filename, IOFlags mode) noexcept
    {
        char buf[512];
        return OpenOrCreate(filename.to_cstr(buf), mode);
    }

    file::file(const char* filename, IOFlags mode) noexcept : Handle(OpenOrCreate(filename, mode)), Mode(mode)
    {
    }
    file::file(const strview& filename, IOFlags mode) noexcept : Handle(OpenOrCreate(filename, mode)), Mode(mode)
    {
    }
    file::file(const wchar_t* filename, IOFlags mode) noexcept : Handle(OpenOrCreate(filename, mode)), Mode(mode)
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
    bool file::open(const char* filename, IOFlags mode) noexcept
    {
        close();
        Mode = mode;
        return (Handle = OpenOrCreate(filename, mode)) != nullptr;
    }
    bool file::open(strview filename, IOFlags mode) noexcept
    {
        char buf[512];
        return open(filename.to_cstr(buf), mode);
    }
    bool file::open(const wchar_t* filename, IOFlags mode) noexcept
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
                CloseHandle((HANDLE)Handle);
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
            return GetFileSize((HANDLE)Handle, 0);
        #else
            struct stat s;
            if (fstat(fileno((FILE*)Handle), &s)) {
                //fprintf(stderr, "fstat error: [%s]\n", strerror(errno));
                return 0;
            }
            return (int)s.st_size;
        #endif
    }
    int64 file::sizel() const noexcept
    {
        if (!Handle) return 0;
        #if USE_WINAPI_IO
            LARGE_INTEGER size;
            if (!GetFileSizeEx((HANDLE)Handle, &size)) {
                //fprintf(stderr, "GetFileSizeEx error: [%d]\n", GetLastError());
                return 0ull;
            }
            return (int64)size.QuadPart;
        #else
            struct stat64 s;
            if (_fstat64(fileno((FILE*)Handle), &s)) {
                //fprintf(stderr, "_fstat64 error: [%s]\n", strerror(errno));
                return 0ull;
            }
            return (int64)s.st_size;
        #endif
    }
    int file::read(void* buffer, int bytesToRead) noexcept
    {
        #if USE_WINAPI_IO
            DWORD bytesRead;
            ReadFile((HANDLE)Handle, buffer, bytesToRead, &bytesRead, nullptr);
            return bytesRead;
        #else
            return (int)fread(buffer, (size_t)bytesToRead, 1, (FILE*)Handle) * bytesToRead;
        #endif
    }
    load_buffer file::read_all() noexcept
    {
        int fileSize = size();
        if (!fileSize) return load_buffer{nullptr, 0};

        // allocate +1 bytes for null terminator; this is for legacy API-s
        char* buffer = (char*)malloc(size_t(fileSize + 1));
        int bytesRead = read(buffer, fileSize);
        buffer[bytesRead] = '\0';
        return load_buffer{buffer, bytesRead};
    }
    string file::read_text() noexcept
    {
        string out;
        int fileSize = size();
        if (fileSize <= 0) return out;

        // allocate +1 bytes for null terminator; this is for legacy API-s
        out.resize(size_t(fileSize));
        int n = read((void*)out.data(), fileSize);
        if (n != fileSize)
            out.resize(size_t(n));
        return out;
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

    unordered_map<string, string> file::read_map(const strview& filename) noexcept
    {
        load_buffer buf = read_all(filename);
        strview key, value;
        keyval_parser parser = { buf };
        unordered_map<string, string> map;
        while (parser.read_next(key, value))
            map[key] = value.to_string();
        return map;
    }

    unordered_map<strview, strview> file::parse_map(const load_buffer& buf) noexcept
    {
        strview key, value;
        keyval_parser parser = { buf };
        unordered_map<strview, strview> map;
        while (parser.read_next(key, value))
            map[key] = value;
        return map;
    }

    int file::write(const void* buffer, int bytesToWrite) noexcept
    {
        if (bytesToWrite <= 0)
            return 0;
        if (Handle == nullptr)
            return 0;
        
        #if USE_WINAPI_IO
            DWORD bytesWritten;
            WriteFile((HANDLE)Handle, buffer, bytesToWrite, &bytesWritten, 0);
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
        if (Handle == nullptr)
            return 0;
        va_list ap; va_start(ap, format);
        #if USE_WINAPI_IO // @note This is heavily optimized
            char buf[4096];
            int n = vsnprintf(buf, sizeof(buf), format, ap);
            if (n >= sizeof(buf))
            {
                const int n2 = n + 1;
                const bool heap = (n2 > 64 * 1024);
                char* b2 = (char*)(heap ? malloc(n2) : _alloca(n2));
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

    void file::flush() noexcept
    {
        #if USE_WINAPI_IO
            FlushFileBuffers((HANDLE)Handle);
        #else
            fflush((FILE*)Handle);
        #endif
    }
    int file::write_new(const char* filename, const void* buffer, int bytesToWrite) noexcept
    {
        file f{ filename, IOFlags::CREATENEW };
        return f.write(buffer, bytesToWrite);
    }
    int file::write_new(const strview& filename, const void* buffer, int bytesToWrite) noexcept
    {
        char buf[512];
        return write_new(filename.to_cstr(buf), buffer, bytesToWrite);
    }

    int file::seek(int filepos, int seekmode) noexcept
    {
        #if USE_WINAPI_IO
            return SetFilePointer((HANDLE)Handle, filepos, 0, seekmode);
        #else
            fseek((FILE*)Handle, filepos, seekmode);
            return (int)ftell((FILE*)Handle);
        #endif
    }
    uint64 file::seekl(int64 filepos, int seekmode) noexcept
    {
        #if USE_WINAPI_IO
            LARGE_INTEGER newpos, nseek;
            nseek.QuadPart = filepos;
            SetFilePointerEx((HANDLE)Handle, nseek, &newpos, seekmode);
            return newpos.QuadPart;
        #else
            fseeki64((FILE*)Handle, filepos, seekmode);
            return (uint64)ftelli64((FILE*)Handle);
        #endif
    }
    int file::tell() const noexcept
    {
        #if USE_WINAPI_IO
            return SetFilePointer((HANDLE)Handle, 0, 0, FILE_CURRENT);
        #else
            return (int)ftell((FILE*)Handle);
        #endif
    }

    int64 file::tell64() const noexcept
    {
        #if USE_WINAPI_IO
            LARGE_INTEGER current;
            SetFilePointerEx((HANDLE)Handle, { 0 }, &current, FILE_CURRENT);
            return current.QuadPart;
        #else
            return (int64)ftelli64((FILE*)Handle);
        #endif
    }

#if USE_WINAPI_IO
    static time_t to_time_t(const FILETIME& ft)
    {
        ULARGE_INTEGER ull = { ft.dwLowDateTime, ft.dwHighDateTime };
        return ull.QuadPart / 10000000ULL - 11644473600ULL;
    }
#endif

    bool file::time_info(time_t* outCreated, time_t* outAccessed, time_t* outModified) const noexcept
    {
    #if USE_WINAPI_IO
        FILETIME c, a, m;
        if (GetFileTime((HANDLE)Handle, outCreated?&c:0,outAccessed?&a:0, outModified?&m:0)) {
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
    time_t file::time_created()  const noexcept { time_t t; return time_info(&t, 0, 0) ? t : time_t(0); }
    time_t file::time_accessed() const noexcept { time_t t; return time_info(0, &t, 0) ? t : time_t(0); }
    time_t file::time_modified() const noexcept { time_t t; return time_info(0, 0, &t) ? t : time_t(0); }

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
            return attr != -1 && !(attr & FILE_ATTRIBUTE_DIRECTORY);
        #else
            struct stat s;
            return stat(filename, &s) ? false : (s.st_mode & S_IFDIR) == 0;
        #endif
    }

    bool folder_exists(const char* folder) noexcept
    {
        #if USE_WINAPI_IO
            DWORD attr = GetFileAttributesA(folder);
            return attr != -1 && (attr & FILE_ATTRIBUTE_DIRECTORY);
        #else
            struct stat s;
            return ::stat(folder, &s) ? false : (s.st_mode & S_IFDIR) != 0;
        #endif
    }

    bool file_or_folder_exists(const char* fileOrFolder) noexcept
    {
        #if USE_WINAPI_IO
            DWORD attr = GetFileAttributesA(fileOrFolder);
            return attr != -1;
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
            if (filesize) *filesize = LARGE_INTEGER{data.nFileSizeLow,(LONG)data.nFileSizeHigh}.QuadPart;
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
        return file_info(filename, &s, 0, 0, 0) ? (int)s : 0;
    }
    int64 file_sizel(const char* filename) noexcept
    {
        int64 s; 
        return file_info(filename, &s, 0, 0, 0) ? s : 0ll;
    }
    time_t file_created(const char* filename) noexcept
    {
        time_t t; 
        return file_info(filename, 0, &t, 0, 0) ? t : time_t(0);
    }
    time_t file_accessed(const char* filename) noexcept
    {
        time_t t; 
        return file_info(filename, 0, 0, &t, 0) ? t : time_t(0);
    }
    time_t file_modified(const char* filename) noexcept
    {
        time_t t; 
        return file_info(filename, 0, 0, 0, &t) ? t : time_t(0);
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
        file src{sourceFile, READONLY};
        if (!src) return false;

        file dst{destinationFile, CREATENEW};
        if (!dst) return false;

        constexpr int blockSize = 64*1024;
        char buf[blockSize];
        int totalBytesRead    = 0;
        int totalBytesWritten = 0;
        for (;;)
        {
            int bytesRead = src.read(buf, blockSize);
            if (bytesRead <= 0)
                break;
            totalBytesRead    += bytesRead;
            totalBytesWritten += dst.write(buf, bytesRead);
        }
        return totalBytesRead == totalBytesWritten;
#endif
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
        string s = { foldername,foldername + wcslen(foldername) };
        return sys_mkdir(s);
    #endif
    }

    bool create_folder(strview foldername) noexcept
    {
        if (!foldername.len || foldername == "./")
            return false;
        if (sys_mkdir(foldername))
            return true; // best case, no recursive mkdir required

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
    bool create_folder(const wstring& foldername) noexcept
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
        // these would delete the root dir. NOPE! This is always a bug.
        if (foldername.empty() || foldername == "/"_sv)
            return false;
        if (mode == non_recursive)
            return sys_rmdir(foldername); // easy path, just gently try to delete...

        vector<string> folders;
        vector<string> files;
        bool deletedChildren = true;

        if (list_alldir(folders, files, foldername))
        {
            for (const string& folder : folders)
                deletedChildren |= delete_folder(path_combine(foldername, folder), recursive);

            for (const string& file : files)
                deletedChildren |= delete_file(path_combine(foldername, file));
        }

        if (deletedChildren)
            return sys_rmdir(foldername); // should be okay to remove now

        return false; // no way to delete, since some subtree files are protected
    }


    string full_path(const char* path) noexcept
    {
        char buf[512];
        #if _WIN32
            size_t len = GetFullPathNameA(path, 512, buf, nullptr);
            if (len) normalize(buf);
            return len ? string{ buf,len } : string{};
        #else
            char* res = realpath(path, buf);
            return res ? string{ res } : string{};
        #endif
    }


    string merge_dirups(strview path) noexcept
    {
        strview pathstr = path;
        const bool isDirPath = path.back() == '/' || path.back() == '\\';
        vector<strview> folders;
        while (strview folder = pathstr.next("/\\")) {
            folders.push_back(folder);
        }

        for (int i = 0; i < (int)folders.size(); ++i)
        {
            if (i > 0 && folders[i] == ".." && folders[i-1] != "..") 
            {
                auto it = folders.begin() + i;
                folders.erase(it - 1, it + 1);
                i -= 2;
            }
        }

        string result;
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


    string file_replace_ext(strview path, strview ext)
    {
        if (strview oldext = file_ext(path))
        {
            int len = int(oldext.str - path.str);
            return strview{ path.str, len } + ext;
        }
        if (path && path.back() != '/' && path.back() != '\\')
        {
            return path + "." + ext;
        }
        return path;
    }
    
    string file_name_append(strview path, strview add)
    {
        string result = folder_path(path);
        result += file_name(path);
        result += add;
        if (strview ext = file_ext(path)) {
            result += '.';
            result += ext;
        }
        return result;
    }
    
    string file_name_replace(strview path, strview newFileName)
    {
        string result = folder_path(path) + newFileName;
        if (strview ext = file_ext(path)) {
            result += '.';
            result += ext;
        }
        return result;
    }
    
    string file_nameext_replace(strview path, strview newFileNameAndExt)
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
    wstring folder_path(const wchar_t* path) noexcept
    {
        auto* end = path + wcslen(path);
        for (; path < end; --end)
            if (*end == '/' || *end == '\\')
                break;
        return path == end ? wstring{} : wstring{path, end + 1};
    }
    wstring folder_path(const wstring& path) noexcept
    {
        auto* ptr = path.c_str();
        auto* end  = ptr + path.size();
        for (; ptr < end; --end)
            if (*end == '/' || *end == '\\')
                break;
        return ptr == end ? wstring{} : wstring{ptr, end + 1};
    }


    string& normalize(string& path, char sep) noexcept
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

    string normalized(strview path, char sep) noexcept
    {
        string res = path.to_string();
        normalize(res, sep);
        return res;
    }

    string path_combine(strview path1, strview path2) noexcept
    {
        path1.trim_end("/\\");
        path2.trim_start("/\\");
        path2.trim_end("/\\");

        if (path1.empty())
        {
            if (path2.empty()) // path_combine("", "") ==> ""
                return {};
            return path2;  // path_combine("", "/tmp.txt") ==> "tmp.txt"
        }
        if (path2.empty())
        {
            return path1;  // path_combine("tmp/", "") ==> "tmp"
        }

        string combined;
        combined.reserve(size_t(path1.len + 1 + path2.len));
        combined.append(path1.str, size_t(path1.len));
        combined.append(1, '/');
        combined.append(path2.str, size_t(path2.len));
        return combined;
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
        dir_iterator::dir_iterator(string&& dir) : dir{move(dir)} {
            char path[512]; snprintf(path, 512, "%.*s/*", (int)this->dir.length(), this->dir.c_str());
            if ((s->hFind = FindFirstFileA(path, &s->ffd)) == INVALID_HANDLE_VALUE)
                s->hFind = nullptr;
        }
        dir_iterator::~dir_iterator() { if (s->hFind) FindClose(s->hFind); }
        dir_iterator::operator bool()  const { return s->hFind != nullptr; }
        bool    dir_iterator::next()         { return s->hFind && FindNextFileA(s->hFind, &s->ffd) != 0; }
        bool    dir_iterator::is_dir() const { return s->hFind && (s->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
        strview dir_iterator::name()   const { return s->hFind ? s->ffd.cFileName : ""_sv; }
#else
        dir_iterator::dir_iterator(string&& dir) : dir{move(dir)} {
            s->e = (s->d=opendir(this->dir.c_str())) != nullptr ? readdir(s->d) : nullptr;
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
        string currentDir = path_combine(queryRoot, relPath);
        for (dir_entry e : dir_iterator{ currentDir })
        {
            bool validDir = e.is_dir && e.name != "." && e.name != "..";
            if ((validDir && dirs) || (!e.is_dir && files)) 
            {
                strview dir = abs ? (strview)currentDir : relPath;
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
            string fullpath = full_path(dir);
            if (fullpath.empty())
                return; // folder does not exist
            
            traverse_dir2(fullpath, strview{}, dirs, files, rec, abs, func);
            return;
        }
        traverse_dir2(dir, strview{}, dirs, files, rec, abs, func);
    }

    ////////////////////////////////////////////////////////////////////////////////

    int list_dirs(vector<string>& out, strview dir, bool recursive, bool fullpath) noexcept
    {
        traverse_dir(dir, true, false, recursive, fullpath, [&](string&& path, bool) {
            out.emplace_back(move(path));
        });
        return (int)out.size();
    }

    int list_files(vector<string>& out, strview dir, strview suffix, bool recursive, bool fullpath) noexcept
    {
        traverse_dir(dir, false, true, recursive, fullpath, [&](string&& path, bool) {
            if (suffix.empty() || strview{path}.ends_withi(suffix))
                out.emplace_back(move(path));
        });
        return (int)out.size();
    }

    int list_alldir(vector<string>& outDirs, vector<string>& outFiles, strview dir, bool recursive, bool fullpath) noexcept
    {
        traverse_dir(dir, true, true, recursive, fullpath, [&](string&& path, bool isDir) {
            auto& out = isDir ? outDirs : outFiles;
            out.emplace_back(move(path));
        });
        return (int)outDirs.size() + (int)outFiles.size();
    }

    vector<string> list_files(strview dir, const vector<strview>& suffixes, bool recursive, bool fullpath) noexcept
    {
        vector<string> out;
        traverse_dir(dir, false, true, recursive, fullpath, [&](string&& path, bool) {
            for (const strview& suffix : suffixes) {
                if (strview{ path }.ends_withi(suffix)) {
                    out.emplace_back(move(path));
                    return;
                }
            }
        });
        return out;
    }

    ////////////////////////////////////////////////////////////////////////////////

    string working_dir() noexcept
    {
        char path[512];
        #if _WIN32
            return string(_getcwd(path, sizeof(path)) ? path : "");
        #else
            return string(getcwd(path, sizeof(path)) ? path : "");
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

    string temp_dir() noexcept
    {
        #if _WIN32
            char path[512];
            DWORD len = GetTempPathA(sizeof(path), path);
            normalize(path);
            return { path, path + len };
        #elif TARGET_OS_IPHONE
            return getenv("TMPDIR");
        #elif __ANDROID__

            // return getContext().getExternalFilesDir(null).getPath();


            return "/data/local/tmp";
        #else
            const char* path;
            (path = getenv("TMP")) ||
            (path = getenv("TEMP")) ||
            (path = getenv("TMPDIR")) ||
            (path = getenv("TEMPDIR")) ||
            (path = "/tmp");
            return path;
        #endif
    }
    
    string home_dir() noexcept
    {
        return getenv("HOME");
    }
    
    ////////////////////////////////////////////////////////////////////////////////


} // namespace rpp
