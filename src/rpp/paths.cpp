#include "paths.h"
#include "file_io.h"
#include "delegate.h"
#include "strview.h"

#include <cerrno> // errno
#include <array> // std::array
#include <locale>  // wstring_convert
#include <sys/stat.h> // stat,fstat
#if _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define _CRT_DISABLE_PERFCRIT_LOCKS 1 // we're running single-threaded I/O only
    #include <Windows.h>
    #include <direct.h> // mkdir, getcwd
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
#if __ANDROID__
    #include <jni.h>
#endif

namespace rpp /* ReCpp */
{
    ////////////////////////////////////////////////////////////////////////////////

    bool file_exists(const char* filename) noexcept
    {
        #if USE_WINAPI_IO
            DWORD attr = GetFileAttributesA(filename);
            return attr != DWORD(-1) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
        #else
            struct stat s;
            return ::stat(filename, &s) == 0 && !S_ISDIR(s.st_mode);
        #endif
    }

    bool file_exists(const wchar_t* filename) noexcept
    {
        #if USE_WINAPI_IO
            DWORD attr = GetFileAttributesW(filename);
            return attr != DWORD(-1) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
        #elif _MSC_VER
            struct _stat64 s;
            return _wstat64(filename, &s) == 0 && S_ISDIR(s.st_mode);
        #else
            return file_exists(rpp::to_string(filename).c_str());
        #endif
    }

    bool is_symlink(const char* filename) noexcept
    {
        #if USE_WINAPI_IO
            DWORD attr = GetFileAttributesA(filename);
            return attr != DWORD(-1) && (attr & FILE_ATTRIBUTE_REPARSE_POINT);
        #else
            struct stat s;
            return ::stat(filename, &s) == 0 && S_ISLNK(s.st_mode);
        #endif
    }

    bool is_symlink(const wchar_t* filename) noexcept
    {
        #if USE_WINAPI_IO
            DWORD attr = GetFileAttributesW(filename);
            return attr != DWORD(-1) && (attr & FILE_ATTRIBUTE_REPARSE_POINT);
        #elif _MSC_VER
            struct _stat64 s;
            return _wstat64(filename, &s) == 0 && S_ISLNK(s.st_mode);
        #else
            return is_symlink(rpp::to_string(filename).c_str());
        #endif
    }

    bool folder_exists(const char* folder) noexcept
    {
        #if USE_WINAPI_IO
            DWORD attr = GetFileAttributesA(folder);
            return attr != DWORD(-1) && (attr & FILE_ATTRIBUTE_DIRECTORY);
        #else
            struct stat s;
            return ::stat(folder, &s) == 0 && S_ISDIR(s.st_mode);
        #endif
    }

    bool file_or_folder_exists(const char* fileOrFolder) noexcept
    {
        #if USE_WINAPI_IO
            DWORD attr = GetFileAttributesA(fileOrFolder);
            return attr != DWORD(-1);
        #else
            struct stat s;
            return ::stat(fileOrFolder, &s) == 0;
        #endif
    }

    bool create_symlink(const char* target, const char* link) noexcept
    {
    #if _WIN32
        DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
        if (folder_exists(target))
            flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
        return CreateSymbolicLinkA(link, target, flags) == TRUE;
    #else
        return symlink(target, link) == 0;
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

        for (size_t i = 0; i < folders.size(); ++i)
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
    dir_iterator::dir_iterator(std::string&& dir) noexcept : dir{std::move(dir)} {  // NOLINT
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
    dir_iterator::dir_iterator(std::string&& dir) noexcept : dir{std::move(dir)} {
        const char* path = this->dir.empty() ? "." : this->dir.c_str();
        s->e = (s->d=opendir(path)) != nullptr ? readdir(s->d) : nullptr;
    }
    dir_iterator::~dir_iterator() noexcept { if (s->d) closedir(s->d); }
    dir_iterator::operator bool() const noexcept { return s->d && s->e; }
    strview dir_iterator::name() const noexcept { return s->e ? strview{s->e->d_name} : strview{}; }
    bool dir_iterator::next() noexcept { return s->d && (s->e = readdir(s->d)) != nullptr; }
    bool dir_iterator::is_dir() const noexcept {
        if (s->e && s->e->d_type) return s->e->d_type == DT_DIR;
        struct stat st;
        return ::stat(full_path().c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }
    bool dir_iterator::is_file() const noexcept {
        if (s->e && s->e->d_type) return s->e->d_type == DT_REG;
        struct stat st;
        return ::stat(full_path().c_str(), &st) == 0 && S_ISREG(st.st_mode);
    }
    bool dir_iterator::is_symlink() const noexcept {
        if (s->e && s->e->d_type) return s->e->d_type == DT_LNK;
        struct stat st;
        return ::stat(full_path().c_str(), &st) == 0 && S_ISLNK(st.st_mode);
    }
    bool dir_iterator::is_device() const noexcept {
        if (s->e && s->e->d_type) {
            int dt = s->e->d_type;
            return dt == DT_BLK || dt == DT_CHR || dt == DT_FIFO || dt == DT_SOCK;
        }
        struct stat st;
        if (::stat(full_path().c_str(), &st) == 0) {
            auto m = st.st_mode;
            return S_ISBLK(m) || S_ISCHR(m) || S_ISFIFO(m) || S_ISSOCK(m);
        }
        return false;
    }
#endif



    ////////////////////////////////////////////////////////////////////////////////

    using DirTraverseFunc = rpp::delegate<void(std::string&& path, bool isDir)>;

    // @param queryRoot The original path passed to the query.
    //                  For abs listing, this must be an absolute path value!
    //                  For rel listing, this is only used for opening the directory
    // @param relPath Relative path from search root, ex: "src", "src/session/util", etc.
    static NOINLINE void traverse_dir2(
        strview queryRoot, strview relPath, bool dirs, bool files, bool rec, bool abs,
        const DirTraverseFunc& func) noexcept
    {
        std::string currentDir = path_combine(queryRoot, relPath);
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
                    if (rec)
                    {
                        traverse_dir2(queryRoot, path_combine(relPath, e.name), dirs, files, rec, abs, func);
                    }
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
            std::string fullpath = full_path(dir.empty() ? "." : dir);
            if (!fullpath.empty()) // folder does not exist
                traverse_dir2(fullpath, strview{}, dirs, files, rec, abs, func);
        }
        else
        {
            traverse_dir2(dir, strview{}, dirs, files, rec, abs, func);
        }
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

    static void append_slash(char* path, int& len) noexcept
    {
        if (path[len-1] != '/')
            path[len++] = '/';
    }

    #if _WIN32
    static void win32_fixup_path(char* path, int& len) noexcept
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
        (void)moduleObject;
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
        (void)moduleObject;
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
