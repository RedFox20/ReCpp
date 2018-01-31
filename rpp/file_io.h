#pragma once
/**
 * Cross platform file utilities, Copyright (c) 2014-2018, Jorma Rebane
 * Distributed under MIT Software License
 *
 * @note This module predates C++17 filesystem and offers a different
 *       set of convenient API's for dealing with every-day File I/O tasks.
 */
#if _MSC_VER
#  pragma warning(disable: 4251)
#endif
#include <ctime> // time_t
#include "strview.h"
#include <vector>
#if __has_include("sprint.h")
#  include "sprint.h"
#else
#  include <unordered_map>
#endif

#ifndef RPPAPI
#  if _MSC_VER
#    define RPPAPI __declspec(dllexport)
#  else // clang/gcc
#    define RPPAPI __attribute__((visibility("default")))
#  endif
#endif

namespace rpp /* ReCpp */
{
    using std::string;
    using std::wstring;
    using std::vector;
    using std::unordered_map;

    enum IOFlags {
        READONLY,			// opens an existing file for reading
        READWRITE,			// opens an existing file for read/write
        CREATENEW,			// creates new file for writing
        APPEND,             // create new OR opens an existing file for appending only
    };


    #ifndef SEEK_SET
    #define SEEK_SET 0
    #define SEEK_CUR 1
    #define SEEK_END 2
    #endif

    /**
     * Automatic whole file loading buffer
     */
    struct RPPAPI load_buffer
    {
        char* str;  // dynamic or internal buffer
        int   len;  // buffer size in bytes

        load_buffer() noexcept : str(nullptr), len(0) {}
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
        strview view() const { return { str, len }; }
    };

    ////////////////////////////////////////////////////////////////////////////////


    /**
     * @brief Buffered FILE structure for performing random access read/write
     *
     * Example: opening a file and reading some info
     * @code 
     *     if (file f {"example.txt"}) {
     *         MyStruct s;
     *         f.read(&s); // read simple binary data blocks
     *     }
     * @endcode
     * 
     * Example: reading all bytes from a file
     * @code
     *     if (load_buffer buf = file::read_all("example.txt")) {
     *         printf("%.*s", buf.len, buf.str);
     *     }
     * @endcode
     * 
     * Example: parsing lines from a file and writing data to a file
     * @code
     *     if (auto parser = buffer_line_parser::from_file("datalines.txt")) {
     *         while (strview line = parser.next_line())
     *             println(line);
     *     }
     *     
     *     if (file f {"datalines.txt", CREATENEW}) {
     *         f.writeln("line1", 10, 20.1f);   // writes 'line1 10 20.1\n'
     *         f.writef("a more %.2f complicated %.*s\n", 3.14159, 4, "line");
     *     }
     * @endcode
     * 
     * Example: parsing or writing a key-value map
     * @code
     *     keyvals.txt : "key1=value1\nkey2=value2\n"
     *     
     *     unordered_map<string,string> keyvals = file::read_map("keyvals.txt");
     *     
     *     file::write_map("keyvals.txt", {
     *         { "key1", "value1" },
     *         { "key2", "value2" },
     *     });
     * @endcode
     */
    struct RPPAPI file
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
        bool open(strview filename, IOFlags mode = READONLY) noexcept;
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

        template<class T> int read(T* object) noexcept
        {
            return read(object, sizeof(T));
        }

        /**
         * Reads the entire contents of the file into a load_buffer
         * unbuffered_file is used internally
         */
        load_buffer read_all() noexcept;

        /**
         * Reads the entire contents of the file into an std::string
         */
        string read_text() noexcept;

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
         * @code
         *     key1=value1\n
         *     key2 = value2 \n
         * @endcode
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
            return write(str.c_str(), (int)str.size());
        }
        int write(const strview& str) noexcept {
            return write(str.str, str.len);
        }
        int write(const wstring& str) noexcept {
            return write(str.c_str(), int(sizeof(wchar_t) * str.size()));
        }
#if RPP_SPRINT_H
        int write(const string_buffer& sb) noexcept {
            return write(sb.ptr, sb.len);
        }
#endif
        
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

#if RPP_SPRINT_H
        /**
         * @brief Stringifies and appends the input arguments one by one with an endline, filling gaps with spaces, similar to Python print()
         * 
         * Example:
         * @code writeln("test:", 10, 20.1f);  --> "test: 10 20.1\n" @endcode
         */
        template<class T, class... Args> int writeln(const T& first, const Args&... args) noexcept
        {
            string_buffer buf;
            buf.writeln(first, args...);
            return write(buf.ptr, buf.len);
        }
#endif

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
         * @code
         *     key1=value1\n
         *     key2=value2\n
         * @endcode
         * @note Please avoid using \n in the keys or values
         * @note Key and Value types require .size() and .c_str()
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
     * Generic parser that stores a file load buffer, with easy construction and usability
     * Example:
     * @code
     *   using buffer_line_parser = buffer_parser<line_parser>;
     *   if (auto parser = buffer_line_parser::from_file("datalines.txt");
     *   {
     *       while (strview line = parser.next_line())
     *           println(line);
     *   }
     * @endcode
     */
    template<class parser> struct buffer_parser : parser
    {
        load_buffer dataOwner;

        buffer_parser(load_buffer&& buf) noexcept : line_parser(buf.str, buf.len), dataOwner(std::move(buf)) {}

        // resets the parser state
        void reset() noexcept { parser::buffer = (strview)dataOwner; }

        explicit operator bool() const noexcept { return (bool)dataOwner; }
        static buffer_parser from_file(strview filename) noexcept {
            return { file::read_all(filename) };
        }
    };

    using buffer_line_parser    = buffer_parser<line_parser>;
    using buffer_bracket_parser = buffer_parser<bracket_parser>;
    using buffer_keyval_parser  = buffer_parser<keyval_parser>;
    
    /**
     * Key-Value map parser that stores a file load buffer, with easy construction and usability
     * Example:
     * @code
     *   if (auto map = key_value_map::from_file("keyvalues.txt");
     *   {
     *       println(map["key"]);
     *   }
     * @endcode
     */
    struct key_value_map
    {
        load_buffer dataOwner;
        unordered_map<strview, strview> map;
        
        key_value_map(load_buffer&& buf) noexcept : dataOwner(std::move(buf)), map(file::parse_map(dataOwner)) {}
        
        explicit operator bool() const noexcept { return (bool)dataOwner && !map.empty(); }
        int size()   const { return (int)map.size(); }
        bool empty() const { return map.empty(); }
        
        strview operator[](strview key) const noexcept {
            auto it = map.find(key);
            return it == map.end() ? strview{} : it->second;
        }
        
        static key_value_map from_file(strview filename) noexcept {
            return { file::read_all(filename) };
        }
    };

    ////////////////////////////////////////////////////////////////////////////////


    /**
     * @return TRUE if the file exists, arg ex: "dir/file.ext"
     */
    RPPAPI bool file_exists(const char* filename) noexcept;
    inline bool file_exists(const string& filename) noexcept { return file_exists(filename.c_str());   }
    inline bool file_exists(const strview filename) noexcept { return file_exists(filename.to_cstr()); }

    /**
     * @return TRUE if the folder exists, arg ex: "root/dir" or "root/dir/"
     */
    RPPAPI bool folder_exists(const char* folder) noexcept;
    inline bool folder_exists(const string& folder) noexcept { return folder_exists(folder.c_str());   }
    inline bool folder_exists(const strview folder) noexcept { return folder_exists(folder.to_cstr()); }

    /**
     * @return TRUE if either a file or a folder exists at the given path
     */
    RPPAPI bool file_or_folder_exists(const char* fileOrFolder) noexcept;
    inline bool file_or_folder_exists(const string& folder) noexcept { return file_or_folder_exists(folder.c_str());   }
    inline bool file_or_folder_exists(const strview folder) noexcept { return file_or_folder_exists(folder.to_cstr()); }

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
    RPPAPI int file_size(const char* filename) noexcept;
    inline int file_size(const string& filename) noexcept { return file_size(filename.c_str());   }
    inline int file_size(const strview filename) noexcept { return file_size(filename.to_cstr()); }

    /**
     * @return Long size of a file
     */
    RPPAPI int64 file_sizel(const char* filename) noexcept;
    inline int64 file_sizel(const string& filename) noexcept { return file_sizel(filename.c_str());   }
    inline int64 file_sizel(const strview filename) noexcept { return file_sizel(filename.to_cstr()); }

    /**
     * @return File creation date
     */
    RPPAPI time_t file_created(const char* filename) noexcept;
    inline time_t file_created(const string& filename) noexcept { return file_created(filename.c_str());   }
    inline time_t file_created(const strview filename) noexcept { return file_created(filename.to_cstr()); }

    /**
     * @return Last file access date
     */
    RPPAPI time_t file_accessed(const char* filename) noexcept;
    inline time_t file_accessed(const string& filename) noexcept { return file_accessed(filename.c_str());   }
    inline time_t file_accessed(const strview filename) noexcept { return file_accessed(filename.to_cstr()); }

    /**
     * @return Last file modification date
     */
    RPPAPI time_t file_modified(const char* filename) noexcept;
    inline time_t file_modified(const string& filename) noexcept { return file_modified(filename.c_str());   }
    inline time_t file_modified(const strview filename) noexcept { return file_modified(filename.to_cstr()); }

    /**
     * @brief Deletes a single file, ex: "root/dir/file.ext"
     * @return TRUE if the file was actually deleted (can fail due to file locks or access rights)
     */
    RPPAPI bool delete_file(const char* filename) noexcept;
    inline bool delete_file(const string& filename) noexcept { return delete_file(filename.c_str());   }
    inline bool delete_file(const strview filename) noexcept { return delete_file(filename.to_cstr()); }

    /**
     * @brief Copies sourceFile to destinationFile, overwriting the previous file!
     * @return TRUE if sourceFile was opened && destinationFile was created && copied successfully
     */
    RPPAPI bool copy_file(const char* sourceFile, const char* destinationFile) noexcept;
    RPPAPI bool copy_file(const strview sourceFile, const strview destinationFile) noexcept;

    /**
     * @brief Copies sourceFile into destinationFolder, overwriting the previous file!
     * @return TRUE if sourceFile was opened && destinationFolder was created && copied successfully
     */
    RPPAPI bool copy_file_into_folder(const strview sourceFile, const strview destinationFolder) noexcept;

    /**
     * Creates a folder, recursively creating folders that do not exist
     * @param foldername Relative or Absolute path
     * @return TRUE if the final folder was actually created (can fail due to access rights)
     */
    RPPAPI bool create_folder(strview foldername) noexcept;
    RPPAPI bool create_folder(const wchar_t* foldername) noexcept;
    RPPAPI bool create_folder(const wstring& foldername) noexcept;

    enum delete_mode
    {
        non_recursive,
        recursive,
    };

    /**
     * Deletes a folder, by default only if it's empty.
     * @param foldername Relative or Absolute path
     * @param mode If delete_mode::recursive, all subdirectories and files will also be deleted (permanently)
     * @return TRUE if the folder was deleted
     */
    RPPAPI bool delete_folder(strview foldername, delete_mode mode = non_recursive) noexcept;

    /**
     * @brief Resolves a relative path to a full path name using filesystem path resolution
     * @result "path" ==> "C:\Projects\Test\path" 
     */
    RPPAPI string full_path(const char* path) noexcept;
    inline string full_path(const string& path) noexcept { return full_path(path.c_str());   }
    inline string full_path(const strview path) noexcept { return full_path(path.to_cstr()); }

    /**
     * @brief Merges all ../ inside of a path
     * @result  ../lib/../bin/file.txt ==> ../bin/file.txt
     */
    RPPAPI string merge_dirups(strview path) noexcept;

    /**
     * @brief Extract the filename (no extension) from a file path
     * 
     * @result /root/dir/file.ext ==> file
     * @result /root/dir/file     ==> file
     * @result /root/dir/         ==> 
     * @result file.ext           ==> file
     * @result /.git/f.reallylong ==> f.reallylong
     * @result /.git/filewnoext   ==> filewnoext
     */
    RPPAPI strview file_name(strview path) noexcept;

    /**
     * @brief Extract the file part (with ext) from a file path
     * @result /root/dir/file.ext ==> file.ext
     * @result /root/dir/file     ==> file
     * @result /root/dir/         ==> 
     * @result file.ext           ==> file.ext
     */
    RPPAPI strview file_nameext(strview path) noexcept;

    /**
     * @brief Extract the extension from a file path
     * @result /root/dir/file.ext ==> ext
     * @result /root/dir/file     ==> 
     * @result /root/dir/         ==> 
     * @result file.ext           ==> ext
     * @result /.git/f.reallylong ==> 
     * @result /.git/filewnoext   ==> 
     */
    RPPAPI strview file_ext(strview path) noexcept;

    /**
     * @brief Replaces the current file path extension
     *        Usage: file_replace_ext("dir/file.old", "new");
     * @result /dir/file.old ==> /dir/file.new
     * @result /dir/file     ==> /dir/file.new
     * @result /dir/         ==> /dir/
     * @result file.old      ==> file.new
     */
    RPPAPI string file_replace_ext(strview path, strview ext);
    
    /**
     * @brief Changes only the file name by appending a string, leaving directory and extension untouched
     * @result /dir/file.txt ==> /dir/fileadd.txt
     * @result /dir/file     ==> /dir/fileadd
     * @result /dir/         ==> /dir/
     * @result file.txt      ==> fileadd.txt
     */
    RPPAPI string file_name_append(strview path, strview add);
    
    /**
     * @brief Replaces only the file name of the path, leaving directory and extension untouched
     * @result /dir/file.txt ==> /dir/replaced.txt
     * @result /dir/file     ==> /dir/replaced
     * @result /dir/         ==> /dir/
     * @result file.txt      ==> replaced.txt
     */
    RPPAPI string file_name_replace(strview path, strview newFileName);
    
    /**
     * @brief Replaces the file name and extension, leaving directory untouched
     * @result /dir/file.txt ==> /dir/replaced.bin
     * @result /dir/file     ==> /dir/replaced
     * @result /dir/         ==> /dir/
     * @result file.txt      ==> replaced.bin
     */
    RPPAPI string file_nameext_replace(strview path, strview newFileNameAndExt);

    /**
     * @brief Extract the foldername from a path name
     * @result /root/dir/file.ext ==> dir
     * @result /root/dir/file     ==> dir
     * @result /root/dir/         ==> dir
     * @result dir/               ==> dir
     * @result file.ext           ==> 
     */
    RPPAPI strview folder_name(strview path) noexcept;

    /**
     * @brief Extracts the full folder path from a file path.
     *        Will preserve / and assume input is always a filePath
     * @result /root/dir/file.ext ==> /root/dir/
     * @result /root/dir/file     ==> /root/dir/
     * @result /root/dir/         ==> /root/dir/
     * @result dir/               ==> dir/
     * @result file.ext           ==> 
     */
    RPPAPI strview folder_path(strview path) noexcept;
    RPPAPI wstring folder_path(const wchar_t* path) noexcept;
    RPPAPI wstring folder_path(const wstring& path) noexcept;

    /**
     * @brief Normalizes the path string to use a specific type of slash
     * @note This does not perform full path expansion.
     * @note The string is modified in-place !careful!
     *
     * @result \root/dir\\file.ext ==> /root/dir/file.ext
     */
    RPPAPI string& normalize(string& path, char sep = '/') noexcept;
    RPPAPI char*   normalize(char*   path, char sep = '/') noexcept;

    /**
     * @brief Normalizes the path string to use a specific type of slash
     * @note A copy of the string is made
     */
    RPPAPI string normalized(strview path, char sep = '/') noexcept;
    
    /**
     * @brief Efficiently combines two path strings, removing any repeated / or \
     * @result path_combine("tmp", "file.txt")   ==> "tmp/file.txt"
     * @result path_combine("tmp/", "file.txt")  ==> "tmp/file.txt"
     * @result path_combine("tmp/", "/file.txt") ==> "tmp/file.txt"
     * @result path_combine("tmp/", "/folder//") ==> "tmp/folder"
     */
    RPPAPI string path_combine(strview path1, strview path2) noexcept;

    ////////////////////////////////////////////////////////////////////////////////


    /**
     * Basic and minimal directory iterator.
     * @note This is not recursive!
     * Example usage:
     * @code
     *     for (dir_entry e : dir_iterator { dir })
     *         if (e.is_dir) e.add_path_to(out);
     * @endcode
     */
    class RPPAPI dir_iterator
    {
        struct impl;
        struct dummy {
            #if _WIN32
                void* hFind;
                char ffd[320];
            #else
                void* d;
                void* e;
            #endif
            impl* operator->();
            const impl* operator->() const;
        };

        dummy s; // iterator state
        const string dir;       // original path used to construct this dir_iterator
        const string reldir;    // relative directory
        mutable string fulldir; // full path to directory we're iterating

    public:
        explicit dir_iterator(const strview& dir) : dir_iterator{ dir.to_string() } {}
        explicit dir_iterator(const string& dir)  : dir_iterator{ string{dir}     } {}
        explicit dir_iterator(string&& dir);
        ~dir_iterator();

        struct entry
        {
            const dir_iterator* it;
            const strview name;
            const bool is_dir;
            entry(const dir_iterator* it) : it{it}, name{ it->name() }, is_dir{ it->is_dir() }
            {
            }
            string path()      const { return path_combine(it->path(),      name); }
            string full_path() const { return path_combine(it->full_path(), name); }
            // all files and directories that aren't "." or ".." are valid
            bool is_valid()    const { return !is_dir || (name != "." && name != ".."); }
            bool add_path_to(vector<string>& out) const {
                if (is_valid()) {
                    out.emplace_back(path());
                    return true;
                }
                return false;
            }
            bool add_full_path_to(vector<string>& out) const {
                if (is_valid()) {
                    out.emplace_back(full_path());
                    return true;
                }
                return false;
            }
        };

        struct iter { // bare minimum for basic looping
            dir_iterator* it;
            bool operator==(const iter& other) const { return it == other.it; }
            bool operator!=(const iter& other) const { return it != other.it; }
            entry operator*() const { return { it }; }
            iter& operator++() {
                if (!it->next()) it = nullptr;
                return *this;
            }
        };

        explicit operator bool() const;
        bool next(); // find next directory entry
        bool is_dir()  const; // if current entry is a dir
        strview name() const; // current entry name
        entry current() const { return { this }; }
        iter begin()          { return { this }; }
        iter end()      const { return { nullptr }; }

        // original path used to construct this dir_iterator
        const string& path() const { return dir; }

        // full path to directory we're iterating
        const string& full_path() const {
            return fulldir.empty() ? (fulldir = rpp::full_path(dir)) : fulldir;
        }
    };

    using dir_entry = dir_iterator::entry;

    ////////////////////////////////////////////////////////////////////////////////


    /**
     * Lists all folders inside this directory
     * @note By default: not recursive
     * @param out Destination vector for result folder names (not full folder paths!)
     * @param dir Relative or full path of this directory
     * @param recursive (default: false) If true, will perform a recursive search
     * @param fullpath (default: false) If true, returned paths will be fullpaths instead of relative
     * @return Number of folders found
     *
     * @example [recursive=false] [fullpath=false] vector<string> {
     *     "bin",
     *     "src",
     *     "lib",
     * };
     * @example [recursive=true] [fullpath=false] vector<string> {
     *     "bin",
     *     "bin/data",
     *     "bin/data/models",
     *     "src",
     *     "src/mesh",
     *     "lib",
     * };
     * @example [recursive=false] [fullpath=true] vector<string> {
     *     "/projects/ReCpp/bin",
     *     "/projects/ReCpp/src",
     *     "/projects/ReCpp/lib",
     * };
     * @example [recursive=true] [fullpath=true] vector<string> {
     *     "/projects/ReCpp/bin",
     *     "/projects/ReCpp/bin/data",
     *     "/projects/ReCpp/bin/data/models",
     *     "/projects/ReCpp/src",
     *     "/projects/ReCpp/src/mesh",
     *     "/projects/ReCpp/lib",
     * };
     */
    RPPAPI int list_dirs(vector<string>& out, strview dir, bool recursive = false, bool fullpath = false) noexcept;

    inline vector<string> list_dirs(strview dir, bool recursive = false, bool fullpath = false) noexcept {
        vector<string> out; list_dirs(out, dir, recursive, fullpath); return out;
    }
    inline int list_dirs_fullpath(vector<string>& out, strview dir, bool recursive = false) noexcept {
        return list_dirs(out, dir, recursive, true);
    }
    inline vector<string> list_dirs_fullpath(strview dir, bool recursive = false) noexcept {
        return list_dirs(dir, recursive, true);
    }
    inline vector<string> list_dirs_fullpath_recursive(strview dir) noexcept {
        return list_dirs(dir, true, true);
    }

    /**
     * Lists all files inside this directory that have the specified extension (default: all files)
     * @note By default: not recursive
     * @param out Destination vector for result file names.
     * @param dir Relative or full path of this directory
     * @param suffix Filter files by suffix, ex: ".txt" or "_mask.jpg", default ("") lists all files
     * @param recursive (default: false) If true, will perform a recursive search
     * @param fullpath (default: false) If true, returned paths will be fullpaths instead of relative
     * @return Number of files found that match the extension
     * Example:
     * @code
     *     vector<string> relativePaths          = list_files(folder);
     *     vector<string> recursiveRelativePaths = list_files_recursive(folder);
     * @endcode
     * @example [recursive=false] [fullpath=false] vector<string> {
     *     "main.cpp",
     *     "file_io.h",
     *     "file_io.cpp",
     * };
     * @example [recursive=false] [fullpath=true] vector<string> {
     *     "/projects/ReCpp/main.cpp",
     *     "/projects/ReCpp/file_io.h",
     *     "/projects/ReCpp/file_io.cpp",
     * };
     * @example [recursive=true] [fullpath=false] vector<string> {
     *     "main.cpp",
     *     "file_io.h",
     *     "file_io.cpp",
     *     "mesh/mesh.h",
     *     "mesh/mesh.cpp",
     *     "mesh/mesh_obj.cpp",
     *     "mesh/mesh_fbx.cpp",
     * };
     * @example [recursive=true] [fullpath=true] vector<string> {
     *     "/projects/ReCpp/main.cpp",
     *     "/projects/ReCpp/file_io.h",
     *     "/projects/ReCpp/file_io.cpp",
     *     "/projects/ReCpp/mesh/mesh.h",
     *     "/projects/ReCpp/mesh/mesh.cpp",
     *     "/projects/ReCpp/mesh/mesh_obj.cpp",
     *     "/projects/ReCpp/mesh/mesh_fbx.cpp",
     * };
     */
    RPPAPI int list_files(vector<string>& out, strview dir, strview suffix = {}, bool recursive = false, bool fullpath = false) noexcept;
    inline vector<string> list_files(strview dir, strview suffix = {}, bool recursive = false, bool fullpath = false) noexcept {
        vector<string> out; list_files(out, dir, suffix, recursive, fullpath); return out;
    }

    inline int list_files_fullpath(vector<string>& out, strview dir, strview suffix = {}, bool recursive = false) noexcept {
        return list_files(out, dir, suffix, recursive, true);
    }
    inline int list_files_recursive(vector<string>& out, strview dir, strview suffix = {}) noexcept {
        return list_files(out, dir, suffix, true, false);
    }
    inline int list_files_fullpath_recursive(vector<string>& out, strview dir, strview suffix = {}) noexcept {
        return list_files(out, dir, suffix, true, true);
    }

    inline vector<string> list_files_fullpath(strview dir, strview suffix = {}) noexcept {
        return list_files(dir, suffix, false, true);
    }
    inline vector<string> list_files_recursive(strview dir, strview suffix = {}) noexcept {
        return list_files(dir, suffix, true, false);
    }
    inline vector<string> list_files_fullpath_recursive(strview dir, strview suffix = {}) noexcept {
        return list_files(dir, suffix, true, true);
    }

    /**
     * Lists all files and folders inside a dir
     * @note By default: not recursive
     * @param outDirs All found directories relative to input dir
     * @param outFiles All found files
     * @param dir Directory to search in
     * @param recursive (default: false) If true, will perform a recursive search
     * @param fullpath (default: false) If true, returned paths will be fullpaths instead of relative
     * @return Number of files and folders found that match the extension
     * Example:
     * @code
     *     string dir = "C:/Projects/ReCpp";
     *     vector<string> dirs, files;
     *     list_alldir(dirs, files, dir, true, false); // recursive, relative paths
     *     // dirs:  { ".git", ".git/logs", ... }
     *     // files: { ".git/config", ..., ".gitignore", ... }
     * @endcode
     */
    RPPAPI int list_alldir(vector<string>& outDirs, vector<string>& outFiles, strview dir,
                    bool recursive = false, bool fullpath = false) noexcept;


    /**
     * Recursively lists all files under this directory and its subdirectories 
     * that match the list of suffixex
     * @param dir Relative or full path of root directory
     * @param suffixes Filter files by suffixes, ex: {"txt","_old.cfg","_mask.jpg"}
     * @param recursive [false] If true, the listing is done recursively
     * @param fullpath  [false] If true, full paths will be resolved
     * @return vector of resulting relative file paths
     */
    RPPAPI vector<string> list_files(strview dir, const vector<strview>& suffixes, bool recursive = false , bool fullpath = false) noexcept;
    inline vector<string> list_files_recursive(strview dir, const vector<strview>& suffixes, bool fullpath = false) noexcept {
        return list_files(dir, suffixes, true, fullpath);
    }

    /**
     * @return The current working directory of the application
     * @example "/Projects/ReCpp"
     * @example "C:\\Projects\\ReCpp"
     */
    RPPAPI string working_dir() noexcept;

    /**
     * Calls chdir() to set the working directory of the application to a new value
     * @return TRUE if chdir() is successful
     */
    RPPAPI bool change_dir(const char* new_wd) noexcept;
    inline bool change_dir(const string& new_wd) noexcept { return change_dir(new_wd.c_str()); }
    inline bool change_dir(const strview new_wd) noexcept { return change_dir(new_wd.to_cstr()); }

    /**
     * @return The system temporary directory for storing misc files
     * @note For windows this is: %USERPROFILE%/AppData/Local/Temp
     * @note for iOS this is $TMPDIR: %APPSTORAGE%/tmp
     */
    RPPAPI string temp_dir() noexcept;
    
    /**
     * @return The system home directory for this user
     * @note ENV $HOME is used
     */
    RPPAPI string home_dir() noexcept;

    ////////////////////////////////////////////////////////////////////////////////


} // namespace rpp
