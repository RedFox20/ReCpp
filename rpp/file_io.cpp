#include "file_io.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h> // stat,fstat
#if _WIN32 || _WIN64
    #define WIN32_LEAN_AND_MEAN
    #define _CRT_DISABLE_PERFCRIT_LOCKS 1 // we're running single-threaded I/O only
    #include <Windows.h>
    #include <direct.h> // mkdir, getcwd
    #include <io.h>
    #define USE_WINAPI_IO 1
    #define stat64 _stat64
#else
    #include <unistd.h>
    #include <string.h>
    #include <dirent.h> // opendir()
#endif


namespace rpp /* ReCpp */
{
    load_buffer::~load_buffer()
    {
        if (ptr) free(ptr); // MEM_RELEASE
    }
    load_buffer::load_buffer(load_buffer&& mv) : ptr(mv.ptr), len(mv.len)
    {
        mv.ptr = 0;
        mv.len = 0;
    }
    load_buffer& load_buffer::operator=(load_buffer&& mv)
    {
        char* p = ptr;
        int   l = len;
        ptr = mv.ptr;
        len = mv.len;
        mv.ptr = p;
        mv.len = l;
        return *this;
    }
    // acquire the data pointer of this load_buffer, making the caller own the buffer
    char* load_buffer::steal_ptr()
    {
        char* p = ptr;
        ptr = 0;
        return p;
    }


    ////////////////////////////////////////////////////////////////////////////////


    static void* OpenFile(const char* filename, IOFlags mode)
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
        }
        SECURITY_ATTRIBUTES secu = { sizeof(secu), NULL, TRUE };
        void* handle = CreateFileA(filename, access, sharing, &secu, createmode, openFlags, 0);
        return handle != INVALID_HANDLE_VALUE ? handle : 0;
    #else
        const char* modeStr;
        switch (mode)
        {
            default: /* default mode */
            case READONLY:  modeStr = "rb";  break; // open existing for read-binary
            case READWRITE: modeStr = "wbx"; break; // open existing for r/w-binary
            case CREATENEW: modeStr = "wb";  break; // create new file for write-binary
        }
        return fopen(filename, modeStr);
    #endif
    }


    file::file(const char* filename, IOFlags mode)
        : Handle(OpenFile(filename, mode)), Mode(mode)
    {
    }
    file::file(const string& filename, IOFlags mode)
        : Handle(OpenFile(filename.c_str(), mode)), Mode(mode)
    {
    }
    file::file(const strview filename, IOFlags mode)
        : Handle(OpenFile(filename.to_cstr(), mode)), Mode(mode)
    {
    }
    file::file(file&& f) : Handle(f.Handle), Mode(f.Mode)
    {
        f.Handle = 0;
    }
    file::~file()
    {
        close();
    }
    file& file::operator=(file&& f)
    {
        close();
        Handle = f.Handle;
        Mode = f.Mode;
        f.Handle = 0;
        return *this;
    }
    bool file::open(const char* filename, IOFlags mode)
    {
        close();
        Mode = mode;
        return (Handle = OpenFile(filename, mode)) != nullptr;
    }
    bool file::open(const string& filename, IOFlags mode)
    {
        return open(filename.c_str(), mode);
    }
    bool file::open(const strview filename, IOFlags mode)
    {
        return open(filename.to_cstr(), mode);
    }
    void file::close()
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
    bool file::good() const
    {
        return Handle != nullptr;
    }
    bool file::bad() const
    {
        return Handle == nullptr;
    }
    int file::size() const
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
    int64 file::sizel() const
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
            struct _stat64 s;
            if (_fstat64(fileno((FILE*)Handle), &s)) {
                //fprintf(stderr, "_fstat64 error: [%s]\n", strerror(errno));
                return 0ull;
            }
            return (int64)s.st_size;
        #endif
    }
    int file::read(void* buffer, int bytesToRead)
    {
        #if USE_WINAPI_IO
            DWORD bytesRead;
            ReadFile((HANDLE)Handle, buffer, bytesToRead, &bytesRead, 0);
            return bytesRead;
        #else
            return (int)fread(buffer, bytesToRead, 1, (FILE*)Handle) * bytesToRead;
        #endif
    }
    load_buffer file::read_all()
    {
        int fileSize = size();
        if (!fileSize) return load_buffer(0, 0);

        char* buffer = (char*)malloc(fileSize);
        int bytesRead = read(buffer, fileSize);
        return load_buffer(buffer, bytesRead);
    }
    load_buffer file::read_all(const char* filename)
    {
        // make sure file f; remains until end of scope, or read_all will bug out
        return file{filename, READONLY}.read_all();
    }
    load_buffer file::read_all(const string& filename)
    {
        return read_all(filename.c_str());
    }
    load_buffer file::read_all(const strview filename)
    {
        return read_all(filename.to_cstr());
    }
    int file::write(const void* buffer, int bytesToWrite)
    {
        #if USE_WINAPI_IO
            DWORD bytesWritten;
            WriteFile((HANDLE)Handle, buffer, bytesToWrite, &bytesWritten, 0);
            return bytesWritten;
        #else
            return (int)fwrite(buffer, bytesToWrite, 1, (FILE*)Handle) * bytesToWrite;
        #endif
    }
    int file::write_new(const char* filename, const void* buffer, int bytesToWrite)
    {
        return file{filename, IOFlags::CREATENEW}.write(buffer, bytesToWrite);
    }
    int file::write_new(const string & filename, const void * buffer, int bytesToWrite)
    {
        return write_new(filename.c_str(), buffer, bytesToWrite);
    }
    int file::write_new(const strview filename, const void* buffer, int bytesToWrite)
    {
        char buf[512];
        return write_new(filename.to_cstr(buf,512), buffer, bytesToWrite);
    }
    int file::seek(int filepos, int seekmode)
    {
        #if USE_WINAPI_IO
            return SetFilePointer((HANDLE)Handle, filepos, 0, seekmode);
        #else
            fseek((FILE*)Handle, filepos, seekmode);
            return ftell((FILE*)Handle);
        #endif
    }
    uint64 file::seekl(uint64 filepos, int seekmode)
    {
        #if USE_WINAPI_IO
            LARGE_INTEGER newpos, nseek;
            nseek.QuadPart = filepos;
            SetFilePointerEx((HANDLE)Handle, nseek, &newpos, seekmode);
            return newpos.QuadPart;
        #else
            // @todo implement 64-bit seek
            fseek((FILE*)Handle, filepos, seekmode);
            return ftell((FILE*)Handle);
        #endif
    }
    int file::tell() const
    {
        #if USE_WINAPI_IO
            return SetFilePointer((HANDLE)Handle, 0, 0, FILE_CURRENT);
        #else
            return ftell((FILE*)Handle);
        #endif
    }
    bool file::time_info(time_t* outCreated, time_t* outAccessed, time_t* outModified) const
    {
    #if USE_WINAPI_IO
        return !!GetFileTime((HANDLE)Handle,         (FILETIME*)outCreated,
                             (FILETIME*)outAccessed, (FILETIME*)outModified);
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
    time_t file::time_created()  const { time_t t; return time_info(&t, 0, 0) ? t : 0ull; }
    time_t file::time_accessed() const { time_t t; return time_info(0, &t, 0) ? t : 0ull; }
    time_t file::time_modified() const { time_t t; return time_info(0, 0, &t) ? t : 0ull; }


    ////////////////////////////////////////////////////////////////////////////////


    bool file_exists(const char* filename)
    {
        #if USE_WINAPI_IO
            DWORD attr = GetFileAttributesA(filename);
            return attr != -1 && !(attr & FILE_ATTRIBUTE_DIRECTORY);
        #else
            struct stat s;
            return stat(filename, &s) ? false : (s.st_mode & S_IFDIR) == 0;
        #endif
    }

    bool folder_exists(const char* folder)
    {
        #if USE_WINAPI_IO
            DWORD attr = GetFileAttributesA(folder);
            return attr != -1 && (attr & FILE_ATTRIBUTE_DIRECTORY);
        #else
            struct stat s;
            return stat(filename, &s) ? false : (s.st_mode & S_IFDIR) != 0;
        #endif
    }

    bool file_info(const char* filename, int64*  filesize, time_t* created, 
                                         time_t* accessed, time_t* modified)
    {
    #if USE_WINAPI_IO
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (GetFileAttributesExA(filename, GetFileExInfoStandard, &data)) {
            if (filesize) *filesize = LARGE_INTEGER{data.nFileSizeLow,(LONG)data.nFileSizeHigh}.QuadPart;
            if (created)  *created  = *(time_t*)&data.ftCreationTime;
            if (accessed) *accessed = *(time_t*)&data.ftLastAccessTime;
            if (modified) *modified = *(time_t*)&data.ftLastWriteTime;
            return true;
        }
    #else
        struct _stat64 s;
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

    int file_size(const char* filename)
    {
        int64 s; 
        return file_info(filename, &s, 0, 0, 0) ? (int)s : 0;
    }
    int64 file_sizel(const char* filename)
    {
        int64 s; 
        return file_info(filename, &s, 0, 0, 0) ? s : 0ll;
    }
    time_t file_created(const char* filename)
    {
        time_t t; 
        return file_info(filename, 0, &t, 0, 0) ? t : 0ull; 
    }
    time_t file_accessed(const char* filename)
    {
        time_t t; 
        return file_info(filename, 0, 0, &t, 0) ? t : 0ull;
    }
    time_t file_modified(const char* filename)
    {
        time_t t; 
        return file_info(filename, 0, 0, 0, &t) ? t : 0ull;
    }
    bool delete_file(const char* filename)
    {
        return ::remove(filename) == 0;
    }

    static bool sys_mkdir(const strview foldername)
    {
    #if _WIN32 || _WIN64
        return mkdir(foldername.to_cstr()) == 0;
    #else
        return mkdir(foldername.to_cstr(), 0700) == 0;
    #endif
    }

    bool create_folder(const strview foldername)
    {
        if (sys_mkdir(foldername))
            return true; // best case, no recursive mkdir required

        // ugh, need to work our way upward to find a root dir that exists:
        // @note heavily optimized to minimize folder_exists() and mkdir() syscalls
        const char* fs = foldername.begin();
        const char* fe = foldername.end();
        const char* p = fe;
        while ((p = strview{fs,p}.rfindany("/\\")))
        {
            if (folder_exists(strview{fs,p}))
                break;
        }

        // now create all the parent dirs between:
        ++p;
        while (const char* e = strview{p,fe}.findany("/\\"))
        {
            if (!sys_mkdir(strview{fs,e}))
                return false; // ugh, something went really wrong here...
            p = e + 1;
        }
        return sys_mkdir(foldername); // and now create the final dir
    }

    bool delete_folder(const string& foldername, bool recursive) 
    {
        if (!recursive)
            return rmdir(foldername.c_str()) == 0; // easy path, just gently try to delete...

        vector<string> folders;
        vector<string> files;
        bool deletedChildren = true;

        if (list_alldir(folders, files, foldername))
        {
            for (const string& folder : folders)
                deletedChildren |= delete_folder(foldername + '/' + folder, true);

            for (const string& file : files)
                deletedChildren |= delete_file(foldername + '/' + file);
        }

        if (deletedChildren)
            return rmdir(foldername.c_str()) == 0; // should be okay to remove now

        return false; // no way to delete, since some subtree files are protected
    }


    string full_path(const char* path)
    {
        char buf[512];
        #if _WIN32 || _WIN64
            size_t len = GetFullPathNameA(path, 512, buf, NULL);
            return len ? string{ buf,len } : string{};
        #else
            char* res = realpath(path, buf);
            return res ? string{ res } : string{};
        #endif
    }


    strview file_name(const strview path)
    {
        strview nameext = file_nameext(path);

        if (const char* dot = nameext.rfind('.'))
            return strview{ nameext.str, dot };
        return nameext;
    }


    strview file_nameext(const strview path)
    {
        if (const char* str = path.rfindany("/\\"))
            return strview{ str + 1, path.end() };
        return path; // assume it's just a file name
    }


    strview folder_name(const strview path)
    {
        strview folder = folder_path(path);
        if (folder)
        {
            if (const char* str = folder.chomp_last().rfindany("/\\"))
                return strview{ str + 1, folder.end() };
        }
        return folder;
    }


    strview folder_path(const strview path)
    {
        if (const char* end = path.rfindany("/\\"))
            return strview{ path.str, end + 1 };
        return strview{};
    }


    string& normalize(string& path, char sep)
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
    char* normalize(char* path, char sep)
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

    string normalized(const strview path, char sep)
    {
        string res = path.to_string();
        normalize(res, sep);
        return res;
    }


    ////////////////////////////////////////////////////////////////////////////////


#if _WIN32
    struct dir_iterator {
        HANDLE hFind;
        WIN32_FIND_DATAA ffd;
        dir_iterator(const strview& dir) {
            char path[512]; sprintf(path, "%.*s/*", dir.len, dir.str);
            if ((hFind = FindFirstFileA(path, &ffd)) == INVALID_HANDLE_VALUE)
                hFind = 0;
        }
        ~dir_iterator() { if (hFind) FindClose(hFind); }
        operator bool() const { return hFind != 0; }
        bool is_dir() const { return (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
        const char* name() const { return ffd.cFileName; }
        bool next() { return FindNextFileA(hFind, &ffd) != 0; }
    };
#else
    struct dir_iterator {
        DIR* d;
        dirent* e;
        dir_iterator(const strview& dir) {
            if ((d=opendir(dir.to_cstr()))) 
                e = readdir(d);
        }
        ~dir_iterator() { if (d) closedir(d); }
        operator bool() const { return d && e; }
        bool is_dir() const { return e->d_type == DT_DIR; }
        const char* name() const { return e->d_name; }
        bool next() { return (e = readdir(d)) != 0; }
    };
#endif

    int list_dirs(vector<string>& out, strview dir)
    {
        if (out.size()) out.clear();

        if (dir_iterator it = { dir }) {
            do {
                if (it.is_dir() && it.name()[0] != '.') {
                    out.emplace_back(it.name());
                }
            } while (it.next());
        }
        return (int)out.size();
    }

    int list_files(vector<string>& out, strview dir, strview ext)
    {
        if (out.size()) out.clear();

        if (dir_iterator it = { dir }) {
            do {
                if (!it.is_dir()) {
                    strview fname = it.name();
                    if (ext && !fname.ends_withi(ext))
                        continue;
                    out.emplace_back(fname.str, fname.len);
                }
            } while (it.next());
        }
        return (int)out.size();
    }

    int list_alldir(vector<string>& outdirs, vector<string>& outfiles, strview dir)
    {
        if (outdirs.size())  outdirs.clear();
        if (outfiles.size()) outfiles.clear();

        if (dir_iterator it = { dir }) {
            do {
                if (!it.is_dir())             outfiles.emplace_back(it.name());
                else if (it.name()[0] != '.') outdirs.emplace_back(it.name());
            } while (it.next());
        }
        return (int)outdirs.size() + (int)outfiles.size();
    }

    string working_dir()
    {
        char path[512];
        return string(getcwd(path, sizeof(path)) ? path : "");
    }

    void set_working_dir(const string& new_wd)
    {
        chdir(new_wd.c_str());
    }


    ////////////////////////////////////////////////////////////////////////////////


} // namespace rpp
