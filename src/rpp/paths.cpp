#include "paths.h"
#include "file_io.h"
#include "delegate.h"
#include "strview.h"

#include <cerrno> // errno
#include <array> // std::array
#include <sys/stat.h> // stat,fstat
#include <sys/types.h> // S_ISDIR
#if _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define _CRT_DISABLE_PERFCRIT_LOCKS 1 // we're running single-threaded I/O only
    #include <Windows.h>
    #include <direct.h> // mkdir, getcwd
    #define USE_WINAPI_IO 1 // WINAPI IO is much faster on windows because msvcrt has lots of overhead
    #define stat64 _stat64
    #define fseeki64 _fseeki64
    #define ftelli64 _ftelli64
    #undef min
    #ifndef S_ISDIR
    #  define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
    #endif
    #ifndef S_ISREG
    #  define S_ISREG(mode)  (((mode) & S_IFMT) == S_IFREG)
    #endif
    #ifndef S_ISLNK
    #  define S_ISLNK(mode)  (((mode) & S_IFMT) == S_IFLNK)
    #endif
    #ifndef S_ISCHR
    #  define S_ISCHR(mode)  (((mode) & S_IFMT) == S_IFCHR)
    #endif
    #ifndef S_ISBLK
    #  define S_ISBLK(mode)  (((mode) & S_IFMT) == S_IFBLK)
    #endif
    #ifndef S_ISFIFO
    #  define S_ISFIFO(mode) (((mode) & S_IFMT) == S_IFIFO)
    #endif
#else
    #include <unistd.h>
    #include <dirent.h> // opendir()

    #define _fstat64 fstat
    #define stat64   stat
    #define fseeki64 fseeko
    #define ftelli64 ftello
#endif
#if __ANDROID__
    #include <jni.h>
#endif

namespace rpp /* ReCpp */
{
    ////////////////////////////////////////////////////////////////////////////////

    // a convenience helper for converting strview to char16_t OR into null-terminated C-string
    struct conv_helper
    {
        static constexpr int MAX_U16 = 1024;
        static constexpr int MAX_U8 = MAX_U16 * 2;
        union {
            char16_t path1_u16[MAX_U16];
            char     path1_u8[MAX_U8];
        #if _MSC_VER
            wchar_t  path1_w[MAX_U16];
        #endif
        };
        union {
            char16_t path2_u16[MAX_U16];
            char     path2_u8[MAX_U8];
        #if _MSC_VER
            wchar_t  path2_w[MAX_U16];
        #endif
        };

    #if _MSC_VER
        FINLINE const wchar_t* to_wcs(strview path) noexcept // UTF8 --> UTF16, and then treat as UCS2
        {
            if (to_ustring(path1_u16, MAX_U16, path.str, path.len) > 0)
                return (const wchar_t*)path1_u16;
            return nullptr; // else: cannot be converted due to invalid UTF8 sequence
        }
        FINLINE const wchar_t* to_wcs(ustrview path) noexcept
        {
            return (const wchar_t*)path.to_cstr(path1_u16, MAX_U16);
        }
        std::pair<const wchar_t*, const wchar_t*> to_wcs(strview path1, strview path2) noexcept
        {
            if (to_ustring(path1_u16, MAX_U16, path1.str, path1.len) > 0 &&
                to_ustring(path2_u16, MAX_U16, path2.str, path2.len) > 0)
            {
                return { (const wchar_t*)path1_u16, (const wchar_t*)path2_u16 };
            }
			return { nullptr, nullptr };
        }
        std::pair<const wchar_t*, const wchar_t*> to_wcs(ustrview path1, ustrview path2) noexcept
        {
            if (const char16_t* path1_u = path1.to_cstr(path1_u16, MAX_U16))
            {
                if (const char16_t* path2_u = path2.to_cstr(path2_u16, MAX_U16))
                    return { (const wchar_t*)path1_u, (const wchar_t*)path2_u };
            }
			return { nullptr, nullptr };
        }
    #endif
        FINLINE const char* to_cstr(strview path) noexcept
        {
            return path.to_cstr(path1_u8, MAX_U8);
        }
        const char* to_cstr(ustrview path) noexcept // UTF16 --> UTF8
        {
            if (to_string(path1_u8, MAX_U8, path.str, path.len) > 0)
                return path1_u8;
            return nullptr; // contains invalid UTF16 sequence, there is no point to continue
        }
        std::pair<const char*, const char*> to_cstr(strview path1, strview path2) noexcept
        {
            if (const char* path1_a = path1.to_cstr(path1_u8, MAX_U8))
            {
                if (const char* path2_a = path2.to_cstr(path2_u8, MAX_U8))
                    return { path1_a, path2_a };
            }
			return { nullptr, nullptr };
		}
        std::pair<const char*, const char*> to_cstr(ustrview path1, ustrview path2) noexcept
        {
            if (to_string(path1_u8, MAX_U8, path1.str, path1.len) > 0 &&
                to_string(path2_u8, MAX_U8, path2.str, path2.len) > 0)
            {
                return { path1_u8, path2_u8 };
            }
			return { nullptr, nullptr };
        }
    };

#if _MSC_VER
    #if USE_WINAPI_IO
    template<StringViewType T>
    static DWORD win32_file_attr(T filename) noexcept
    {
        conv_helper conv;
        // try UTF-16 first, then fall back to ANSI codepage
        // windows internally converts to UCS-2 anyway, so this won't hurt performance
        if (const wchar_t* filename_w = conv.to_wcs(filename))
            return GetFileAttributesW(filename_w);
        if (const char* filename_a = conv.to_cstr(filename))
            return GetFileAttributesA(filename_a);
        return DWORD(-1); // error
    }
    template<StringViewType T>
    static bool win32_set_file_attr(T filename, DWORD attr) noexcept
    {
		conv_helper conv;
        if (const wchar_t* filename_w = conv.to_wcs(filename))
            return SetFileAttributesW(filename_w, attr) == TRUE;
		if (const char* filename_a = conv.to_cstr(filename))
            return SetFileAttributesA(filename_a, attr) == TRUE;
		return false; // conversion failed
    }
    #endif
    using os_stat64 = struct _stat64;
    template<StringViewType T>
    static bool get_stat64(T filename, os_stat64* out) noexcept
    {
        conv_helper conv;
        // try UTF-16 first, then fall back to ANSI codepage
        if (const wchar_t* filename_w = conv.to_wcs(filename))
            return _wstat64(filename_w, out) == 0;
        if (const char* filename_a = conv.to_cstr(filename))
            return _stat64(filename_a, out) == 0;
        return false;
    }
#else // Linux
    using os_stat64 = struct stat64;
    template<StringViewType T>
    static bool get_stat64(T filename, os_stat64* out) noexcept
    {
        conv_helper conv;
        if (const char* filename_a = conv.to_cstr(filename))
            return stat64(filename_a, out) == 0;
        return false;
    }
    template<StringViewType T>
    static bool set_st_mode(T filename, mode_t mode) noexcept
    {
        conv_helper conv;
        if (const char* filename_a = conv.to_cstr(filename))
            return chmod(filename_a, mode) == 0;
        return false; // conversion failed
	}
#endif

    enum file_flags
    {
        FF_INVALID = 0, // does not exist
        FF_FILE    = (1<<1),   // exists and is a file
        FF_FOLDER  = (1 << 2), // exists and is a folder
        FF_SYMLINK = (1 << 3), // exists and is a symlink
        FF_FILE_OR_FOLDER = FF_FILE | FF_FOLDER, // exists and is either a file or a folder
    };

#if USE_WINAPI_IO
    template<StringViewType T>
    static file_flags read_file_flags(T filename) noexcept
    {
        file_flags flags = FF_INVALID;
        DWORD attr = win32_file_attr(filename);
        if (attr != DWORD(-1))
        {
            if (attr & FILE_ATTRIBUTE_DIRECTORY) flags |= FF_FOLDER;
            else                                 flags |= FF_FILE;
            if (attr & FILE_ATTRIBUTE_REPARSE_POINT) flags |= FF_SYMLINK;
        }
        return flags;
    }
#else
    template<StringViewType T>
    static file_flags read_file_flags(T filename) noexcept
    {
        file_flags flags = FF_INVALID;
        os_stat64 s;
        if (get_stat64(filename, &s) == 0/*OK*/)
        {
            if (S_ISDIR(s.st_mode)) flags |= FF_FOLDER;
            else                    flags |= FF_FILE;
            if (S_ISLNK(s.st_mode)) flags |= FF_SYMLINK;
        }
        return flags;
    }
#endif

    bool file_exists(strview filename) noexcept
    {
        file_flags f = read_file_flags(filename);
        return (f & FF_FILE) != 0;
    }
    bool file_exists(ustrview filename) noexcept
    {
        file_flags f = read_file_flags(filename);
        return (f & FF_FILE) != 0;
    }

    bool is_symlink(strview filename) noexcept
    {
        file_flags f = read_file_flags(filename);
        return (f & FF_SYMLINK) != 0;
    }
    bool is_symlink(ustrview filename) noexcept
    {
        file_flags f = read_file_flags(filename);
        return (f & FF_SYMLINK) != 0;
    }

    bool folder_exists(strview folder) noexcept
    {
        file_flags f = read_file_flags(folder);
        return (f & FF_FOLDER) != 0;
    }
    bool folder_exists(ustrview folder) noexcept
    {
        file_flags f = read_file_flags(folder);
        return (f & FF_FOLDER) != 0;
    }

    bool file_or_folder_exists(strview fileOrFolder) noexcept
    {
        file_flags f = read_file_flags(fileOrFolder);
        return (f & FF_FILE_OR_FOLDER) != 0;
    }
    bool file_or_folder_exists(ustrview fileOrFolder) noexcept
    {
        file_flags f = read_file_flags(fileOrFolder);
        return (f & FF_FILE_OR_FOLDER) != 0;
    }

#if _MSC_VER
    template<StringViewType T>
    static bool win32_symlink(T target, T link) noexcept
    {
        DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
        if (read_file_flags(target) & FF_FOLDER) flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
        conv_helper c_target, c_link;
        const wchar_t* target_w = c_target.to_wcs(target);
        const wchar_t* link_w = c_link.to_wcs(link);
        if (target_w && link_w)
            return CreateSymbolicLinkW(link_w, target_w, flags) == TRUE;
        const char* target_a = c_target.to_cstr(target);
        const char* link_a = c_link.to_cstr(link);
        if (target_a && link_a)
            return CreateSymbolicLinkA(link_a, target_a, flags) == TRUE;
        return false; // conversion failed
    }
#endif

    bool create_symlink(strview target, strview link) noexcept
    {
    #if _WIN32
        return win32_symlink(target, link);
    #else
        conv_helper c_target, c_link;
        return symlink(c_target.to_cstr(target), c_link.to_cstr(link)) == 0;
    #endif
    }
    bool create_symlink(ustrview target, ustrview link) noexcept
    {
    #if _WIN32
        return win32_symlink(target, link);
    #else
        conv_helper c_target, c_link;
        return symlink(c_target.to_cstr(target), c_link.to_cstr(link)) == 0;
    #endif
    }

#if USE_WINAPI_IO
    static time_t to_time_t(const FILETIME& ft) noexcept
    {
        ULARGE_INTEGER time;
        time.LowPart = ft.dwLowDateTime;
        time.HighPart = ft.dwHighDateTime;
        return time.QuadPart / 10000000ULL - 11644473600ULL;
    }
    // gets filesize, creation time, access time and modification time
    template<StringViewType T>
    static bool win32_stat64(T filename, os_stat64* out) noexcept
    {
        WIN32_FILE_ATTRIBUTE_DATA data;
        conv_helper conv;
        if (const wchar_t* filename_w = conv.to_wcs(filename)) {
            if (!GetFileAttributesExW(filename_w, GetFileExInfoStandard, &data))
                return false;
        } else if (const char* filename_a = conv.to_cstr(filename)) {
            if (!GetFileAttributesExA(filename_a, GetFileExInfoStandard, &data))
                return false;
        } else {
            return false; // conversion failed
        }
        LARGE_INTEGER li; li.LowPart = data.nFileSizeLow; li.HighPart = data.nFileSizeHigh;
        out->st_size = (int64)li.QuadPart;
        out->st_ctime = to_time_t(data.ftCreationTime);
        out->st_atime = to_time_t(data.ftLastAccessTime);
        out->st_mtime = to_time_t(data.ftLastWriteTime);
        return true;
    }
#endif

    bool file_info(strview filename, int64*  filesize, time_t* created,
                                     time_t* accessed, time_t* modified) noexcept
    {
        os_stat64 s;
    #if USE_WINAPI_IO
        if (win32_stat64(filename, &s))
    #else
        if (get_stat64(filename, &s))
    #endif
        {
            if (filesize) *filesize = (int64)s.st_size;
            if (created)  *created  = s.st_ctime;
            if (accessed) *accessed = s.st_atime;
            if (modified) *modified = s.st_mtime;
            return true;
        }
        return false;
    }

    bool file_info(ustrview filename, int64*  filesize, time_t* created,
                                      time_t* accessed, time_t* modified) noexcept
    {
        os_stat64 s;
    #if USE_WINAPI_IO
        if (win32_stat64(filename, &s))
    #else
        if (get_stat64(filename, &s))
    #endif
        {
            if (filesize) *filesize = (int64)s.st_size;
            if (created)  *created  = s.st_ctime;
            if (accessed) *accessed = s.st_atime;
            if (modified) *modified = s.st_mtime;
            return true;
        }
        return false;
    }

    bool file_info(intptr_t fd, int64*  filesize, time_t* created,
                                time_t* accessed, time_t* modified) noexcept
    {
        if (!fd)
            return false;
    #if USE_WINAPI_IO
        FILETIME c, a, m;
        if (GetFileTime((HANDLE)fd, created?&c:nullptr,accessed?&a: nullptr, modified?&m: nullptr)) {
            if (filesize) {
                LARGE_INTEGER li;
                GetFileSizeEx((HANDLE)fd, &li);
                *filesize = li.QuadPart;
            }
            if (created)  *created  = to_time_t(c);
            if (accessed) *accessed = to_time_t(a);
            if (modified) *modified = to_time_t(m);
            return true;
        }
        return false;
    #else
        os_stat64 s;
        if (_fstat64((int)fd, &s)) {
            if (filesize) *filesize = (int64)s.st_size;
            if (created)  *created  = s.st_ctime;
            if (accessed) *accessed = s.st_atime;
            if (modified) *modified = s.st_mtime;
            return true;
        }
        return false;
    #endif
    }

    int file_size(strview filename) noexcept
    {
        int64 s; 
        return file_info(filename, &s, nullptr, nullptr, nullptr) ? static_cast<int>(s) : 0;
    }
    int file_size(ustrview filename) noexcept
    {
        int64 s;
        return file_info(filename, &s, nullptr, nullptr, nullptr) ? static_cast<int>(s) : 0;
    }
    int64 file_sizel(strview filename) noexcept
    {
        int64 s; 
        return file_info(filename, &s, nullptr, nullptr, nullptr) ? s : 0ll;
    }
    int64 file_sizel(ustrview filename) noexcept
    {
        int64 s;
        return file_info(filename, &s, nullptr, nullptr, nullptr) ? s : 0ll;
    }
    time_t file_created(strview filename) noexcept
    {
        time_t t; 
        return file_info(filename, nullptr, &t, nullptr, nullptr) ? t : time_t(0);
    }
    time_t file_created(ustrview filename) noexcept
    {
        time_t t;
        return file_info(filename, nullptr, &t, nullptr, nullptr) ? t : time_t(0);
    }
    time_t file_accessed(strview filename) noexcept
    {
        time_t t; 
        return file_info(filename, nullptr, nullptr, &t, nullptr) ? t : time_t(0);
    }
    time_t file_accessed(ustrview filename) noexcept
    {
        time_t t;
        return file_info(filename, nullptr, nullptr, &t, nullptr) ? t : time_t(0);
    }
    time_t file_modified(strview filename) noexcept
    {
        time_t t; 
        return file_info(filename, nullptr, nullptr, nullptr, &t) ? t : time_t(0);
    }
    time_t file_modified(ustrview filename) noexcept
    {
        time_t t;
        return file_info(filename, nullptr, nullptr, nullptr, &t) ? t : time_t(0);
    }

    bool delete_file(strview filename) noexcept
    {
        conv_helper conv;
    #if _MSC_VER
        return ::_wremove(conv.to_wcs(filename)) == 0;
    #else
        return ::remove(conv.to_cstr(filename)) == 0;
    #endif
    }
    bool delete_file(ustrview filename) noexcept
    {
        conv_helper conv;
    #if _MSC_VER
        return ::_wremove(conv.to_wcs(filename)) == 0;
    #else
        return ::remove(conv.to_cstr(filename)) == 0;
    #endif
    }

#if !USE_WINAPI_IO
    static bool copy_file_buffered(strview sourceFile, strview destinationFile) noexcept
    {
        file src { sourceFile, file::READONLY };
        if (!src) return false;

        file dst { destinationFile, file::CREATENEW };
        if (!dst) return false;

        // copy the file access rights
        if (!copy_file_mode(sourceFile, destinationFile))
            return false;

        // special case: empty file
        int64 size = src.sizel();
        if (size == 0) return true;

        constexpr int64 blockSize = 64 * 1024;
        char buf[blockSize];
        int64 totalBytesRead = 0;
        int64 totalBytesWritten = 0;
        for (;;)
        {
            int64 bytesToRead = std::min(size - totalBytesRead, blockSize);
            if (bytesToRead <= 0)
                break;
            int bytesRead = src.read(buf, (int)bytesToRead);
            if (bytesRead <= 0)
                break;
            totalBytesRead += bytesRead;
            totalBytesWritten += dst.write(buf, bytesRead);
        }
        return totalBytesRead == totalBytesWritten;
    }
#endif

    bool copy_file(strview sourceFile, strview destinationFile) noexcept
    {
    #if USE_WINAPI_IO
        conv_helper conv;
        // CopyFileA/W always copies the file access rights
		if (auto [src, dest] = conv.to_wcs(sourceFile, destinationFile); src && dest)
			return CopyFileW(src, dest, /*failIfExists:*/false) == TRUE;
		if (auto [src, dest] = conv.to_cstr(sourceFile, destinationFile); src && dest)
            return CopyFileA(src, dest, /*failIfExists:*/false) == TRUE;
        return false;
    #else
		return copy_file_buffered(sourceFile, destinationFile);
    #endif
    }

    bool copy_file_mode(strview sourceFile, strview destinationFile) noexcept
    {
    #if _WIN32
        DWORD attr = win32_file_attr(sourceFile);
        return attr != DWORD(-1) && win32_set_file_attr(destinationFile, attr);
    #else
        os_stat64 s;
		return get_stat64(sourceFile, &s) && set_st_mode(destinationFile, s.st_mode);
    #endif
    }
    bool copy_file_mode(ustrview sourceFile, ustrview destinationFile) noexcept
    {
    #if _WIN32
        DWORD attr = win32_file_attr(sourceFile);
		return attr != DWORD(-1) && win32_set_file_attr(destinationFile, attr);
    #else
        os_stat64 s;
        return get_stat64(sourceFile, &s) && set_st_mode(destinationFile, s.st_mode);
    #endif
    }

    bool copy_file_if_needed(strview sourceFile, strview destinationFile) noexcept
    {
        if (file_exists(destinationFile))
            return true;
        return copy_file(sourceFile, destinationFile);
    }
    bool copy_file_if_needed(ustrview sourceFile, ustrview destinationFile) noexcept
    {
        if (file_exists(destinationFile))
            return true;
        return copy_file(sourceFile, destinationFile);
    }

    bool copy_file_into_folder(strview sourceFile, strview destinationFolder) noexcept
    {
        string destFile = path_combine(destinationFolder, file_nameext(sourceFile));
        return copy_file(sourceFile, destFile);
    }
    bool copy_file_into_folder(ustrview sourceFile, ustrview destinationFolder) noexcept
    {
        ustring destFile = path_combine(destinationFolder, file_nameext(sourceFile));
        return copy_file(sourceFile, destFile);
    }
    
    // on failure, check errno for details, if folder (or file) exists, we consider it a success
    // @note Might not be desired behaviour for all cases, so use file_exists or folder_exists.
    static bool sys_mkdir(strview foldername) noexcept
    {
        conv_helper conv;
    #if _WIN32
        return _wmkdir(conv.to_wcs(foldername)) == 0 || errno == EEXIST;
    #else
        return mkdir(conv.to_cstr(foldername), 0755) == 0 || errno == EEXIST;
    #endif
    }
    static bool sys_mkdir(ustrview foldername) noexcept
    {
        conv_helper conv;
    #if _WIN32
        return _wmkdir(conv.to_wcs(foldername)) == 0 || errno == EEXIST;
    #else
        return mkdir(conv.to_cstr(foldername), 0755) == 0 || errno == EEXIST;
    #endif
    }


    template<StringViewType T>
    struct strview_constants
    {
        using tchar = typename T::char_t;

        // slashes: "/\\"
        static constexpr tchar slashes_a[3] = { (tchar)'/', (tchar)'\\', (tchar)'\0' };
        static constexpr T slashes_sv { slashes_a, 2 };

        // dotslash: "./"
        static constexpr tchar dotslash_a[3] = { (tchar)'.', (tchar)'/', (tchar)'\0' };
        static constexpr T dotslash_sv { dotslash_a, 2 };

        // dotslashes: "./\\"
        static constexpr tchar dotslashes_a[4] = { (tchar)'.', (tchar)'/', (tchar)'\\', (tchar)'\0' };
        static constexpr T dotslashes_sv { dotslashes_a, 3 };

        // dotdot: "."
        static constexpr tchar dot_a[3] = { (tchar)'.', (tchar)'\0' };
        static constexpr T dot_sv { dot_a, 1 };

        // dotdot: ".."
        static constexpr tchar dotdot_a[3] = { (tchar)'.', (tchar)'.', (tchar)'\0' };
        static constexpr T dotdot_sv { dotdot_a, 2 };

        // dotdotslash: "../"
        static constexpr tchar dotdotslash_a[4] = { (tchar)'.', (tchar)'.', (tchar)'/', (tchar)'\0' };
        static constexpr T dotdotslash_sv { dotdotslash_a, 3 };
    };


    template<StringViewType T>
    static bool create_folder(T foldername) noexcept
    {
        using tchar = typename T::char_t;
        if (!foldername.len) // fail on empty strings purely for catching bugs
            return false;

        // foldername == "./", current folder already exist
        if (foldername == strview_constants<T>::dotslash_sv)
            return true;

        if (sys_mkdir(foldername)) // best case, no recursive mkdir required
            return true;

        // ugh, need to work our way upward to find a root dir that exists:
        // @note heavily optimized to minimize folder_exists() and mkdir() syscalls
        const tchar* fs = foldername.begin();
        const tchar* fe = foldername.end();
        const tchar* p = fe;
        while ((p = T{ fs,p }.rfindany(strview_constants<T>::slashes_a, 2)) != nullptr)
        {
            if (folder_exists(T{ fs,p }))
                break;
        }

        // now create all the parent dir between:
        p = p ? p + 1 : fs; // handle /dir/ vs dir/ case

        while (const tchar* e = T{ p,fe }.findany(strview_constants<T>::slashes_a, 2))
        {
            if (!sys_mkdir(T{ fs,e }))
                return false; // ugh, something went really wrong here...
            p = e + 1;
        }
        return sys_mkdir(foldername); // and now create the final dir
    }
    bool create_folder(strview foldername) noexcept
    {
        return create_folder<strview>(foldername);
    }
    bool create_folder(ustrview foldername) noexcept
    {
        return create_folder<ustrview>(foldername);
    }


    static bool sys_rmdir(strview foldername) noexcept
    {
        conv_helper conv;
    #if _WIN32
        return _wrmdir(conv.to_wcs(foldername)) == 0;
    #else
        return rmdir(conv.to_cstr(foldername)) == 0;
    #endif
    }
    static bool sys_rmdir(ustrview foldername) noexcept
    {
        conv_helper conv;
    #if _WIN32
        return _wrmdir(conv.to_wcs(foldername)) == 0;
    #else
        return rmdir(conv.to_cstr(foldername)) == 0;
    #endif
    }

    template<StringViewType T>
    static bool delete_folder(T foldername, delete_mode mode) noexcept
    {
        // these would delete the root dir...
        if (foldername.empty())
            return false;
        //  || foldername == "/"_sv
        if (foldername.len == 1 && foldername[0] == '/')
            return false;
        if (mode == delete_mode::non_recursive)
            return sys_rmdir(foldername); // easy path, just gently try to delete...

        using tstring = typename T::string_t;
        std::vector<tstring> folders;
        std::vector<tstring> files;
        bool deletedChildren = true;

        if (list_alldir(folders, files, foldername))
        {
            for (const tstring& folder : folders)
                deletedChildren |= delete_folder(path_combine(foldername, folder), delete_mode::recursive);

            for (const tstring& file : files)
                deletedChildren |= delete_file(path_combine(foldername, file));
        }

        if (deletedChildren)
            return sys_rmdir(foldername); // should be okay to remove now

        return false; // no way to delete, since some subtree files are protected
    }
    bool delete_folder(strview foldername, delete_mode mode) noexcept
    {
        return delete_folder<strview>(foldername, mode);
    }
    bool delete_folder(ustrview foldername, delete_mode mode) noexcept
    {
        return delete_folder<ustrview>(foldername, mode);
    }

    string full_path(strview path) noexcept
    {
        conv_helper conv;
    #if _WIN32
        if (const wchar_t* path_w = conv.to_wcs(path))
        {
            DWORD len = GetFullPathNameW(path_w, sizeof(conv.path2_w), conv.path2_w, nullptr);
            return to_string(conv.path2_u16, len);
        }
        if (const char* path_a = conv.to_cstr(path))
        {
            DWORD len = GetFullPathNameA(path_a, sizeof(conv.path2_u8), conv.path2_u8, nullptr);
            if (len == 0) return string{};
            return string{ conv.path2_u8, conv.path2_u8 + len };
        }
        return string{};
    #else
        // TODO: potential buffer overflow
        char* res = realpath(conv.to_cstr(path), conv.path2_u8);
        return res ? string{ res } : string{};
    #endif
    }
    ustring full_path(ustrview path) noexcept
    {
        conv_helper conv;
#if _WIN32
        if (const wchar_t* path_w = conv.to_wcs(path))
        {
            DWORD len = GetFullPathNameW(path_w, sizeof(conv.path2_w), conv.path2_w, nullptr);
            if (len == 0) return ustring{};
            return ustring{ conv.path2_u16, conv.path2_u16 + len };
        }
        if (const char* path_a = conv.to_cstr(path))
        {
            DWORD len = GetFullPathNameA(path_a, sizeof(conv.path2_u8), conv.path2_u8, nullptr);
            return to_ustring(conv.path2_u8, len);
        }
        return ustring{};
#else
        // TODO: potential buffer overflow
        char* res = realpath(conv.to_cstr(path), conv.path2_u8);
        return res ? string{ res } : string{};
#endif
    }

    template<StringViewType T>
    static auto merge_dirups(T path) noexcept -> T::string_t
    {
        using tchar = typename T::char_t;
        T pathstr = path;
        const bool isDirPath = path.back() == (tchar)'/' || path.back() == (tchar)'\\';
        std::vector<T> folders;
        while (T folder = pathstr.next(strview_constants<T>::slashes_a)) {
            folders.push_back(folder);
        }

        for (size_t i = 0; i < folders.size(); ++i)
        {
            if (i > 0 && folders[i] == strview_constants<T>::dotdot_sv
                      && folders[i - 1] != strview_constants<T>::dotdot_sv)
            {
                auto it = folders.begin() + i;
                folders.erase(it - 1, it + 1);
                i -= 2;
            }
        }

        T::string_t result;
        for (const T& folder : folders) {
            result += folder;
            result += (tchar)'/';
        }
        if (!isDirPath) { // it's a filename? so pop the last /
            result.pop_back();
        }
        return result;
    }
    string merge_dirups(strview path) noexcept
    {
        return merge_dirups<strview>(path);
    }
    ustring merge_dirups(ustrview path) noexcept
    {
        return merge_dirups<ustrview>(path);
    }


    static constexpr int EXT_LEN_MAX = 8; // max length of file extension, including dot
    
    template<StringViewType T>
    static auto file_name(T path) noexcept -> T
    {
        T nameext = file_nameext(path);
        if (auto* ptr = nameext.substr(nameext.len - EXT_LEN_MAX).rfind('.'))
            return T{ nameext.str, ptr };
        return nameext; // no extension found, return the whole name
    }
    strview file_name(strview path) noexcept
    {
        return file_name<strview>(path);
    }
    ustrview file_name(ustrview path) noexcept
    {
        return file_name<ustrview>(path);
    }


    template<StringViewType T>
    static auto file_nameext(T path) noexcept -> T
    {
        if (auto* str = path.rfindany(strview_constants<T>::slashes_a))
            return T{ str + 1, path.end() };
        return path; // assume it's just a file name
    }
    strview file_nameext(strview path) noexcept
    {
        return file_nameext<strview>(path);
    }
    ustrview file_nameext(ustrview path) noexcept
    {
        return file_nameext<ustrview>(path);
    }


    template<StringViewType T>
    static auto file_ext(T path) noexcept -> T
    {
        if (auto* ptr = path.substr(path.len - EXT_LEN_MAX).rfindany(strview_constants<T>::dotslashes_a)) {
            if (*ptr == (T::char_t)'.') return T{ ptr + 1, path.end() };
        }
        return T{}; // no extension found
    }
    strview file_ext(strview path) noexcept
    {
        return file_ext<strview>(path);
    }
    ustrview file_ext(ustrview path) noexcept
    {
        return file_ext<ustrview>(path);
    }


    template<StringViewType T>
    static auto file_replace_ext(T path, T ext) noexcept -> T::string_t
    {
        if (T oldext = file_ext(path))
        {
            int len = int(oldext.str - path.str);
            return concat(T{ path.str, len }, ext);
        }
        if (path && path.back() != (T::char_t)'/' && path.back() != (T::char_t)'\\')
        {
            return concat(path, strview_constants<T>::dot_sv, ext);
        }
        return path;
    }
    string file_replace_ext(strview path, strview ext) noexcept
    {
        return file_replace_ext<strview>(path, ext);
    }
    ustring file_replace_ext(ustrview path, ustrview ext) noexcept
    {
        return file_replace_ext<ustrview>(path, ext);
    }
    

    template<StringViewType T>
    static auto file_name_append(T path, T add) noexcept -> T::string_t
    {
        T::string_t result = concat(folder_path(path), file_name(path), add);
        if (T ext = file_ext(path)) {
            result += (T::char_t)'.';
            result += ext;
        }
        return result;
    }
    string file_name_append(strview path, strview add) noexcept
    {
        return file_name_append<strview>(path, add);
    }
    ustring file_name_append(ustrview path, ustrview add) noexcept
    {
        return file_name_append<ustrview>(path, add);
    }
    

    template<StringViewType T>
    static auto file_name_replace(T path, T newFileName) noexcept -> T::string_t
    {
        T::string_t result = concat(folder_path(path), newFileName);
        if (T ext = file_ext(path)) {
            result += (T::char_t)'.';
            result += ext;
        }
        return result;
    }
    string file_name_replace(strview path, strview newFileName) noexcept
    {
        return file_name_replace<strview>(path, newFileName);
    }
    ustring file_name_replace(ustrview path, ustrview newFileName) noexcept
    {
        return file_name_replace<ustrview>(path, newFileName);
    }
    

    string file_nameext_replace(strview path, strview newFileNameAndExt) noexcept
    {
        return concat(folder_path(path), newFileNameAndExt);
    }
    ustring file_nameext_replace(ustrview path, ustrview newFileNameAndExt) noexcept
    {
        return concat(folder_path(path), newFileNameAndExt);
    }


    template<StringViewType T>
    static T folder_name(T path) noexcept
    {
        T folder = folder_path(path);
        if (folder)
        {
            if (const auto* str = folder.chomp_last().rfindany(strview_constants<T>::slashes_a))
                return T{ str + 1, folder.end() };
        }
        return folder;
    }
    strview folder_name(strview path) noexcept
    {
        return folder_name<strview>(path);
    }
    ustrview folder_name(ustrview path) noexcept
    {
        return folder_name<ustrview>(path);
    }


    template<StringViewType T>
    static T folder_path(T path) noexcept
    {
        if (const auto* end = path.rfindany(strview_constants<T>::slashes_a))
            return T{ path.str, end + 1 };
        return T{}; // no folder path found, return empty
    }
    strview folder_path(strview path) noexcept
    {
        return folder_path<strview>(path);
    }
    ustrview folder_path(ustrview path) noexcept
    {
        return folder_path<ustrview>(path);
    }


    template<typename Char>
    static void normalize_nullterm(Char* path, Char sep) noexcept
    {
        if (sep == (Char)'/') {
            for (Char* s = path; *s; ++s) if (ch == (Char)'\\') ch = (Char)'/';
        }
        else if (sep == (Char)'\\') {
            for (Char* s = path; *s; ++s) if (ch == (Char)'/')  ch = (Char)'\\';
        }
        // else: ignore any other separators
    }
    string& normalize(string& path, char sep) noexcept
    {
        normalize_nullterm(path.data(), sep);
    }
    char* normalize(char* path, char sep) noexcept
    {
        normalize_nullterm(path, sep);
    }
    ustring& normalize(ustring& path, char16_t sep) noexcept
    {
        normalize_nullterm(path.data(), sep);
    }
    char16_t* normalize(char16_t* path, char16_t sep) noexcept
    {
        normalize_nullterm(path, sep);
    }


    string normalized(strview path, char sep) noexcept
    {
        string res = path.to_string();
        normalize_nullterm(res.data(), sep);
        return res;
    }
    ustring normalized(ustrview path, char16_t sep) noexcept
    {
        ustring res = path.to_string();
        normalize_nullterm(res.data(), sep);
        return res;
    }


    template<StringViewType T, size_t N>
    static T::string_t slash_combine(const std::array<T, N>& args)
    {
        size_t res = args[0].size();
        for (size_t i = 1; i < N; ++i) {
            if (auto n = static_cast<size_t>(args[i].size())) {
                if (res != 0)
                    res += 1;
                res += n;
            }
        }
        
        T::string_t result; result.reserve(res);
        result.append(args[0].data(), args[0].size());

        for (size_t i = 1; i < N; ++i) {
            if (auto n = static_cast<size_t>(args[i].size())) {
                if (!result.empty())
                    result.append(1, (T::char_t)'/');
                result.append(args[i].data(), n);
            }
        }
        return result;
    }

    string path_combine(strview path1, strview path2) noexcept
    {
        path1.trim_end(strview_constants<strview>::slashes_a);
        path2.trim(strview_constants<strview>::slashes_a);
        return slash_combine(std::array<strview, 2>{{path1, path2}});
    }
    string path_combine(strview path1, strview path2, strview path3) noexcept
    {
        path1.trim_end(strview_constants<strview>::slashes_a);
        path2.trim(strview_constants<strview>::slashes_a);
        path3.trim(strview_constants<strview>::slashes_a);
        return slash_combine(std::array<strview, 3>{{path1, path2, path3}});
    }
    string path_combine(strview path1, strview path2, strview path3, strview path4) noexcept
    {
        path1.trim_end(strview_constants<strview>::slashes_a);
        path2.trim(strview_constants<strview>::slashes_a);
        path3.trim(strview_constants<strview>::slashes_a);
        path4.trim(strview_constants<strview>::slashes_a);
        return slash_combine(std::array<strview, 4>{{path1, path2, path3, path4}});
    }

    ustring path_combine(ustrview path1, ustrview path2) noexcept
    {
        path1.trim_end(strview_constants<ustrview>::slashes_a);
        path2.trim(strview_constants<ustrview>::slashes_a);
        return slash_combine(std::array<ustrview, 2>{{ path1, path2 }});
    }
    ustring path_combine(ustrview path1, ustrview path2, ustrview path3) noexcept
    {
        path1.trim_end(strview_constants<ustrview>::slashes_a);
        path2.trim(strview_constants<ustrview>::slashes_a);
        path3.trim(strview_constants<ustrview>::slashes_a);
        return slash_combine(std::array<ustrview, 3>{{ path1, path2, path3 }});
    }
    ustring path_combine(ustrview path1, ustrview path2, ustrview path3, ustrview path4) noexcept
    {
        path1.trim_end(strview_constants<ustrview>::slashes_a);
        path2.trim(strview_constants<ustrview>::slashes_a);
        path3.trim(strview_constants<ustrview>::slashes_a);
        path4.trim(strview_constants<ustrview>::slashes_a);
        return slash_combine(std::array<ustrview, 4>{{ path1, path2, path3, path4 }});
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
    dir_iterator::impl* dir_iterator::dummy::operator->() noexcept
    {
#if _WIN32
        static_assert(sizeof(ffd) == sizeof(impl::ffd), "dir_iterator::dummy size mismatch");
#endif
        return reinterpret_cast<impl*>(this);
    }
    const dir_iterator::impl* dir_iterator::dummy::operator->() const noexcept
    {
        return reinterpret_cast<const impl*>(this);
    }

#if _WIN32
    // ReSharper disable CppSomeObjectMembersMightNotBeInitialized
    dir_iterator::dir_iterator(string&& dir) noexcept : dir{std::move(dir)} {  // NOLINT
        char path[512];
        if (this->dir.empty()) { // handle dir=="" special case
            snprintf(path, 512, "./*");
        } else {
            snprintf(path, 512, "%.*s/*", static_cast<int>(this->dir.length()), this->dir.c_str());
        }
        if ((s->hFind = FindFirstFileA(path, &s->ffd)) == INVALID_HANDLE_VALUE)
            s->hFind = nullptr;
    // ReSharper restore CppSomeObjectMembersMightNotBeInitialized
    }
    dir_iterator::~dir_iterator() noexcept { if (s->hFind) FindClose(s->hFind); }
    dir_iterator::operator bool() const noexcept { return s->hFind != nullptr; }
    strview dir_iterator::name() const noexcept { return s->hFind ? s->ffd.cFileName : ""_sv; }
    bool dir_iterator::next() noexcept { return s->hFind && FindNextFileA(s->hFind, &s->ffd) != 0; }
    bool dir_iterator::is_dir() const noexcept { return s->hFind && (s->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
    bool dir_iterator::is_file() const noexcept { return s->hFind && (s->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0; }
    bool dir_iterator::is_symlink() const noexcept { return s->hFind && (s->ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0; }
    bool dir_iterator::is_device() const noexcept { return s->hFind && (s->ffd.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) != 0; }
#else
    dir_iterator::dir_iterator(string&& dir) noexcept : dir{std::move(dir)}
    {
        const char* path = this->dir.empty() ? "." : this->dir.c_str();
        s->d = opendir(path);
        s->e = s->d != nullptr ? readdir(s->d) : nullptr;
    }
    dir_iterator::~dir_iterator() noexcept { if (s->d) closedir(s->d); }
    dir_iterator::operator bool() const noexcept { return s->d && s->e; }
    strview dir_iterator::name() const noexcept { return s->e ? strview{s->e->d_name} : strview{}; }
    bool dir_iterator::next() noexcept {return s->d && (s->e = readdir(s->d)) != nullptr; }
    bool dir_iterator::is_dir() const noexcept {
        if (s->e && s->e->d_type) return s->e->d_type == DT_DIR;
        struct stat st;
        return ::stat(path_combine(full_path(), name()).c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }
    bool dir_iterator::is_file() const noexcept {
        if (s->e && s->e->d_type) return s->e->d_type == DT_REG;
        struct stat st;
        return ::stat(path_combine(full_path(), name()).c_str(), &st) == 0 && S_ISREG(st.st_mode);
    }
    bool dir_iterator::is_symlink() const noexcept {
        if (s->e && s->e->d_type) return s->e->d_type == DT_LNK;
        struct stat st;
        return ::stat(path_combine(full_path(), name()).c_str(), &st) == 0 && S_ISLNK(st.st_mode);
    }
    bool dir_iterator::is_device() const noexcept {
        if (s->e && s->e->d_type) {
            int dt = s->e->d_type;
            return dt == DT_BLK || dt == DT_CHR || dt == DT_FIFO || dt == DT_SOCK;
        }
        struct stat st;
        if (::stat(path_combine(full_path(), name()).c_str(), &st) == 0) {
            auto m = st.st_mode;
            return S_ISBLK(m) || S_ISCHR(m) || S_ISFIFO(m) || S_ISSOCK(m);
        }
        return false;
    }
#endif



    ////////////////////////////////////////////////////////////////////////////////

    using DirTraverseFunc = rpp::delegate<void(string&& path, bool isDir)>;

    // @param queryRoot The original path passed to the query.
    //                  For abs listing, this must be an absolute path value!
    //                  For rel listing, this is only used for opening the directory
    // @param relPath Relative path from search root, ex: "src", "src/session/util", etc.
    static NOINLINE void traverse_dir2(
        strview queryRoot, strview relPath, bool dirs, bool files, bool rec, bool abs,
        const DirTraverseFunc& func) noexcept
    {
        string currentDir = path_combine(queryRoot, relPath);
        for (dir_entry e : dir_iterator{ currentDir })
        {
            if (e.is_special_dir())
                continue; // skip . and ..
            if (e.is_dir())
            {
                if (dirs)
                {
                    strview dir = abs ? strview{currentDir} : relPath;
                    func(path_combine(dir, e.name), /*isDir*/true);
                }
                if (rec)
                {
                    traverse_dir2(queryRoot, path_combine(relPath, e.name), dirs, files, rec, abs, func);
                }
            }
            else // file, symlink or device
            {
                if (files)
                {
                    strview dir = abs ? strview{currentDir} : relPath;
                    func(path_combine(dir, e.name), /*isDir*/false);
                }
            }
        }
    }
    static NOINLINE void traverse_dir(
        strview dir, bool dirs, bool files, bool rec, bool abs,
        const DirTraverseFunc& func) noexcept
    {
        if (abs)
        {
            string fullpath = full_path(dir.empty() ? "." : dir);
            if (!fullpath.empty()) // folder does not exist
                traverse_dir2(fullpath, strview{}, dirs, files, rec, abs, func);
        }
        else
        {
            traverse_dir2(dir, strview{}, dirs, files, rec, abs, func);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////

    int list_dirs(std::vector<string>& out, strview dir, bool recursive, bool fullpath) noexcept
    {
        traverse_dir(dir, true, false, recursive, fullpath, [&](string&& path, bool) {
            out.emplace_back(std::move(path));
        });
        return static_cast<int>(out.size());
    }

    int list_dirs_relpath(std::vector<string>& out, strview dir, bool recursive) noexcept
    {
        traverse_dir(dir, true, false, recursive, false, [&](string&& path, bool) {
            out.emplace_back(path_combine(dir, path));
        });
        return static_cast<int>(out.size());
    }

    int list_files(std::vector<string>& out, strview dir, strview suffix, bool recursive, bool fullpath) noexcept
    {
        traverse_dir(dir, false, true, recursive, fullpath, [&](string&& path, bool) {
            if (suffix.empty() || strview{path}.ends_withi(suffix))
                out.emplace_back(std::move(path));
        });
        return static_cast<int>(out.size());
    }

    int list_files_relpath(std::vector<string>& out, strview dir, strview suffix, bool recursive) noexcept
    {
        traverse_dir(dir, false, true, recursive, false, [&](string&& path, bool) {
            if (suffix.empty() || strview{path}.ends_withi(suffix))
                out.emplace_back(path_combine(dir, path));
        });
        return static_cast<int>(out.size());
    }

    int list_alldir(std::vector<string>& outDirs, std::vector<string>& outFiles, strview dir, bool recursive, bool fullpath) noexcept
    {
        traverse_dir(dir, true, true, recursive, fullpath, [&](string&& path, bool isDir) {
            auto& out = isDir ? outDirs : outFiles;
            out.emplace_back(std::move(path));
        });
        return static_cast<int>(outDirs.size() + outFiles.size());
    }

    int list_alldir_relpath(std::vector<string>& outDirs, std::vector<string>& outFiles, strview dir, bool recursive) noexcept
    {
        traverse_dir(dir, true, true, recursive, false, [&](string&& path, bool isDir) {
            auto& out = isDir ? outDirs : outFiles;
            out.emplace_back(path_combine(dir, path));
        });
        return static_cast<int>(outDirs.size() + outFiles.size());
    }

    std::vector<string> list_files(strview dir, const std::vector<strview>& suffixes, bool recursive, bool fullpath) noexcept
    {
        std::vector<string> out;
        traverse_dir(dir, false, true, recursive, fullpath, [&](string&& path, bool) {
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

    template<class TChar>
    static void append_slash(TChar* path, int& len) noexcept
    {
        if (path[len-1] != TChar('/'))
            path[len++] = TChar('/');
    }

    #if _WIN32
    template<class TChar>
    static void win32_fixup_path(TChar* path, int& len) noexcept
    {
        normalize(path);
        append_slash(path, len);
    }
    #endif

    string working_dir() noexcept
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

    string module_dir(void* moduleObject) noexcept
    {
        (void)moduleObject;
        #if _WIN32
            char path[512];
            int len = (int)GetModuleFileNameA(reinterpret_cast<HMODULE>(moduleObject), path, 512);
            normalize(path);
            return folder_path(strview{path, len});
        #elif __APPLE__
            extern string apple_module_dir(void* moduleObject) noexcept;
            return apple_module_dir(moduleObject);
        #else
            // @todo Implement missing platforms: __linux__, __ANDROID__, RASPI
            return working_dir();
        #endif
    }

    string module_path(void* moduleObject) noexcept
    {
        (void)moduleObject;
        #if _WIN32
            char path[512];
            int len = (int)GetModuleFileNameA(reinterpret_cast<HMODULE>(moduleObject), path, 512);
            normalize(path);
            return { path, path+len };
        #elif __APPLE__
            extern string apple_module_path(void* moduleObject) noexcept;
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

    string temp_dir() noexcept
    {
        #if _WIN32
            char path[512];
            int len = (int)GetTempPathA(sizeof(path), path);
            win32_fixup_path(path, len);
            return { path, path + len };
        #elif __APPLE__
            extern string apple_temp_dir() noexcept;
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
    
    string home_dir() noexcept
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

    ustring home_dirw() noexcept
    {
        #if _MSC_VER
            size_t len = 0;
            wchar_t path[512];
            _wgetenv_s(&len, path, sizeof(path), L"USERPROFILE");
            if (len == 0)
                return {};
            int slen = static_cast<int>(len) - 1;
            win32_fixup_path(path, slen);
            return { (char16_t*)path, (char16_t*)path + len };
        #else
            return to_u16string(home_dir());
        #endif
    }
    
    ////////////////////////////////////////////////////////////////////////////////
} // namespace rpp
