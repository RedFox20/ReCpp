#pragma once
#ifndef RPP_FILE_IO_H
#define RPP_FILE_IO_H
/**
 * Cross platform file utilities, Copyright (c) 2014 - Jorma Rebane
 */
#include <time.h> // time_t
#include "strview.h"
#include <unordered_map> // keyvalue saving

namespace rpp /* ReCpp */
{
    using namespace std; // we love std; you should too.

    enum IOFlags {
        READONLY,			// opens an existing file for reading
        READWRITE,			// opens an existing file for read/write
        CREATENEW,			// creates new file for writing
        APPEND,             // opens an existing file for appending only
    };


    #ifndef SEEK_SET
    #define SEEK_SET 0
    #define SEEK_CUR 1
    #define SEEK_END 2
    #endif

    /**
     * Automatic whole file loading buffer
     */
    struct load_buffer
    {
        char* str;  // dynamic or internal buffer
        int   len;  // buffer size in bytes

        load_buffer() noexcept : str(0), len(0) {}
        // takes ownership of a malloc-ed pointer and frees it when out of scope
        load_buffer(char* buffer, int size) noexcept : str(buffer), len(size) {}
        ~load_buffer();

        load_buffer(const load_buffer& rhs)            = delete; // NOCOPY
        load_buffer& operator=(const load_buffer& rhs) = delete; // NOCOPY

        load_buffer(load_buffer&& mv) noexcept;
        load_buffer& operator=(load_buffer&& mv) noexcept;

        // acquire the data pointer of this load_buffer, making the caller own the buffer
        char* steal_ptr() noexcept;

        template<class T> operator T*() noexcept { return (T*)str; }
        int size()      const noexcept { return len; }
        int length()    const noexcept { return len; }
        char* data()    const noexcept { return str; }
        char* c_str()   const noexcept { return str; }
        explicit operator bool() const noexcept { return str != nullptr; }
        explicit operator strview() const noexcept { return { str,len }; }
    };

    ////////////////////////////////////////////////////////////////////////////////


    /**
     * Buffered FILE structure for performing random access read/write
     *
     *  Example usage:
     *         file f("test.obj");
     *         char* buffer = malloc(f.size());
     *         f.read(buffer, f.size());
     *
     */
    struct file
    {
        void*	Handle;	// File handle
        IOFlags	Mode;	// File openmode READWRITE or READONLY

        file() noexcept : Handle(nullptr), Mode(READONLY)
        {
        }

        /**
         * Opens an existing file for reading with mode = READONLY
         * Creates a new file for reading/writing with mode = READWRITE
         * @param filename File name to open or create
         * @param mode File open mode
         */
        file(const char* filename, IOFlags mode = READONLY) noexcept;
        file(const string&  filename, IOFlags mode = READONLY) noexcept : file(filename.c_str(), mode)
        {
        }
        file(const strview& filename, IOFlags mode = READONLY) noexcept;
        file(const wchar_t* filename, IOFlags mode = READONLY) noexcept;
        file(const wstring& filename, IOFlags mode = READONLY) noexcept : file(filename.c_str(), mode)
        {
        }
        file(file&& f) noexcept;
        ~file();

        file& operator=(file&& f) noexcept;

        file(const file& f) = delete;
        file& operator=(const file& f) = delete;

        /**
         * Opens an existing file for reading with mode = READONLY
         * Creates a new file for reading/writing with mode = READWRITE
         * @param filename File name to open or create
         * @param mode File open mode
         * @return TRUE if file open/create succeeded, FALSE if failed
         */
        bool open(const char* filename, IOFlags mode = READONLY) noexcept;
        bool open(const string& filename, IOFlags mode = READONLY) noexcept
        {
            return open(filename.c_str(), mode);
        }
        bool open(const strview filename, IOFlags mode = READONLY) noexcept;
        bool open(const wchar_t* filename, IOFlags mode = READONLY) noexcept;
        bool open(const wstring& filename, IOFlags mode = READONLY) noexcept
        {
            return open(filename.c_str(), mode);
        }
        void close() noexcept;

        /**
         * @return TRUE if file handle is valid (file exists or has been created)
         */
        bool good() const noexcept;
        explicit operator bool() const noexcept { return good(); }

        /**
         * @return TRUE if the file handle is INVALID
         */
        bool bad() const noexcept;

        /**
         * @return Size of the file in bytes
         */
        int size() const noexcept;

        /**
         * @return 64-bit unsigned size of the file in bytes
         */
        int64 sizel() const noexcept;

        /**
         * Reads a block of bytes from the file. Standard OS level
         * IO buffering is performed.
         *
         * @param buffer Buffer to read bytes to
         * @param bytesToRead Number of bytes to read from the file
         * @return Number of bytes actually read from the file
         */
        int read(void* buffer, int bytesToRead) noexcept;

        /**
         * Reads the entire contents of the file into a load_buffer
         * unbuffered_file is used internally
         */
        load_buffer read_all() noexcept;

        /**
         * Reads the entire contents of the file into a load_buffer
         * The file is opened as READONLY, unbuffered_file is used internally
         */
        static load_buffer read_all(const char* filename) noexcept;
        static load_buffer read_all(const string& filename) noexcept
        {
            return read_all(filename.c_str());
        }
        static load_buffer read_all(const strview& filename) noexcept;
        static load_buffer read_all(const wchar_t* filename) noexcept;
        static load_buffer read_all(const wstring& filename) noexcept
        {
            return read_all(filename.c_str());
        }

        /**
         * Reads a simple key-value map from file in the form of:
         * #comment
         * key1=value1\n
         * key2 = value2 \n
         * @return Hash map of key string and value string
         */
        static unordered_map<string, string> read_map(const strview& filename) noexcept;

        /**
         * Parses a simple key-value map from a load_buffer file.
         * Intended usage:
         *     load_buffer buf = file::read_all("values.txt");
         *     auto map = file::parse_map(buf);
         * @return Map of string views, referencing into buf
         */
        static unordered_map<strview, strview> parse_map(const load_buffer& buf) noexcept;

        /**
         * Writes a block of bytes to the file. Regular Windows IO
         * buffering is ENABLED for WRITE.
         *
         * @param buffer Buffer to write bytes from
         * @param bytesToWrite Number of bytes to write to the file
         * @return Number of bytes actually written to the file
         */
        int write(const void* buffer, int bytesToWrite) noexcept;

        /**
         * Writes a C array with size information to the file.
         * Relies on OS file buffering
         */
        template<class T, int N> int write(const T(&items)[N]) noexcept {
            return write(items, int(sizeof(T)) * N);
        }
        template<int N> int write(const char(&str)[N]) noexcept {
            return write(str, N - 1);
        }
        template<int N> int write(const wchar_t (&str)[N]) noexcept {
            return write(str, int(sizeof(wchar_t)) * (N - 1));
        }
        int write(const string& str) noexcept {
            return write(str.c_str(), (int)str.length());
        }
        int write(const strview& str) noexcept {
            return write(str.str, str.len);
        }
        int write(const wstring& str) noexcept {
            return write(str.c_str(), int(sizeof(wchar_t) * str.size()));
        }
        
        /**
         * Writes a formatted string to file
         * For format string reference, check printf() documentation.
         * @return Number of bytes written to the file
         */
        int writef(const char* format, ...) noexcept;

        /**
         * Writes a string to file and also appends a newline
         */
        int writeln(const strview& str) noexcept;

        /**
         * Just append a newline
         */
        int writeln() noexcept;

        /**
         * Stringifies and appends the input arguments one by one with an endline, filling gaps with spaces, similar to Python print()
         * Ex: writeln("test:", 10, 20.1f);  --> "test: 10 20.1\n"
         */
        template<class T, class... Args> int writeln(const T& first, const Args&... args) noexcept
        {
            string_buffer buf;
            buf.writeln(first, args...);
            return write(buf.str, buf.len);
        }

        /**
         * Forcefully flushes any OS file buffers to send all data to the storage device
         * @warning Don't call this too haphazardly, or you will ruin your IO performance!
         */
        void flush() noexcept;

        /**
         * Creates a new file and fills it with the provided data.
         * Regular Windows IO buffering is ENABLED for WRITE.
         *
         * Openmode is IOFlags::CREATENEW
         *
         * @param filename Name of the file to create and write to
         * @param buffer Buffer to write bytes from
         * @param bytesToWrite Number of bytes to write to the file
         * @return Number of bytes actually written to the file
         */
        static int write_new(const char* filename, const void* buffer, int bytesToWrite) noexcept;
        static int write_new(const strview& filename, const void* buffer, int bytesToWrite) noexcept;
        static int write_new(const string& filename, const void* buffer, int bytesToWrite) noexcept
        {
            return write_new(filename.c_str(), buffer, bytesToWrite);
        }
        static int write_new(const strview& filename, const strview& data) noexcept
        {
            return write_new(filename, data.str, data.len);
        }
        template<class T, class U>
        static int write_new(const strview& filename, const vector<T,U>& plainOldData) noexcept
        {
            return write_new(filename, plainOldData.data(), int(plainOldData.size()*sizeof(T)));
        }

        /**
         * Writes a simple key-value map to file in the form of:
         * key1=value1\n
         * key2=value2\n
         * Please avoid using \n in the keys or values
         * Key and Value types require .size() and .c_str()
         * @return Number of bytes written
         */
        template<class K, class V, class H, class C, class A>
        static int write_map(const strview& filename, const unordered_map<K, V, H, C, A>& map) noexcept
        {
            size_t required = 0;
            for (auto& kv : map)
                required += kv.first.size() + kv.second.size() + 2ul;

            string buffer; buffer.reserve(required);
            for (auto& kv : map)
            {
                buffer.append(kv.first.c_str(), kv.first.size());
                buffer += '=';
                buffer.append(kv.second.c_str(), kv.second.size());
                buffer += '\n';
            }
            return write_new(filename, buffer);
        }

        /**
         * Seeks to the specified position in a file. Seekmode is
         * determined like in fseek: SEEK_SET, SEEK_CUR and SEEK_END
         *
         * @param filepos Position in file where to seek to
         * @param seekmode Seekmode to use: SEEK_SET, SEEK_CUR or SEEK_END
         * @return Current position in the file
         */
        int seek(int filepos, int seekmode = SEEK_SET) noexcept;
        uint64 seekl(int64 filepos, int seekmode = SEEK_SET) noexcept;

        /**
         * @return Current position in the file
         */
        int tell() const noexcept;

        /**
         * @return 64-bit position in the file
         */
        int64 tell64() const noexcept;

        /**
         * Get multiple time info from this file handle
         */
        bool time_info(time_t* outCreated, time_t* outAccessed, time_t* outModified) const noexcept;

        /**
         * @return File creation time
         */
        time_t time_created() const noexcept;

        /**
         * @return File access time - when was this file last accessed?
         */
        time_t time_accessed() const noexcept;

        /**
         * @return File write time - when was this file last modified
         */
        time_t time_modified() const noexcept;

        /**
         * Gets the size of the file along with last modified time.
         * This has less syscalls than calling f.size() and f.time_modified() separately
         * @return Size of the file
         */
        int size_and_time_modified(time_t* outModified) const noexcept;
    };


    ////////////////////////////////////////////////////////////////////////////////


    /**
     * Stores a load buffer for line parsing
     */
    struct buffer_line_parser : line_parser
    {
        load_buffer buf;
        buffer_line_parser(load_buffer&& buf) noexcept : line_parser(buf.str, buf.len), buf(move(buf)) {}
        buffer_line_parser(const buffer_line_parser&) = delete; // NOCOPY
        buffer_line_parser& operator=(const buffer_line_parser&) = delete;
        buffer_line_parser(buffer_line_parser&&) = default;
        buffer_line_parser& operator=(buffer_line_parser&&) = default;
        explicit operator bool() const noexcept { return (bool)buf; }
        static buffer_line_parser from_file(strview filename) {
            return { file::read_all(filename) };
        }
    };


    /**
     * Stores a load buffer for bracket parsing
     */
    struct buffer_bracket_parser : bracket_parser
    {
        load_buffer buf;
        buffer_bracket_parser(load_buffer&& buf) noexcept : bracket_parser(buf.str, buf.len), buf(move(buf)) {}
        buffer_bracket_parser(const buffer_bracket_parser&) = delete; // NOCOPY
        buffer_bracket_parser& operator=(const buffer_bracket_parser&) = delete;
        buffer_bracket_parser(buffer_bracket_parser&&) = default;
        buffer_bracket_parser& operator=(buffer_bracket_parser&&) = default;
        operator bool() const noexcept { return (bool)buf; }
        static buffer_bracket_parser from_file(strview filename) {
            return { file::read_all(filename) };
        }
    };


    ////////////////////////////////////////////////////////////////////////////////


    /**
     * @return TRUE if the file exists, arg ex: "dir/file.ext"
     */
    bool file_exists(const char* filename) noexcept;
    inline bool file_exists(const string& filename) noexcept { return file_exists(filename.c_str());   }
    inline bool file_exists(const strview filename) noexcept { return file_exists(filename.to_cstr()); }

    /**
     * @return TRUE if the folder exists, arg ex: "root/dir" or "root/dir/"
     */
    bool folder_exists(const char* folder) noexcept;
    inline bool folder_exists(const string& folder) noexcept { return folder_exists(folder.c_str());   }
    inline bool folder_exists(const strview folder) noexcept { return folder_exists(folder.to_cstr()); }

    /**
     * @brief Gets basic information of a file
     * @param filename Name of the file, ex: "dir/file.ext"
     * @param filesize (optional) If not null, writes the long size of the file
     * @param created  (optional) If not null, writes the file creation date
     * @param accessed (optional) If not null, writes the last file access date
     * @param modified (optional) If not null, writes the last file modification date
     * @return TRUE if the file exists and required data was retrieved from the OS
     */
    bool file_info(const char* filename, int64*  filesize, time_t* created, 
                                         time_t* accessed, time_t* modified) noexcept;

    /**
     * @return Short size of a file
     */
    int file_size(const char* filename) noexcept;
    inline int file_size(const string& filename) noexcept { return file_size(filename.c_str());   }
    inline int file_size(const strview filename) noexcept { return file_size(filename.to_cstr()); }

    /**
     * @return Long size of a file
     */
    int64 file_sizel(const char* filename) noexcept;
    inline int64 file_sizel(const string& filename) noexcept { return file_sizel(filename.c_str());   }
    inline int64 file_sizel(const strview filename) noexcept { return file_sizel(filename.to_cstr()); }

    /**
     * @return File creation date
     */
    time_t file_created(const char* filename) noexcept;
    inline time_t file_created(const string& filename) noexcept { return file_created(filename.c_str());   }
    inline time_t file_created(const strview filename) noexcept { return file_created(filename.to_cstr()); }

    /**
     * @return Last file access date
     */
    time_t file_accessed(const char* filename) noexcept;
    inline time_t file_accessed(const string& filename) noexcept { return file_accessed(filename.c_str());   }
    inline time_t file_accessed(const strview filename) noexcept { return file_accessed(filename.to_cstr()); }

    /**
     * @return Last file modification date
     */
    time_t file_modified(const char* filename) noexcept;
    inline time_t file_modified(const string& filename) noexcept { return file_modified(filename.c_str());   }
    inline time_t file_modified(const strview filename) noexcept { return file_modified(filename.to_cstr()); }

    /**
     * @brief Deletes a single file, ex: "root/dir/file.ext"
     * @return TRUE if the file was actually deleted (can fail due to file locks or access rights)
     */
    bool delete_file(const char* filename) noexcept;
    inline bool delete_file(const string& filename) noexcept { return delete_file(filename.c_str());   }
    inline bool delete_file(const strview filename) noexcept { return delete_file(filename.to_cstr()); }

    /**
     * Creates a folder, recursively creating folders that do not exist
     * @return TRUE if the final folder was actually created (can fail due to access rights)
     */
    bool create_folder(const strview foldername) noexcept;
    bool create_folder(const wchar_t* foldername) noexcept;
    bool create_folder(const wstring& foldername) noexcept;

    /**
     * Deletes a folder, by default only if it's empty.
     * @param recursive If TRUE, all subdirectories and files will also be deleted (permanently)
     * @return TRUE if the folder was deleted
     */
    bool delete_folder(const string& foldername, bool recursive = false) noexcept;
    inline bool delete_folder(const char*   foldername, bool recursive = false) noexcept { return delete_folder(string{ foldername },   recursive); }
    inline bool delete_folder(const strview foldername, bool recursive = false) noexcept { return delete_folder(foldername.to_string(), recursive); }

    /**
     * @brief Resolves a relative path to a full path name using filesystem path resolution
     *        Ex: "path" ==> "C:\Projects\Test\path" 
     */
    string full_path(const char* path) noexcept;
    inline string full_path(const string& path) noexcept { return full_path(path.c_str());   }
    inline string full_path(const strview path) noexcept { return full_path(path.to_cstr()); }

    // merges all ../ of a full path
    string merge_dirups(const strview path) noexcept;

    /**
     * @brief Extract the filename (no extension) from a file path
     *        Ex: /root/dir/file.ext ==> file
     *        Ex: /root/dir/file     ==> file
     *        Ex: /root/dir/         ==> 
     *        Ex: file.ext           ==> file
     */
    strview file_name(const strview path) noexcept;

    /**
     * @brief Extract the file part (with ext) from a file path
     *        Ex: /root/dir/file.ext ==> file.ext
     *        Ex: /root/dir/file     ==> file
     *        Ex: /root/dir/         ==> 
     *        Ex: file.ext           ==> file.ext
     */
    strview file_nameext(const strview path) noexcept;

    /**
     * @brief Extract the extension from a file path
     *        Ex: /root/dir/file.ext ==> ext
     *        Ex: /root/dir/file     ==> 
     *        Ex: /root/dir/         ==> 
     *        Ex: file.ext           ==> ext
     */
    strview file_ext(const strview path) noexcept;

    /**
     * @brief Extract the foldername from a path name
     *        Ex: /root/dir/file.ext ==> dir
     *        Ex: /root/dir/file     ==> dir
     *        Ex: /root/dir/         ==> dir
     *        Ex: dir/               ==> dir
     *        Ex: file.ext           ==> 
     */
    strview folder_name(const strview path) noexcept;

    /**
     * @brief Extracts the full folder path from a file path.
     *        Will preserve / and assume input is always a filePath
     *        Ex: /root/dir/file.ext ==> /root/dir/
     *        Ex: /root/dir/file     ==> /root/dir/
     *        Ex: /root/dir/         ==> /root/dir/
     *        Ex: dir/               ==> dir/
     *        Ex: file.ext           ==> 
     */
    strview folder_path(const strview path) noexcept;
    wstring folder_path(const wchar_t* path) noexcept;
    wstring folder_path(const wstring& path) noexcept;
    /**
     * @brief Normalizes the path string to use a specific type of slash
     * @note This does not perform full path expansion.
     * @note The string is modified in-place !careful!
     *
     *       Ex:  \root\dir/file.ext ==> /root/dir/file.ext
     */
    string& normalize(string& path, char sep = '/') noexcept;
    char*   normalize(char*   path, char sep = '/') noexcept;

    /**
     * @brief Normalizes the path string to use a specific type of slash
     * @note A copy of the string is made
     */
    string normalized(const strview path, char sep = '/') noexcept;
    
    /**
     * @brief Efficiently combines two path strings, removing any repeated / or \
     *          Ex: path_combine("tmp", "file.txt")   ==> "tmp/file.txt"
     *          Ex: path_combine("tmp/", "file.txt")  ==> "tmp/file.txt"
     *          Ex: path_combine("tmp/", "/file.txt") ==> "tmp/file.txt"
     *          Ex: path_combine("tmp/", "/folder//") ==> "tmp/folder"
     */
    string path_combine(strview path1, strview path2) noexcept;

    ////////////////////////////////////////////////////////////////////////////////


    /**
     * Lists all folders inside this directory
     * @param out Destination vector for result folder names (not full folder paths!)
     * @param dir Relative or full path of this directory
     * @return Number of folders found
     */
    int list_dirs(vector<string>& out, strview dir) noexcept;
    inline vector<string> list_dirs(strview dir) noexcept
    {
        vector<string> out; list_dirs(out, dir); return out;
    }

    int list_dirs_fullpath(vector<string>& out, strview dir) noexcept;
    inline vector<string> list_dirs_fullpath(strview dir) noexcept
    {
        vector<string> out; list_dirs_fullpath(out, dir); return out;
    }

    /**
     * Lists all files inside this directory that have the specified extension (default: all files)
     * @param out Destination vector for result file names (not full file paths!)
     * @param dir Relative or full path of this directory
     * @param ext Filter files by extension, ex: "txt", default ("") lists all files
     * @return Number of files found that match the extension
     */
    int list_files(vector<string>& out, strview dir, strview ext = {}) noexcept;
    inline vector<string> list_files(strview dir, strview ext = {}) noexcept
    {
        vector<string> out; list_files(out, dir, ext); return out;
    }
    
    int list_files_fullpath(vector<string>& out, strview dir, strview ext = {}) noexcept;
    inline vector<string> list_files_fullpath(strview dir, strview ext = {}) noexcept
    {
        vector<string> out; list_files_fullpath(out, dir, ext); return out;
    }
    
    /**
     * Lists all files and folders inside a dir
     */
    int list_alldir(vector<string>& outdirs, vector<string>& outfiles, strview dir) noexcept;

    /**
     * Recursively lists all files under this directory and its subdirectories 
     * that have the specified extension (default: all files)
     * @param dir Relative or full path of root directory
     * @param ext Filter files by extension, ex: "txt", default ("") lists all files
     * @return vector of resulting relative file paths
     */
    vector<string> list_files_recursive(strview dir, strview ext = {}) noexcept;

    /**
     * Recursively lists all files under this directory and its subdirectories 
     * that match the list of extensions
     * @param dir Relative or full path of root directory
     * @param exts Filter files by extensions, ex: {"txt","cfg"}
     * @return vector of resulting relative file paths
     */
    vector<string> list_files_recursive(strview dir, const vector<strview>& exts) noexcept;

    /**
     * @return The current working directory of the application
     */
    string working_dir() noexcept;

    /**
     * Calls chdir() to set the working directory of the application to a new value
     * @return TRUE if chdir() is successful
     */
    bool change_dir(const char* new_wd) noexcept;
    inline bool change_dir(const string& new_wd) noexcept { return change_dir(new_wd.c_str()); }
    inline bool change_dir(const strview new_wd) noexcept { return change_dir(new_wd.to_cstr()); }


    ////////////////////////////////////////////////////////////////////////////////


} // namespace rpp
#endif // RPP_FILE_IO_H
