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
    #define USE_WINAPI_IO 0 // WINAPI IO is much faster on windows because msvcrt has lots of overhead
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
            char16_t path_u16[MAX_U16];
            char path_u8[MAX_U8];
        #if _MSC_VER
            wchar_t path_w[MAX_U16];
        #endif
        };

    #if _MSC_VER
        FINLINE const wchar_t* to_wcs(strview path) noexcept
        {
            // UTF8 --> UTF16, and then treat as UCS2
            if (to_ustring(path_u16, MAX_U16, path.str, path.len) > 0)
                return (const wchar_t*)path_u16;
            // else: cannot be converted due to invalid UTF8 sequence
            return nullptr;
        }
        FINLINE const wchar_t* to_wcs(ustrview path) noexcept
        {
            return (const wchar_t*)path.to_cstr(path_u16, MAX_U16);
        }
    #endif
        FINLINE const char* to_cstr(strview path) noexcept
        {
            return path.to_cstr(path_u8, MAX_U8);
        }
        const char* to_cstr(ustrview path) noexcept
        {
            // UTF16 --> UTF8
            if (to_string(path_u8, MAX_U8, path.str, path.len) > 0)
                return path_u8;
            // contains invalid UTF16 sequence, there is no point to continue
            return nullptr;
        }
    };

#if _MSC_VER
    #if USE_WINAPI_IO
    template<StringViewType T>
    static DWORD win32_file_attributes(T filename) noexcept
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
        DWORD attr = win32_file_attributes(filename);
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
        if (win32_stat64(filename, &s)) {
    #else
        if (get_stat64(filename, &s)) {
    #endif
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
        if (win32_stat64(filename, &s)) {
    #else
        if (get_stat64(filename, &s)) {
    #endif
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

    bool copy_file(const char* sourceFile, const char* destinationFile) noexcept
    {
        #if USE_WINAPI_IO
            // CopyFileA always copies the file access rights
            return CopyFileA(sourceFile, destinationFile, /*failIfExists:*/false) == TRUE;
        #else
            file src{sourceFile, file::READONLY};
            if (!src) return false;

            file dst{destinationFile, file::CREATENEW};
            if (!dst) return false;

            // copy the file access rights
            if (!copy_file_mode(sourceFile, destinationFile))
                return false;

            // special case: empty file
            int64 size = src.sizel();
            if (size == 0) return true;

            constexpr int64 blockSize = 64*1024;
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
        #endif
    }

    bool copy_file(const strview sourceFile, const strview destinationFile) noexcept
    {
        char buf1[1024];
        char buf2[1024];
        return copy_file(sourceFile.to_cstr(buf1), destinationFile.to_cstr(buf2));
    }

    bool copy_file_mode(const char* sourceFile, const char* destinationFile) noexcept
    {
        #if _WIN32
            DWORD attr = GetFileAttributesA(sourceFile);
            return attr != DWORD(-1) && SetFileAttributesA(destinationFile, attr) != FALSE;
        #else
            struct stat s;
            return stat(sourceFile, &s) != 0 ? false : chmod(destinationFile, s.st_mode) == 0;
        #endif
    }

    bool copy_file_mode(const strview sourceFile, const strview destinationFile) noexcept
    {
        char buf1[1024];
        char buf2[1024];
        return copy_file_mode(sourceFile.to_cstr(buf1), destinationFile.to_cstr(buf2));
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
        string destFile = path_combine(destinationFolder, file_nameext(sourceFile));
        return copy_file(sourceFile.to_cstr(buf1), destFile.c_str());
    }
    
    // on failure, check errno for details, if folder (or file) exists, we consider it a success
    // @note Might not be desired behaviour for all cases, so use file_exists or folder_exists.
    static bool sys_mkdir(strview foldername) noexcept
    {
        char buf[1024];
    #if _WIN32
        return _mkdir(foldername.to_cstr(buf)) == 0 || errno == EEXIST;
    #else
        return mkdir(foldername.to_cstr(buf), 0755) == 0 || errno == EEXIST;
    #endif
    }
    static bool sys_mkdir(ustrview foldername) noexcept
    {
    #if _WIN32
        char16_t buf[1024];
        return _wmkdir((const wchar_t*)foldername.to_cstr(buf)) == 0 || errno == EEXIST;
    #else
        char buf[1024];
        int n = to_string(buf, )
        return sys_mkdir(to_string(foldername));
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
    bool create_folder(const ustring& foldername) noexcept
    {
        if (foldername.empty() || foldername == L"./")
            return false;
        return sys_mkdir(foldername.c_str());
    }

    static bool sys_rmdir(strview foldername) noexcept
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

        std::vector<string> folders;
        std::vector<string> files;
        bool deletedChildren = true;

        if (list_alldir(folders, files, foldername))
        {
            for (const string& folder : folders)
                deletedChildren |= delete_folder(path_combine(foldername, folder), delete_mode::recursive);

            for (const string& file : files)
                deletedChildren |= delete_file(path_combine(foldername, file));
        }

        if (deletedChildren)
            return sys_rmdir(foldername); // should be okay to remove now

        return false; // no way to delete, since some subtree files are protected
    }

#if RPP_ENABLE_UNICODE
    static bool sys_rmdir(ustrview foldername) noexcept
    {
    #if _WIN32
        wchar_t buf[512];
        return _wrmdir(foldername.to_cstr(buf)) == 0;
    #else
        // convert to UTF8 and call the other function
        return sys_rmdir(foldername.to_string());
    #endif
    }
    bool delete_folder(ustrview foldername, delete_mode mode) noexcept
    {
        if (foldername.empty() || foldername == L"/"_sv)
            return false;
        if (mode == delete_mode::non_recursive)
            return sys_rmdir(foldername);

        std::vector<std::wstring> folders;
        std::vector<std::wstring> files;
        bool deletedChildren = true;

        if (list_alldir(folders, files, foldername.to_string()))
        {
            for (const std::wstring& folder : folders)
                deletedChildren |= delete_folder(path_combine(foldername, folder), delete_mode::recursive);
            for (const std::wstring& file : files)
                deletedChildren |= delete_file(path_combine(foldername, file));
        }
        if (deletedChildren)
            return sys_rmdir(foldername);
        return false; // no way to delete, since some subtree files are protected
    }

#endif // RPP_ENABLE_UNICODE

    string full_path(const char* path) noexcept
    {
        char buf[4096];
        #if _WIN32
            size_t len = GetFullPathNameA(path, sizeof(buf), buf, nullptr);
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
        std::vector<strview> folders;
        while (strview folder = pathstr.next("/\\")) {
            folders.push_back(folder);
        }

        for (size_t i = 0; i < folders.size(); ++i)
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


    string file_replace_ext(strview path, strview ext) noexcept
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
    
    string file_name_append(strview path, strview add) noexcept
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
    
    string file_name_replace(strview path, strview newFileName) noexcept
    {
        string result = folder_path(path) + newFileName;
        if (strview ext = file_ext(path)) {
            result += '.';
            result += ext;
        }
        return result;
    }
    
    string file_nameext_replace(strview path, strview newFileNameAndExt) noexcept
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
    ustring folder_path(const wchar_t* path) noexcept
    {
        auto* end = path + wcslen(path);
        for (; path < end; --end)
            if (*end == '/' || *end == '\\')
                break;
        return path == end ? ustring{} : ustring{path, end + 1};
    }
    ustring folder_path(const ustring& path) noexcept
    {
        auto* ptr = path.c_str();
        auto* end  = ptr + path.size();
        for (; ptr < end; --end)
            if (*end == '/' || *end == '\\')
                break;
        return ptr == end ? ustring{} : ustring{ptr, end + 1};
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

    template<class TStrView = rpp::strview, size_t N>
    static TStrView::string_t slash_combine(const std::array<TStrView, N>& args)
    {
        size_t res = args[0].size();
        for (size_t i = 1; i < N; ++i) {
            if (auto n = static_cast<size_t>(args[i].size())) {
                if (res != 0)
                    res += 1;
                res += n;
            }
        }
        
        TStrView::string_t result; result.reserve(res);
        result.append(args[0].data(), args[0].size());

        for (size_t i = 1; i < N; ++i) {
            if (auto n = static_cast<size_t>(args[i].size())) {
                if (!result.empty())
                    result.append(1, '/');
                result.append(args[i].data(), n);
            }
        }
        return result;
    }

    string path_combine(strview path1, strview path2) noexcept
    {
        path1.trim_end("/\\");
        path2.trim("/\\");
        return slash_combine(std::array<strview, 2>{{path1, path2}});
    }

    string path_combine(strview path1, strview path2, strview path3) noexcept
    {
        path1.trim_end("/\\");
        path2.trim("/\\");
        path3.trim("/\\");
        return slash_combine(std::array<strview, 3>{{path1, path2, path3}});
    }

    string path_combine(strview path1, strview path2, strview path3, strview path4) noexcept
    {
        path1.trim_end("/\\");
        path2.trim("/\\");
        path3.trim("/\\");
        path4.trim("/\\");
        return slash_combine(std::array<strview, 4>{{path1, path2, path3, path4}});
    }

    ustring path_combine(ustrview path1, ustrview path2) noexcept
    {
        path1.trim_end(u"/\\");
        path2.trim(u"/\\");
        return slash_combine(std::array<ustrview, 2>{{ path1, path2 }});
    }

    ustring path_combine(ustrview path1, ustrview path2, ustrview path3) noexcept
    {
        path1.trim_end(u"/\\");
        path2.trim(u"/\\");
        path3.trim(u"/\\");
        return slash_combine(std::array<ustrview, 3>{{ path1, path2, path3 }});
    }

    ustring path_combine(ustrview path1, ustrview path2, ustrview path3, ustrview path4) noexcept
    {
        path1.trim_end(u"/\\");
        path2.trim(u"/\\");
        path3.trim(u"/\\");
        path4.trim(u"/\\");
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
