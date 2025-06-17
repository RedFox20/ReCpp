#pragma once
/**
 * Inlined path utilities for both paths.cpp and file_io.cpp
 */
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
#else
    #include <unistd.h>
    #include <dirent.h> // opendir()
    #include <fcntl.h> // fallocate
    #define _fstat64 fstat
    #define stat64   stat
    #define fseeki64 fseeko
    #define ftelli64 ftello
#endif

namespace rpp
{
    // a convenience helper for converting strview to char16_t OR into null-terminated C-string
    struct conv_buffer
    {
        // PATH_MAX on Linux is 4096
        static constexpr int MAX_A = 4096;
        static constexpr int MAX_U = MAX_A / 2;
        union {
            char16_t path_u[MAX_U];
            char     path_a[MAX_A];
        #if _MSC_VER
            wchar_t  path_w[MAX_U];
        #endif
        };
    #if _MSC_VER
        FINLINE const wchar_t* to_wstr(strview path) noexcept // UTF8 --> UTF16, and then treat as UCS2
        {
            if (to_ustring(path_u, MAX_U, path.str, path.len) >= 0)
                return (const wchar_t*)path_u;
            return nullptr; // else: cannot be converted due to invalid UTF8 sequence
        }
        FINLINE const wchar_t* to_wstr(ustrview path) noexcept
        {
            int len = path.len;
            if (len == 0) return L""; // empty string, return empty wchar_t string
            if (path.str[len] == u'\0') return (const wchar_t*)path.str; // already a null-terminated string, return it as is
            size_t n = size_t((len < MAX_U) ? len : MAX_U - 1);
            memcpy(path_w, path.str, sizeof(char16_t) * n);
            path_w[n] = L'\0';
            return (const wchar_t*)path_w;
        }
    #endif
        FINLINE const char* to_cstr(strview path) noexcept
        {
            int len = path.len;
            if (len == 0) return ""; // empty string, return empty wchar_t string
            if (path.str[len] == '\0') return path.str; // already a null-terminated string, return it as is
            size_t n = size_t((len < MAX_A) ? len : MAX_A - 1);
            memcpy(path_a, path.str, sizeof(char) * n);
            path_a[n] = '\0';
            return path_a;

            return path.to_cstr(path_a, MAX_A);
        }
        FINLINE const char* to_cstr(ustrview path) noexcept // UTF16 --> UTF8
        {
            if (to_string(path_a, MAX_A, path.str, path.len) >= 0)
                return path_a;
            return nullptr; // contains invalid UTF16 sequence, there is no point to continue
        }
    };

#if _MSC_VER
    struct wchar_conv
    {
        conv_buffer buf;
        const wchar_t* wstr = nullptr;
        explicit operator bool() const noexcept { return !!wstr; }

        template<StringViewType T>
        wchar_conv(T path) noexcept : wstr{buf.to_wstr(path)}
        {
        }
    };
    struct wchar_dual_conv
    {
        conv_buffer buf1;
        conv_buffer buf2;
        const wchar_t* wstr1 = nullptr;
        const wchar_t* wstr2 = nullptr;
        explicit operator bool() const noexcept { return !!wstr1; }

        template<StringViewType T>
        wchar_dual_conv(T path1, T path2) noexcept
        {
            const wchar_t* w1 = buf1.to_wstr(path1);
            const wchar_t* w2 = buf2.to_wstr(path2);
            if (w1 && w2) {
                wstr1 = w1;
                wstr2 = w2;
            }
        }
    };
#else
    // conv helper which converts UTF16 to multibyte UTF8
    struct multibyte_conv
    {
        conv_buffer buf;
        const char* cstr = nullptr;
        explicit operator bool() const noexcept { return cstr; }

        template<StringViewType T>
        wchar_fallback_conv(T path) noexcept
            : cstr{buf.to_cstr(path)}
        {
        }
    };
    // conv helper where both conversions must succeed, otherwise result is empty
    struct multibyte_dual_conv
    {
        conv_buffer buf1;
        conv_buffer buf2;
        const char* cstr1 = nullptr;
        const char* cstr2 = nullptr;
        explicit operator bool() const noexcept { return cstr1; }

        template<StringViewType T>
        multibyte_dual_conv(T path1, T path2) noexcept
        {
            const char* a1 = buf1.to_cstr(path1);
            const char* a2 = buf2.to_cstr(path2);
            if (a1 && a2) {
                cstr1 = a1;
                cstr2 = a2;
            }
        }
    };
#endif

#if _MSC_VER
    template<StringViewType T>
    static DWORD win32_file_attr(T filename) noexcept
    {
        // convert to UTF-16, windows internally converts to UCS-2 anyway, so this won't hurt performance
        if (wchar_conv conv{ filename })
            return GetFileAttributesW(conv.wstr);
        return DWORD(-1); // error
    }
    template<StringViewType T>
    static bool win32_set_file_attr(T filename, DWORD attr) noexcept
    {
        if (wchar_conv conv{ filename })
            return SetFileAttributesW(conv.wstr, attr) == TRUE;
        return false; // conversion failed
    }

    using os_stat64 = struct ::_stat64;

    template<StringViewType T>
    static bool sys_stat64(T filename, os_stat64* out) noexcept
    {
        if (wchar_conv conv{ filename })
            return _wstat64(conv.wstr, out) == 0;
        return false;
    }
#else // Linux

    using os_stat64 = struct ::stat64;

    template<StringViewType T>
    static bool sys_stat64(T filename, os_stat64* out) noexcept
    {
        multibyte_conv conv { filename };
        if (conv.cstr) return stat64(conv.cstr, out) == 0;
        return false;
    }
    template<StringViewType T>
    static bool set_st_mode(T filename, mode_t mode) noexcept
    {
        multibyte_conv conv{ filename };
        if (conv.cstr) return chmod(conv.cstr, mode) == 0;
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

#if _MSC_VER
    template<StringViewType T>
    static file_flags read_file_flags(T filename) noexcept
    {
        int flags = FF_INVALID;
        DWORD attr = win32_file_attr<T>(filename);
        if (attr != DWORD(-1))
        {
            if (attr & FILE_ATTRIBUTE_DIRECTORY) flags |= FF_FOLDER;
            else                                 flags |= FF_FILE;
            if (attr & FILE_ATTRIBUTE_REPARSE_POINT) flags |= FF_SYMLINK;
        }
        return file_flags(flags);
    }
#else
    template<StringViewType T>
    static file_flags read_file_flags(T filename) noexcept
    {
        int flags = FF_INVALID;
        os_stat64 s;
        if (sys_stat64<T>(filename, &s) == 0/*OK*/)
        {
            if (S_ISDIR(s.st_mode)) flags |= FF_FOLDER;
            else                    flags |= FF_FILE;
            if (S_ISLNK(s.st_mode)) flags |= FF_SYMLINK;
        }
        return file_flags(flags);
    }
#endif

} // namespace rpp
