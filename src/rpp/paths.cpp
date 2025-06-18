#include "paths.h"
#include "file_io.h"
#include "delegate.h"
#include "paths.inl"

namespace rpp /* ReCpp */
{
    ////////////////////////////////////////////////////////////////////////////////

    bool file_exists(strview filename) noexcept
    {
        return (read_file_flags(filename) & FF_FILE) != 0;
    }
    bool is_symlink(strview filename) noexcept
    {
        return (read_file_flags(filename) & FF_SYMLINK) != 0;
    }
    bool folder_exists(strview folder) noexcept
    {
        return (read_file_flags(folder) & FF_FOLDER) != 0;
    }
    bool file_or_folder_exists(strview fileOrFolder) noexcept
    {
        return (read_file_flags(fileOrFolder) & FF_FILE_OR_FOLDER) != 0;
    }

#if RPP_ENABLE_UNICODE
    bool file_exists(ustrview filename) noexcept
    {
        return (read_file_flags(filename) & FF_FILE) != 0;
    }
    bool is_symlink(ustrview filename) noexcept
    {
        return (read_file_flags(filename) & FF_SYMLINK) != 0;
    }
    bool folder_exists(ustrview folder) noexcept
    {
        return (read_file_flags(folder) & FF_FOLDER) != 0;
    }
    bool file_or_folder_exists(ustrview fileOrFolder) noexcept
    {
        return (read_file_flags(fileOrFolder) & FF_FILE_OR_FOLDER) != 0;
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

#if _MSC_VER
    template<StringViewType T>
    static bool sys_symlink(T target, T link) noexcept
    {
        DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
        if (read_file_flags(target) & FF_FOLDER) flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;

        if (wchar_dual_conv dconv { /*str1*/link, /*str2*/target })
            return CreateSymbolicLinkW(dconv.wstr1, dconv.wstr2, flags) == TRUE;
        return false; // conversion failed
    }
#else
    template<StringViewType T>
    static bool sys_symlink(T target, T link) noexcept
    {
        if (multibyte_dual_conv dconv{ /*str1*/target, /*str2*/link })
            return symlink(/*target*/dconv.cstr1, /*link*/dconv.cstr2) == 0;
        return false;
    }
#endif
    bool create_symlink(strview target, strview link) noexcept
    {
        return sys_symlink<strview>(target, link);
    }
#if RPP_ENABLE_UNICODE
    bool create_symlink(ustrview target, ustrview link) noexcept
    {
        return sys_symlink<ustrview>(target, link);
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////
    
    // minimal version of stat64 struct, with low level dev and inode info removed
    struct sys_min_fstats
    {
        int64  size;
        time_t atime;
        time_t mtime;
        time_t ctime;
    };
#if _MSC_VER
    static time_t to_time_t(const FILETIME& ft) noexcept
    {
        ULARGE_INTEGER time; time.LowPart = ft.dwLowDateTime; time.HighPart = ft.dwHighDateTime;
        return time.QuadPart / 10000000ULL - 11644473600ULL;
    }
    // gets filesize, creation time, access time and modification time
    template<StringViewType T>
    static bool sys_minstat(T filename, sys_min_fstats* out) noexcept
    {
        WIN32_FILE_ATTRIBUTE_DATA data;
        wchar_conv conv { filename };
        if (conv.wstr) {
            if (!GetFileAttributesExW(conv.wstr, GetFileExInfoStandard, &data))
                return false;
        } else {
            return false; // conversions failed
		}
        ULARGE_INTEGER li; li.LowPart = data.nFileSizeLow; li.HighPart = data.nFileSizeHigh;
        out->size = (int64)li.QuadPart;
        out->ctime = to_time_t(data.ftCreationTime);
        out->atime = to_time_t(data.ftLastAccessTime);
        out->mtime = to_time_t(data.ftLastWriteTime);
        return true;
    }
#else
    template<StringViewType T>
    static bool sys_minstat(T filename, sys_min_fstats* out) noexcept
    {
        if (multibyte_conv conv { filename }) {
            os_stat64 s;
            if (stat64(conv.cstr, &s) == 0) {
                out->size  = (int64)s.st_size;
                out->ctime = s.st_ctime;
                out->atime = s.st_atime;
                out->mtime = s.st_mtime;
				return true;
            }
        }
        return false;
    }
#endif

    bool file_info(strview filename, int64*  filesize, time_t* created,
                                     time_t* accessed, time_t* modified) noexcept
    {
        sys_min_fstats s;
        if (sys_minstat<strview>(filename, &s)) {
            if (filesize) *filesize = s.size;
            if (created)  *created  = s.ctime;
            if (accessed) *accessed = s.atime;
            if (modified) *modified = s.mtime;
            return true;
        }
        return false;
    }

#if RPP_ENABLE_UNICODE
    bool file_info(ustrview filename, int64*  filesize, time_t* created,
                                      time_t* accessed, time_t* modified) noexcept
    {
        sys_min_fstats s;
        if (sys_minstat<ustrview>(filename, &s)) {
            if (filesize) *filesize = s.size;
            if (created)  *created  = s.ctime;
            if (accessed) *accessed = s.atime;
            if (modified) *modified = s.mtime;
            return true;
        }
        return false;
    }
#endif // RPP_ENABLE_UNICODE

    bool file_info(intptr_t fd, int64*  filesize, time_t* created,
                                time_t* accessed, time_t* modified) noexcept
    {
        if (!fd)
            return false;
    #if _MSC_VER
        FILETIME c, a, m;
        if (GetFileTime((HANDLE)fd, created?&c:nullptr,accessed?&a: nullptr, modified?&m: nullptr)) {
            if (filesize) {
                LARGE_INTEGER li; GetFileSizeEx((HANDLE)fd, &li);
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
        if (_fstat64((int)fd, &s) == 0) {
            if (filesize) *filesize = s.st_size;
            if (created)  *created  = s.st_ctime;
            if (accessed) *accessed = s.st_atime;
            if (modified) *modified = s.st_mtime;
            return true;
        }
        return false;
    #endif
    }

    ////////////////////////////////////////////////////////////////////////////////

    int file_size(strview filename) noexcept
    {
        int64 s; 
        return file_info(filename, &s, nullptr, nullptr, nullptr) ? static_cast<int>(s) : 0;
    }
    int64 file_sizel(strview filename) noexcept
    {
        int64 s; 
        return file_info(filename, &s, nullptr, nullptr, nullptr) ? s : 0ll;
    }
    time_t file_created(strview filename) noexcept
    {
        time_t t; 
        return file_info(filename, nullptr, &t, nullptr, nullptr) ? t : time_t(0);
    }
    time_t file_accessed(strview filename) noexcept
    {
        time_t t; 
        return file_info(filename, nullptr, nullptr, &t, nullptr) ? t : time_t(0);
    }
    time_t file_modified(strview filename) noexcept
    {
        time_t t; 
        return file_info(filename, nullptr, nullptr, nullptr, &t) ? t : time_t(0);
    }

#if RPP_ENABLE_UNICODE
    int file_size(ustrview filename) noexcept
    {
        int64 s;
        return file_info(filename, &s, nullptr, nullptr, nullptr) ? static_cast<int>(s) : 0;
    }
    int64 file_sizel(ustrview filename) noexcept
    {
        int64 s;
        return file_info(filename, &s, nullptr, nullptr, nullptr) ? s : 0ll;
    }
    time_t file_created(ustrview filename) noexcept
    {
        time_t t;
        return file_info(filename, nullptr, &t, nullptr, nullptr) ? t : time_t(0);
    }
    time_t file_accessed(ustrview filename) noexcept
    {
        time_t t;
        return file_info(filename, nullptr, nullptr, &t, nullptr) ? t : time_t(0);
    }
    time_t file_modified(ustrview filename) noexcept
    {
        time_t t;
        return file_info(filename, nullptr, nullptr, nullptr, &t) ? t : time_t(0);
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

    template<StringViewType T>
    static bool sys_delete(T filename) noexcept
    {
    #if _MSC_VER
		if (wchar_conv conv { filename })
            return ::_wremove(conv.wstr) == 0;
        return false;
    #else
        if (multibyte_conv conv { filename })
            return ::remove(conv.cstr) == 0;
        return false;
    #endif
    }
    bool delete_file(strview filename) noexcept
    {
		return sys_delete<strview>(filename);
    }
#if RPP_ENABLE_UNICODE
    bool delete_file(ustrview filename) noexcept
    {
		return sys_delete<ustrview>(filename);
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

	template<StringViewType T>
    static bool sys_copy_file(T sourceFile, T destinationFile) noexcept
    {
    #if _MSC_VER
        if (wchar_dual_conv conv{ /*str1*/sourceFile, /*str2*/destinationFile }) {
            // CopyFileA/W always copies the file access rights
            return CopyFileW(conv.wstr1, conv.wstr2, /*failIfExists:*/false) == TRUE;
        }
        return false; // conversion failed
    #else
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
    #endif
    }

    bool copy_file(strview sourceFile, strview destinationFile) noexcept
    {
		return sys_copy_file<strview>(sourceFile, destinationFile);
    }
#if RPP_ENABLE_UNICODE
    bool copy_file(ustrview sourceFile, ustrview destinationFile) noexcept
    {
        return sys_copy_file<ustrview>(sourceFile, destinationFile);
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

	template<StringViewType T>
    static bool copy_file_mode(T sourceFile, T destinationFile) noexcept
    {
    #if _WIN32
        DWORD attr = win32_file_attr(sourceFile);
        return attr != DWORD(-1) && win32_set_file_attr(destinationFile, attr);
    #else
        os_stat64 s;
		return sys_stat64(sourceFile, &s) && set_st_mode(destinationFile, s.st_mode);
    #endif
    }
    
    bool copy_file_mode(strview sourceFile, strview destinationFile) noexcept
    {
		return copy_file_mode<strview>(sourceFile, destinationFile);
    }
#if RPP_ENABLE_UNICODE
    bool copy_file_mode(ustrview sourceFile, ustrview destinationFile) noexcept
    {
        return copy_file_mode<ustrview>(sourceFile, destinationFile);
    }
#endif

    bool copy_file_if_needed(strview sourceFile, strview destinationFile) noexcept
    {
        if (file_exists(destinationFile))
            return true;
        return copy_file(sourceFile, destinationFile);
    }
#if RPP_ENABLE_UNICODE
    bool copy_file_if_needed(ustrview sourceFile, ustrview destinationFile) noexcept
    {
        if (file_exists(destinationFile))
            return true;
        return copy_file(sourceFile, destinationFile);
    }
#endif

    bool copy_file_into_folder(strview sourceFile, strview destinationFolder) noexcept
    {
        string destFile = path_combine(destinationFolder, file_nameext(sourceFile));
        return copy_file(sourceFile, destFile);
    }
#if RPP_ENABLE_UNICODE
    bool copy_file_into_folder(ustrview sourceFile, ustrview destinationFolder) noexcept
    {
        ustring destFile = path_combine(destinationFolder, file_nameext(sourceFile));
        return copy_file(sourceFile, destFile);
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

    template<typename Char>
    struct chars
    {
        using tchar = Char;
        static constexpr tchar FORESLASH = tchar('/');
        static constexpr tchar BACKSLASH = tchar('\\');
        static constexpr tchar DOT = tchar('.');
        static constexpr tchar NUL = tchar('\0');
    };

    template<StringViewType T>
    struct constants
    {
        using tchar = typename T::char_t;
        static constexpr tchar FORESLASH = tchar('/');
        static constexpr tchar BACKSLASH = tchar('\\');
        static constexpr tchar DOT = tchar('.');
        static constexpr tchar NUL = tchar('\0');

        // slashes: "/\\"
        static constexpr tchar slashes_a[3] = { FORESLASH, BACKSLASH, NUL };
        static constexpr T slashes_sv { slashes_a, 2 };

        // dotslash: "./"
        static constexpr tchar dotslash_a[3] = { DOT, FORESLASH, NUL };
        static constexpr T dotslash_sv { dotslash_a, 2 };

        // dotslashes: "./\\"
        static constexpr tchar dotslashes_a[4] = { DOT, FORESLASH, BACKSLASH, NUL };
        static constexpr T dotslashes_sv { dotslashes_a, 3 };

        // dot: "."
        static constexpr tchar dot_a[3] = { DOT, NUL };
        static constexpr T dot_sv { dot_a, 1 };

        // dotdot: ".."
        static constexpr tchar dotdot_a[3] = { DOT, DOT, NUL };
        static constexpr T dotdot_sv { dotdot_a, 2 };

        // dotdotslash: "../"
        static constexpr tchar dotdotslash_a[4] = { DOT, DOT, FORESLASH, NUL };
        static constexpr T dotdotslash_sv { dotdotslash_a, 3 };
    };

    ////////////////////////////////////////////////////////////////////////////////
        
    // on failure, check errno for details, if folder (or file) exists, we consider it a success
    // @note Might not be desired behaviour for all cases, so use file_exists or folder_exists.
	template<StringViewType T>
    static bool sys_mkdir(T foldername) noexcept
    {
    #if _WIN32
        if (wchar_conv conv{ foldername })
        {
            BOOL result = CreateDirectoryW(conv.wstr, nullptr); // -> ERROR_PATH_NOT_FOUND or ERROR_ALREADY_EXISTS
            return result == TRUE || GetLastError() == ERROR_ALREADY_EXISTS;
        }
		return false; // conversion failed
    #else
        if (multibyte_conv conv { foldername })
            return mkdir(conv.cstr, 0755) == 0 || errno == EEXIST;
        return false;
    #endif
    }

    template<StringViewType T>
    static bool create_folder(T foldername) noexcept
    {
        using tchar = typename T::char_t;
        if (!foldername.len) // fail on empty strings purely for catching bugs
            return false;

        // foldername == "./", current folder already exist
        if (foldername == constants<T>::dotslash_sv)
            return true;

        if (sys_mkdir(foldername)) // best case, no recursive mkdir required
            return true;

        // ugh, need to work our way upward to find a root dir that exists:
        // @note heavily optimized to minimize folder_exists() and mkdir() syscalls
        const tchar* fs = foldername.begin();
        const tchar* fe = foldername.end();
        const tchar* p = fe;
        while ((p = T{ fs,p }.rfindany(constants<T>::slashes_a, 2)) != nullptr)
        {
            if (folder_exists(T{ fs,p }))
                break;
        }

        // now create all the parent dir between:
        p = p ? p + 1 : fs; // handle /dir/ vs dir/ case

        while (const tchar* e = T{ p,fe }.findany(constants<T>::slashes_a, 2))
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
#if RPP_ENABLE_UNICODE
    bool create_folder(ustrview foldername) noexcept
    {
        return create_folder<ustrview>(foldername);
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

	template<StringViewType T>
    static bool sys_rmdir(T foldername) noexcept
    {
    #if _WIN32
        if (wchar_conv conv { foldername })
            return _wrmdir(conv.wstr) == 0;
        return false;
    #else
		if (multibyte_conv conv { foldername })
            return rmdir(conv.cstr) == 0;
		return false; // conversion failed
    #endif
    }

    template<StringViewType T>
    static bool delete_folder(T foldername, delete_mode mode) noexcept
    {
        using tstring = typename T::string_t;
		using tchar = typename T::char_t;

        // these would delete the root dir...
        if (foldername.empty())
            return false;
        if (foldername.len == 1 && foldername[0] == tchar('/')) //  or if foldername == "/"_sv
            return false;
        if (mode == delete_mode::non_recursive)
            return sys_rmdir(foldername); // easy path, just gently try to delete...

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
#if RPP_ENABLE_UNICODE
    bool delete_folder(ustrview foldername, delete_mode mode) noexcept
    {
        return delete_folder<ustrview>(foldername, mode);
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

    string full_path(strview path) noexcept
    {
    #if _WIN32
        if (wchar_conv conv { path }) {
            conv_buffer out;
            if (DWORD len = GetFullPathNameW(conv.wstr, out.MAX_U, out.path_w, nullptr)) {
                if (len >= out.MAX_U) { len = out.MAX_U; out.path_w[len - 1] = L'\0'; }
                normalize((char16_t*)out.path_w, u'/'); // normalize windows paths to forward slashes
                return to_string(out.path_u, len);
            }
        }
        return string{};
    #else
        if (multibyte_conv conv { path }) {
            conv_buffer out;
            if (char* res = realpath(conv.cstr, out.path_a))
				return string{ res };
        }
		return string{};
    #endif
    }
#if RPP_ENABLE_UNICODE
    ustring full_path(ustrview path) noexcept
    {
    #if _WIN32
        if (wchar_conv conv{ path }) {
            conv_buffer out;
            if (DWORD len = GetFullPathNameW(conv.wstr, out.MAX_U, out.path_w, nullptr)) {
                if (len >= out.MAX_U) { len = out.MAX_U; out.path_w[len-1] = L'\0'; }
                normalize(out.path_u, u'/'); // normalize windows paths to forward slashes
                return ustring{ out.path_u, out.path_u + len };
            }
        }
        return ustring{};
    #else
        if (multibyte_conv conv { path }) {
            conv_buffer out;
            if (char* res = realpath(conv.cstr, out.path_a))
				return to_ustring(res);
        }
		return {};
    #endif
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

    template<StringViewType T>
    static auto merge_dirups(T path) noexcept -> typename T::string_t
    {
        using tchar = typename T::char_t;
        T pathstr = path;
        const bool isDirPath = path.back() == tchar('/') || path.back() == tchar('\\');
        std::vector<T> folders;
        while (T folder = pathstr.next(constants<T>::slashes_a)) {
            folders.push_back(folder);
        }

        for (size_t i = 0; i < folders.size(); ++i)
        {
            if (i > 0 && folders[i] == constants<T>::dotdot_sv
                      && folders[i-1] != constants<T>::dotdot_sv)
            {
                auto it = folders.begin() + i;
                folders.erase(it - 1, it + 1);
                i -= 2;
            }
        }

        typename T::string_t result;
        for (const T& folder : folders) {
            result += folder;
            result += tchar('/');
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
#if RPP_ENABLE_UNICODE
    ustring merge_dirups(ustrview path) noexcept
    {
        return merge_dirups<ustrview>(path);
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

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
#if RPP_ENABLE_UNICODE
    ustrview file_name(ustrview path) noexcept
    {
        return file_name<ustrview>(path);
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

    template<StringViewType T>
    static auto file_nameext(T path) noexcept -> T
    {
        if (auto* str = path.rfindany(constants<T>::slashes_a))
            return T{ str + 1, path.end() };
        return path; // assume it's just a file name
    }
    strview file_nameext(strview path) noexcept
    {
        return file_nameext<strview>(path);
    }
#if RPP_ENABLE_UNICODE
    ustrview file_nameext(ustrview path) noexcept
    {
        return file_nameext<ustrview>(path);
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

    template<StringViewType T>
    static auto file_ext(T path) noexcept -> T
    {
        if (auto* ptr = path.substr(path.len - EXT_LEN_MAX).rfindany(constants<T>::dotslashes_a)) {
            if (*ptr == constants<T>::DOT)
                return T{ ptr + 1, path.end() };
        }
        return T{}; // no extension found
    }
    strview file_ext(strview path) noexcept
    {
        return file_ext<strview>(path);
    }
#if RPP_ENABLE_UNICODE
    ustrview file_ext(ustrview path) noexcept
    {
        return file_ext<ustrview>(path);
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

    template<StringViewType T>
    static auto file_replace_ext(T path, T ext) noexcept -> typename T::string_t
    {
        if (T oldext = file_ext(path))
        {
            int len = int(oldext.str - path.str);
            return concat(T{ path.str, len }, ext);
        }
        if (path && path.back() != constants<T>::FORESLASH
                 && path.back() != constants<T>::BACKSLASH)
        {
            return concat(path, constants<T>::dot_sv, ext);
        }
        return path;
    }
    string file_replace_ext(strview path, strview ext) noexcept
    {
        return file_replace_ext<strview>(path, ext);
    }
#if RPP_ENABLE_UNICODE
    ustring file_replace_ext(ustrview path, ustrview ext) noexcept
    {
        return file_replace_ext<ustrview>(path, ext);
    }
#endif
    
    ////////////////////////////////////////////////////////////////////////////////

    template<StringViewType T>
    static auto file_name_append(T path, T add) noexcept -> typename T::string_t
    {
        typename T::string_t result = concat(folder_path(path), file_name(path), add);
        if (T ext = file_ext(path)) {
            result += constants<T>::DOT;
            result += ext;
        }
        return result;
    }
    string file_name_append(strview path, strview add) noexcept
    {
        return file_name_append<strview>(path, add);
    }
#if RPP_ENABLE_UNICODE
    ustring file_name_append(ustrview path, ustrview add) noexcept
    {
        return file_name_append<ustrview>(path, add);
    }
#endif
    
    ////////////////////////////////////////////////////////////////////////////////

    template<StringViewType T>
    static auto file_name_replace(T path, T newFileName) noexcept -> typename T::string_t
    {
        typename T::string_t result = concat(folder_path(path), newFileName);
        if (T ext = file_ext(path)) {
            result += constants<T>::DOT;
            result += ext;
        }
        return result;
    }
    string file_name_replace(strview path, strview newFileName) noexcept
    {
        return file_name_replace<strview>(path, newFileName);
    }
#if RPP_ENABLE_UNICODE
    ustring file_name_replace(ustrview path, ustrview newFileName) noexcept
    {
        return file_name_replace<ustrview>(path, newFileName);
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////
    
    string file_nameext_replace(strview path, strview newFileNameAndExt) noexcept
    {
        return concat(folder_path(path), newFileNameAndExt);
    }
#if RPP_ENABLE_UNICODE
    ustring file_nameext_replace(ustrview path, ustrview newFileNameAndExt) noexcept
    {
        return concat(folder_path(path), newFileNameAndExt);
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

    template<StringViewType T>
    static T folder_name(T path) noexcept
    {
        T folder = folder_path(path);
        if (folder)
        {
            if (const auto* str = folder.chomp_last().rfindany(constants<T>::slashes_a))
                return T{ str + 1, folder.end() };
        }
        return folder;
    }
    strview folder_name(strview path) noexcept
    {
        return folder_name<strview>(path);
    }
#if RPP_ENABLE_UNICODE
    ustrview folder_name(ustrview path) noexcept
    {
        return folder_name<ustrview>(path);
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

    template<StringViewType T>
    static T folder_path(T path) noexcept
    {
        if (const auto* end = path.rfindany(constants<T>::slashes_a))
            return T{ path.str, end + 1 };
        return T{}; // no folder path found, return empty
    }
    strview folder_path(strview path) noexcept
    {
        return folder_path<strview>(path);
    }
#if RPP_ENABLE_UNICODE
    ustrview folder_path(ustrview path) noexcept
    {
        return folder_path<ustrview>(path);
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

    template<typename Char>
    static void normalize_nullterm(Char* path, Char sep) noexcept
    {
        if (sep == chars<Char>::FORESLASH) {
            for (Char* s = path; *s; ++s)
                if (*s == chars<Char>::BACKSLASH)
                    *s = chars<Char>::FORESLASH;
        } else if (sep == chars<Char>::BACKSLASH) {
            for (Char* s = path; *s; ++s)
                if (*s == chars<Char>::FORESLASH)
                    *s = chars<Char>::BACKSLASH;
        }
        // else: ignore any other separators
    }
    string& normalize(string& path, char sep) noexcept
    {
        normalize_nullterm(path.data(), sep);
        return path;
    }
    char* normalize(char* path, char sep) noexcept
    {
        normalize_nullterm(path, sep);
        return path;
    }
    string normalized(strview path, char sep) noexcept
    {
        string res = path.to_string();
        normalize_nullterm(res.data(), sep);
        return res;
    }
#if RPP_ENABLE_UNICODE
    ustring& normalize(ustring& path, char16_t sep) noexcept
    {
        normalize_nullterm(path.data(), sep);
        return path;
    }
    char16_t* normalize(char16_t* path, char16_t sep) noexcept
    {
        normalize_nullterm(path, sep);
        return path;
    }
    ustring normalized(ustrview path, char16_t sep) noexcept
    {
        ustring res = path.to_string();
        normalize_nullterm(res.data(), sep);
        return res;
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

    template<StringViewType T, size_t N>
    static auto slash_combine(const std::array<T, N>& args) -> typename T::string_t
    {
        size_t res = args[0].size();
        for (size_t i = 1; i < N; ++i) {
            if (auto n = static_cast<size_t>(args[i].size())) {
                if (res != 0)
                    res += 1;
                res += n;
            }
        }
        
        typename T::string_t result; result.reserve(res);
        result.append(args[0].data(), args[0].size());

        for (size_t i = 1; i < N; ++i) {
            if (auto n = static_cast<size_t>(args[i].size())) {
                if (!result.empty())
                    result.append(1, constants<T>::FORESLASH);
                result.append(args[i].data(), n);
            }
        }
        return result;
    }

    string path_combine(strview path1, strview path2) noexcept
    {
        path1.trim_end(constants<strview>::slashes_a);
        path2.trim(constants<strview>::slashes_a);
        return slash_combine(std::array<strview, 2>{{path1, path2}});
    }
    string path_combine(strview path1, strview path2, strview path3) noexcept
    {
        path1.trim_end(constants<strview>::slashes_a);
        path2.trim(constants<strview>::slashes_a);
        path3.trim(constants<strview>::slashes_a);
        return slash_combine(std::array<strview, 3>{{path1, path2, path3}});
    }
    string path_combine(strview path1, strview path2, strview path3, strview path4) noexcept
    {
        path1.trim_end(constants<strview>::slashes_a);
        path2.trim(constants<strview>::slashes_a);
        path3.trim(constants<strview>::slashes_a);
        path4.trim(constants<strview>::slashes_a);
        return slash_combine(std::array<strview, 4>{{path1, path2, path3, path4}});
    }

#if RPP_ENABLE_UNICODE
    ustring path_combine(ustrview path1, ustrview path2) noexcept
    {
        path1.trim_end(constants<ustrview>::slashes_a);
        path2.trim(constants<ustrview>::slashes_a);
        return slash_combine(std::array<ustrview, 2>{{ path1, path2 }});
    }
    ustring path_combine(ustrview path1, ustrview path2, ustrview path3) noexcept
    {
        path1.trim_end(constants<ustrview>::slashes_a);
        path2.trim(constants<ustrview>::slashes_a);
        path3.trim(constants<ustrview>::slashes_a);
        return slash_combine(std::array<ustrview, 3>{{ path1, path2, path3 }});
    }
    ustring path_combine(ustrview path1, ustrview path2, ustrview path3, ustrview path4) noexcept
    {
        path1.trim_end(constants<ustrview>::slashes_a);
        path2.trim(constants<ustrview>::slashes_a);
        path3.trim(constants<ustrview>::slashes_a);
        path4.trim(constants<ustrview>::slashes_a);
        return slash_combine(std::array<ustrview, 4>{{ path1, path2, path3, path4 }});
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

    template<typename Char>
    static bool should_skip_dir_entry(const Char* filename) noexcept
    {
        // skip "." and ".."
        return (filename[0] == Char('.') && filename[1] == Char('\0'))
            || (filename[0] == Char('.') && filename[1] == Char('.') && filename[2] == Char('\0'));
    }

    struct dir_iter_base::impl
    {
    #if _WIN32
        HANDLE hFind = nullptr;
        WIN32_FIND_DATAW ffd; // always uses wchar
    #else
        DIR* d = nullptr;
        dirent* e = nullptr;
        string dirname;
    #endif
        template<StringViewType T>
        void open(T dir) noexcept
        {
        #if _MSC_VER
            auto find_first_file = [](WIN32_FIND_DATAW* ffd, const wchar_t* path) {
                HANDLE h = FindFirstFileW(path, ffd);
				return h == INVALID_HANDLE_VALUE ? nullptr : h;
			};
            if (dir.empty()) { // handle dir=="" special case
				hFind = find_first_file(&ffd, L"./*");
            } else {
                // only support wstr to simplify API, because internally windows uses WCHAR anyway
                conv_buffer buf;
                if (const wchar_t* dir_w = buf.to_wstr(dir)) {
                    wchar_t path[2048];
                    _snwprintf(path, _countof(path), L"%ls/*", dir_w);
                    hFind = find_first_file(&ffd, path);
                }
            }
            if (hFind && should_skip_dir_entry(ffd.cFileName)) {
                next();
            }
        #else
            if (dir.empty()) { // handle dir=="" special case
                d = opendir(".");
                dirname = "";
            } else if (multibyte_conv conv { dir }) {
                d = opendir(conv.cstr);
                if (d != nullptr) dirname = conv.cstr;
            }
            e = d != nullptr ? readdir(d) : nullptr;
            if (e && should_skip_dir_entry(e->d_name)) {
                next();
            }
        #endif
        }
        void close() noexcept
        {
        #if _MSC_VER
            if (hFind) { FindClose(hFind); hFind = nullptr; }
        #else
            if (d) { closedir(d); d = nullptr; e = nullptr; }
        #endif
        }
        bool next() noexcept
        {
        #if _WIN32
            if (hFind) {
                while (FindNextFileW(hFind, &ffd) == TRUE) {
                    if (should_skip_dir_entry(ffd.cFileName))
                        continue;
                    return true;
                }
            }
            ffd.cFileName[0] = L'\0';
            ffd.dwFileAttributes = 0;
        #else
            if (d) {
                while ((e = readdir(d)) != nullptr) {
                    if (should_skip_dir_entry(e->d_name))
                        continue;
                    return true;
                }
            }
            e = nullptr;
        #endif
            return false;
        }
    };

    FINLINE dir_iter_base::impl* dir_iter_base::state::operator->() noexcept
    {
    #if _WIN32
        static_assert(sizeof(ffd) == sizeof(impl::ffd), "dir_iterator::dummy size mismatch");
    #endif
        return reinterpret_cast<impl*>(this);
    }
    FINLINE const dir_iter_base::impl* dir_iter_base::state::operator->() const noexcept
    {
        return reinterpret_cast<const impl*>(this);
    }

    dir_iter_base::dir_iter_base(strview dir) noexcept
    {
        s->open(dir);
    }
#if RPP_ENABLE_UNICODE
    dir_iter_base::dir_iter_base(ustrview dir) noexcept
    {
        s->open(dir);
    }
#endif
    dir_iter_base::~dir_iter_base() noexcept
    {
        s->close();
    }
    template<StringViewType T>
    auto dir_iter_base::name_util<T>::get(const dir_iter_base* it) noexcept -> typename T::string_t
    {
        if (!it || !*it)
            return {};
        if constexpr (std::is_same_v<T, rpp::strview>)
        {
            #if _MSC_VER
                return rpp::to_string((const char16_t*)it->s->ffd.cFileName);
            #else
                return string{ it->s->e->d_name };
            #endif
    #if RPP_ENABLE_UNICODE
        }
        else if constexpr (std::is_same_v<T, rpp::ustrview>)
        {
            #if _MSC_VER
                return ustring{ (const char16_t*)it->s->ffd.cFileName };
            #else
                return rpp::to_ustring(it->s->e->d_name);
            #endif
    #endif // RPP_ENABLE_UNICODE
        }
        return {};
    }
    bool dir_iter_base::next() noexcept { return s->next(); }

#if _WIN32
    dir_iter_base::operator bool()   const noexcept { return s->hFind && *s->ffd.cFileName; }
    bool dir_iter_base::is_dir()     const noexcept { return s->hFind && *s->ffd.cFileName && (s->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
    bool dir_iter_base::is_file()    const noexcept { return s->hFind && *s->ffd.cFileName && (s->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0; }
    bool dir_iter_base::is_symlink() const noexcept { return s->hFind && *s->ffd.cFileName && (s->ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0; }
    bool dir_iter_base::is_device()  const noexcept { return s->hFind && *s->ffd.cFileName && (s->ffd.dwFileAttributes & FILE_ATTRIBUTE_DEVICE) != 0; }
#else
    dir_iter_base::operator bool() const noexcept { return s->d && s->e; }
    bool dir_iter_base::is_dir() const noexcept
    {
        if (dirent* e = s->e) {
            if (e->d_type) return e->d_type == DT_DIR;
            struct stat st;
            return ::stat(path_combine(s->dirname, e->d_name).c_str(), &st) == 0 && S_ISDIR(st.st_mode);
        }
        return false;
    }
    bool dir_iter_base::is_file() const noexcept
    {
        if (dirent* e = s->e) {
            if (e->d_type) return e->d_type == DT_REG;
            struct stat st;
            return ::stat(path_combine(s->dirname, e->d_name).c_str(), &st) == 0 && S_ISREG(st.st_mode);
        }
        return false;
    }
    bool dir_iter_base::is_symlink() const noexcept
    {
        if (dirent* e = s->e) {
            if (e->d_type) return e->d_type == DT_LNK;
            struct stat st;
            return ::stat(path_combine(s->dirname, e->d_name).c_str(), &st) == 0 && S_ISLNK(st.st_mode);
        }
        return false;
    }
    bool dir_iter_base::is_device() const noexcept
    {
        if (dirent* e = s->e) {
            if (e->d_type) {
                int dt = e->d_type;
                return dt == DT_BLK || dt == DT_CHR || dt == DT_FIFO || dt == DT_SOCK;
            }
            struct stat st;
            if (::stat(path_combine(s->dirname, e->d_name).c_str(), &st) == 0) {
                auto m = st.st_mode;
                return S_ISBLK(m) || S_ISCHR(m) || S_ISFIFO(m) || S_ISSOCK(m);
            }
        }
        return false;
    }
#endif

    ////////////////////////////////////////////////////////////////////////////////

    // @param queryRoot The original path passed to the query.
    //                  For abs listing, this must be an absolute path value!
    //                  For rel listing, this is only used for opening the directory
    // @param relPath Relative path from search root, ex: "src", "src/session/util", etc.
    template<StringViewType T, typename DirTraverseFunc>
    static NOINLINE void traverse_dir(T queryRoot, T relPath, bool dirs, bool files,
                                      list_dir_flags flags, const DirTraverseFunc& func) noexcept
    {
        typename T::string_t currentDir;
        if (flags & dir_fullpath)
        {
            typename T::string_t fullpath = full_path(queryRoot.empty() ? constants<T>::dot_sv : queryRoot);
            if (fullpath.empty()) // dir does not exist
                return;
            currentDir = path_combine(fullpath, relPath);
        }
        else
        {
            currentDir = path_combine(queryRoot, relPath);
        }

        for (directory_entry<T> e : directory_iter<T>{ currentDir })
        {
            if (e.is_dir())
            {
                if (dirs)
                {
                    T dir = (flags & dir_fullpath) ? T{currentDir} : relPath;
                    func(path_combine(dir, e.name), /*isDir*/true);
                }
                if (flags & dir_recursive)
                {
                    traverse_dir<T>(queryRoot, /*relPath*/path_combine(relPath, e.name), dirs, files, flags, func);
                }
            }
            else // file, symlink or device
            {
                if (files)
                {
                    T dir = (flags & dir_fullpath) ? T{currentDir} : relPath;
                    func(path_combine(dir, e.name), /*isDir*/false);
                }
            }
        }
    }

    ////////////////////////////////////////////////////////////////////////////////

    template<StringViewType T>
    static int list_dirs(std::vector<typename T::string_t>& out, T dir, list_dir_flags flags) noexcept
    {
        traverse_dir<T>(dir, T{}, true, false, flags, [&](typename T::string_t&& path, bool) {
            if (flags & dir_relpath_combine) {
                out.emplace_back(path_combine(dir, path));
            } else {
                out.emplace_back(std::move(path));
            }
        });
        return static_cast<int>(out.size());
    }
    int list_dirs(string_list& out, strview dir, list_dir_flags flags) noexcept
    {
        return list_dirs<strview>(out, dir, flags);
    }
#if RPP_ENABLE_UNICODE
    int list_dirs(ustring_list& out, ustrview dir, list_dir_flags flags) noexcept
    {
        return list_dirs<ustrview>(out, dir, flags);
    }
#endif


    template<StringViewType T>
    static int list_files(std::vector<typename T::string_t>& out, T dir, T suffix, list_dir_flags flags) noexcept
    {
        traverse_dir<T>(dir, T{}, false, true, flags, [&](typename T::string_t&& path, bool) {
            if (suffix.empty() || T{path}.ends_withi(suffix)) {
                if (flags & dir_relpath_combine) {
                    out.emplace_back(path_combine(dir, path));
                } else {
                    out.emplace_back(std::move(path));
                }
            }
        });
        return static_cast<int>(out.size());
    }
    int list_files(string_list& out, strview dir, strview suffix, list_dir_flags flags) noexcept
    {
        return list_files<strview>(out, dir, suffix, flags);
    }
#if RPP_ENABLE_UNICODE
    int list_files(ustring_list& out, ustrview dir, ustrview suffix, list_dir_flags flags) noexcept
    {
        return list_files<ustrview>(out, dir, suffix, flags);
    }
#endif


    template<StringViewType T>
    static int list_files(std::vector<typename T::string_t>& out, T dir,
                          const std::vector<T>& suffixes, list_dir_flags flags) noexcept
    {
        traverse_dir<T>(dir, T{}, false, true, flags, [&](typename T::string_t&& path, bool) {
            for (const T& suffix : suffixes) {
                if (T{path}.ends_withi(suffix)) {
                    out.emplace_back(std::move(path));
                    return;
                }
            }
        });
        return static_cast<int>(out.size());
    }
    int list_files(string_list& out, strview dir, const std::vector<strview>& suffixes, list_dir_flags flags) noexcept
    {
        return list_files<strview>(out, dir, suffixes, flags);
    }
#if RPP_ENABLE_UNICODE
    int list_files(ustring_list& out, ustrview dir, const std::vector<ustrview>& suffixes, list_dir_flags flags) noexcept
    {
        return list_files<ustrview>(out, dir, suffixes, flags);
    }
#endif


    template<StringViewType T>
    static int list_alldir(std::vector<typename T::string_t>& outDirs,
                           std::vector<typename T::string_t>& outFiles,
                           T dir, list_dir_flags flags) noexcept
    {
        traverse_dir<T>(dir, T{}, true, true, flags, [&](typename T::string_t&& path, bool isDir) {
            auto& out = isDir ? outDirs : outFiles;
            out.emplace_back(std::move(path));
        });
        return static_cast<int>(outDirs.size() + outFiles.size());
    }
    int list_alldir(string_list& outDirs, string_list& outFiles, strview dir, list_dir_flags flags) noexcept
    {
        return list_alldir<strview>(outDirs, outFiles, dir, flags);
    }
#if RPP_ENABLE_UNICODE
    int list_alldir(ustring_list& outDirs, ustring_list& outFiles, ustrview dir, list_dir_flags flags) noexcept
    {
        return list_alldir<ustrview>(outDirs, outFiles, dir, flags);
    }
#endif


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
            int len = (int)GetModuleFileNameA(reinterpret_cast<HMODULE>(moduleObject), path, sizeof(path));
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
            int len = (int)GetModuleFileNameA(reinterpret_cast<HMODULE>(moduleObject), path, sizeof(path));
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

    template<StringViewType T>
    static bool change_dir(T new_wd) noexcept
    {
        #if _WIN32
            if (wchar_conv conv { new_wd }) {
                return _wchdir(conv.wstr) == 0;
            }
            return false; // conversion failed
        #else
            if (multibyte_conv conv { new_wd }) {
                return chdir(conv.cstr) == 0;
            }
            return false;
        #endif
    }
    bool change_dir(strview new_wd) noexcept
    {
        return change_dir<strview>(new_wd);
    }
#if RPP_ENABLE_UNICODE
    bool change_dir(ustrview new_wd) noexcept
    {
        return change_dir<ustrview>(new_wd);
    }
#endif

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
            char path[1024];
            getenv_s(&len, path, _countof(path), "USERPROFILE");
            if (len == 0)
                return {};
            int slen = static_cast<int>(len) - 1;
            win32_fixup_path(path, slen);
            return { path, path + slen };
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

#if RPP_ENABLE_UNICODE
    ustring home_diru() noexcept
    {
        #if _MSC_VER
            size_t len = 0;
            wchar_t path[1024];
            _wgetenv_s(&len, path, _countof(path), L"USERPROFILE");
            if (len == 0)
                return {};
            int slen = static_cast<int>(len) - 1;
            win32_fixup_path((char16_t*)path, slen);
            return { (char16_t*)path, (char16_t*)path + slen };
        #else
            return to_ustring(home_dir());
        #endif
    }
#endif // RPP_ENABLE_UNICODE
    
    ////////////////////////////////////////////////////////////////////////////////
} // namespace rpp
