#include "file_io.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h> // stat,fstat
#if _WIN32 || _WIN64
    #define WIN32_LEAN_AND_MEAN
    #define _CRT_DISABLE_PERFCRIT_LOCKS 1 // we're running single-threaded I/O only
    #include <Windows.h>
    #include <direct.h> // mkdir
    #include <io.h>
    #define USE_WINAPI_IO 1
    #define stat64 _stat64
#else
    #include <unistd.h>
    #include <string.h>
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
    uint64 file::sizel() const
    {
        if (!Handle) return 0;
        #if USE_WINAPI_IO
            LARGE_INTEGER size;
            if (!GetFileSizeEx((HANDLE)Handle, &size)) {
                //fprintf(stderr, "GetFileSizeEx error: [%d]\n", GetLastError());
                return 0ull;
            }
            return size.QuadPart;
        #else
            struct _stat64 s;
            if (_fstat64(fileno((FILE*)Handle), &s)) {
                //fprintf(stderr, "_fstat64 error: [%s]\n", strerror(errno));
                return 0ull;
            }
            return s.st_size;
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
        file f(filename, READONLY);
        return f.read_all();
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
        return file(filename, IOFlags::CREATENEW).write(buffer, bytesToWrite);
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
    void file::time_info(time_t* outCreated, time_t* outAccessed, time_t* outModified) const
    {
        #if USE_WINAPI_IO
            GetFileTime((HANDLE)Handle, (FILETIME*)outCreated, 
                (FILETIME*)outAccessed, (FILETIME*)outModified);
        #else
            struct stat s;
            if (fstat(fileno((FILE*)Handle), &s)) {
                //fprintf(stderr, "fstat error: [%s]\n", strerror(errno));
                return 0ull;
            }
            if (outCreated)  *outCreated  = s.st_ctime;
            if (outAccessed) *outAccessed = s.st_atime;
            if (outModified) *outModified = s.st_mtime;
            return s.st_ctime;
        #endif
    }
    time_t file::time_created()  const { time_t t; time_info(&t, 0, 0); return t; }
    time_t file::time_accessed() const { time_t t; time_info(0, &t, 0); return t; }
    time_t file::time_modified() const { time_t t; time_info(0, 0, &t); return t; }


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
    bool file_exists(const string& filename) { return file_exists(filename.c_str());  }
    bool file_exists(const strview filename) { return file_exists(filename.to_cstr()); }

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
    bool folder_exists(const string& folder) { return folder_exists(folder.c_str());    }
    bool folder_exists(const strview folder) { return folder_exists(folder.to_cstr()); }

    void file_info(const char* filename, uint64_t* filesize, time_t* created, 
                                         time_t*   accessed, time_t* modified)
    {
        #if USE_WINAPI_IO
            WIN32_FILE_ATTRIBUTE_DATA data;
            GetFileAttributesExA(filename, GetFileExInfoStandard, &data);
            if (filesize) *filesize = LARGE_INTEGER{data.nFileSizeLow,(LONG)data.nFileSizeHigh}.QuadPart;
            if (created)  *created  = *(time_t*)&data.ftCreationTime;
            if (accessed) *accessed = *(time_t*)&data.ftLastAccessTime;
            if (modified) *modified = *(time_t*)&data.ftLastWriteTime;
        #else
            struct _stat64 s;
            if (stat64(filename, &s) == 0/*OK*/) {
                if (filesize) *filesize = s.st_size;
                if (created)  *created  = s.st_ctime;
                if (accessed) *accessed = s.st_atime;
                if (modified) *modified = s.st_mtime;
            }
        #endif
    }


    uint file_size(const char* filename)
    {
        uint64_t s; file_info(filename, &s, 0, 0, 0); return (uint)s;
    }
    uint file_size(const string& filename) { return file_size(filename.c_str());   }
    uint file_size(const strview filename) { return file_size(filename.to_cstr()); }


    uint64_t file_sizel(const char* filename)
    {
        uint64_t s; file_info(filename, &s, 0, 0, 0); return s;
    }
    uint64_t file_sizel(const string& filename) { return file_sizel(filename.c_str());   }
    uint64_t file_sizel(const strview filename) { return file_sizel(filename.to_cstr()); }


    time_t file_created(const char* filename)
    { 
        time_t t; file_info(filename, 0, &t, 0, 0); return t; 
    }
    time_t file_created(const string& filename) { return file_created(filename.c_str());   }
    time_t file_created(const strview filename) { return file_created(filename.to_cstr()); }


    time_t file_accessed(const char* filename)
    {
        time_t t; file_info(filename, 0, 0, &t, 0); return t; 
    }
    time_t file_accessed(const string& filename) { return file_accessed(filename.c_str());   }
    time_t file_accessed(const strview filename) { return file_accessed(filename.to_cstr()); }


    time_t file_modified(const char* filename)
    {
        time_t t; file_info(filename, 0, 0, 0, &t); return t;
    }
    time_t file_modified(const string& filename) { return file_modified(filename.c_str());   }
    time_t file_modified(const strview filename) { return file_modified(filename.to_cstr()); }


    bool create_folder(const char* filename)
    {
    #if _WIN32 || _WIN64
        return mkdir(filename) == 0;
    #else
        return mkdir(filename, 0700) == 0;
    #endif
    }
    bool create_folder(const string& filename) { return create_folder(filename.c_str());   }
    bool create_folder(const strview filename) { return create_folder(filename.to_cstr()); }


    bool delete_folder(const char* filename)
    {
        return rmdir(filename) == 0;
    }
    bool delete_folder(const string& filename) { return delete_folder(filename.c_str());   }
    bool delete_folder(const strview filename) { return delete_folder(filename.to_cstr()); }


#if _WIN32
    int path::list_dirs(vector<string>& out, const char* directory, const char* matchPattern)
    {
        out.clear();
        char findPattern[512];
        sprintf(findPattern, "%s\\%s", directory, matchPattern);
        WIN32_FIND_DATAA ffd;
        HANDLE hFind = FindFirstFileA(findPattern, &ffd);
        if (hFind == INVALID_HANDLE_VALUE)
            return 0;
        do
        {
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (strcmp(ffd.cFileName, ".") != 0 && strcmp(ffd.cFileName, "..") != 0)
                    out.emplace_back(ffd.cFileName);
            }
        } while (FindNextFileA(hFind, &ffd));
        FindClose(hFind);
        return (int)out.size();
    }
    int path::list_files(vector<string>& out, const char* directory, const char* matchPattern)
    {
        out.clear();
        char findPattern[512];
        sprintf(findPattern, "%s\\%s", directory, matchPattern);
        WIN32_FIND_DATAA ffd;
        HANDLE hFind = FindFirstFileA(findPattern, &ffd);
        if (hFind == INVALID_HANDLE_VALUE)
            return 0;
        do
        {
            if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                out.emplace_back(ffd.cFileName);
        } while (FindNextFileA(hFind, &ffd));
        FindClose(hFind);
        return (int)out.size();
    }
#else
    int path::list_dirs(vector<string>& out, const char* directory, const char* matchPattern)
    {
        out.clear();
        return (int)out.size();
    }
    int path::list_files(vector<string>& out, const char* directory, const char* matchPattern)
    {
        out.clear();
        return (int)out.size();
    }
#endif


    string path::working_dir()
    {
        char path[512];
        return string(getcwd(path, sizeof path) ? path : "");
    }
    void path::set_working_dir(const string& new_wd)
    {
        chdir(new_wd.c_str());
    }


    static size_t getfullpath(const char* filename, char* buffer)
    {
    #if _WIN32 || _WIN64
        if (size_t len = GetFullPathNameA(filename, 512, buffer, NULL))
            return len;
    #else
        if (char* result = realpath(filename, buffer))
            return strlen(result);
    #endif
        buffer[0] = '\0';
        return 0;
    }
    string path::fullpath(const string& relativePath)
    {
        return fullpath(relativePath.c_str());
    }
    string path::fullpath(const char* relativePath)
    {
        char path[512];
        return string{ path, getfullpath(relativePath, path) };
    }


    static const char* scanback(const char* start, const char* end)
    {
        if (end < start) // if end is invalid, default to start
            return start;
        for (; start < end; --end)
            if (*end == '/' || *end == '\\')
                return ++end; // return part after encounter
        return end;
    }
    static size_t getfilepart(const char* filename, const char** filepart)
    {
        int len = (int)strlen(filename);
        const char* ptr = scanback(filename, filename + len - 1);
        int filepartlen = int(filename + len - ptr);
        *filepart = ptr;
        return filepartlen;
    }


    string path::filename(const string& someFilePath)
    {
        return filename(someFilePath.c_str());
    }
    string path::filename(const char* someFilePath)
    {
        const char* filepart;
        size_t len = getfilepart(someFilePath, &filepart);
        return string{ filepart, len };
    }


    string path::filename_namepart(const string& someFilePath)
    {
        return filename_namepart(someFilePath.c_str());
    }
    string path::filename_namepart(const char * someFilePath)
    {
        const char* filepart;
        size_t len = getfilepart(someFilePath, &filepart);
        const char* ptr = filepart + len - 1;
        for (; filepart < ptr; --ptr) // scan back till first .
            if (*ptr == '.')
                break;
        if (ptr == filepart) // no extension
            ptr = filepart + len;   // use original end
        return string{ filepart, ptr };
    }


    string path::foldername(const string& someFolderPath)
    {
        return foldername(someFolderPath.c_str());
    }
    string path::foldername(const char* someFolderPath)
    {
        size_t len = strlen(someFolderPath);
        const char* ptr = scanback(someFolderPath, someFolderPath + len - 1);
        if (someFolderPath < ptr)
            --ptr;
        return string{ someFolderPath, ptr };
    }


}