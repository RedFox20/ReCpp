#pragma once
/**
 * Cross platform file utilities, Copyright (c) 2014-2018, Jorma Rebane
 * Distributed under MIT Software License
 *
 * @note This module predates C++17 filesystem and offers a different
 *       set of convenient API's for dealing with every-day File I/O Tasks.
 */
#if _MSC_VER
#  pragma warning(disable: 4251)
#endif
#include "config.h"
#include "strview.h"
#include "sprint.h"
#include "paths.h" // included for backwards compatibility
#include <ctime> // time_t
#include <vector>

namespace rpp /* ReCpp */
{
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
     */
    struct RPPAPI file
    {
        enum class mode {
            READONLY,   // opens an existing file for reading
            READWRITE,  // opens or creates file for read/write
            CREATENEW,  // always creates (overwrite) a new file for read/write
            APPEND,     // opens or creates file for appending only, seek operations will not work
        };

        static constexpr mode READONLY  = mode::READONLY;  // opens an existing file for reading
        static constexpr mode READWRITE = mode::READWRITE; // opens or creates file for read/write
        static constexpr mode CREATENEW = mode::CREATENEW; // always creates (overwrite) a new file for read/write
        static constexpr mode APPEND    = mode::APPEND;    // opens or creates file for appending only, seek operations will not work

        void* Handle; // File handle
        mode  Mode;	  // File open mode READWRITE or READONLY

        file() noexcept : Handle{nullptr}, Mode{READONLY} {}

        /**
         * Opens an existing file for reading with mode = READONLY
         * Creates a new file for reading/writing with mode = READWRITE
         * @param filename File name to open or create
         * @param mode File open mode
         */
        file(strview filename, mode mode = READONLY) noexcept;
    #if RPP_ENABLE_UNICODE
        file(ustrview filename, mode mode = READONLY) noexcept;
    #endif // RPP_ENABLE_UNICODE
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
        bool open(strview filename, mode mode = READONLY) noexcept;
    #if RPP_ENABLE_UNICODE
        bool open(ustrview filename, mode mode = READONLY) noexcept;
    #endif // RPP_ENABLE_UNICODE
        void close() noexcept;

        /**
         * @return TRUE if file handle is valid (file exists or has been created)
         */
        bool good() const noexcept;
        explicit operator bool() const noexcept { return good(); }
        bool is_open() const noexcept { return good(); }

        /**
         * @returns The underlying OS file handle, Linux fd or Windows HANDLE
         */
        int os_handle() const noexcept;

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
        int64 sizel() const noexcept; // NOLINT

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
         * Reads a POD struct sequentially
         * @return Number of bytes actually read from the file
         */
        template<class POD> int read(POD* plainOldDataObject) noexcept
        {
            return read(plainOldDataObject, sizeof(POD));
        }

        /**
         * Reads the entire contents of the file into a load_buffer
         * unbuffered_file is used internally
         */
        load_buffer read_all() noexcept;

        /**
         * Reads contents from current seek pos until end as std::string
         */
        std::string read_text() noexcept;
        
        /**
         * Save all contents to a new file.
         * @return TRUE if entire file was copied successfully
         */
        bool save_as(strview filename) noexcept;

        /**
         * Reads the entire contents of the file into a load_buffer
         * The file is opened as READONLY, unbuffered IO is preferred internally
         */
        static load_buffer read_all(strview filename)  noexcept
        {
            return file{ filename, READONLY }.read_all();
        }
    #if RPP_ENABLE_UNICODE
        static load_buffer read_all(ustrview filename) noexcept
        {
            return file{ filename, READONLY }.read_all();
        }
    #endif // RPP_ENABLE_UNICODE

        /**
         * Reads the entire contents of the file into a std::string
         * The file is opened as READONLY, unbuffered IO is preferred internally
         */
        static std::string read_all_text(strview filename) noexcept
        {
            return file{ filename, READONLY }.read_text();
        }
    #if RPP_ENABLE_UNICODE
        static std::string read_all_text(ustrview filename) noexcept
        {
            return file{ filename, READONLY }.read_text();
        }
    #endif

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
        template<class T, int N> int write(const T (&items)[N]) noexcept {
            return write(items, int(sizeof(T)) * N);
        }
        int write(strview str) noexcept {
            return write(str.str, str.len);
        }
    #if RPP_ENABLE_UNICODE
        int write(ustrview str) noexcept {
            return write(str.str, int(sizeof(ustrview::char_t) * str.len));
        }
    #endif // RPP_ENABLE_UNICODE
        template<typename Char>
        int write(const std::basic_string<Char>& str) noexcept {
            return write(str.data(), int(sizeof(Char) * str.size()));
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
        int writef(PRINTF_FMTSTR const char* format, ...) noexcept PRINTF_CHECKFMT2; // NOLINT

        /**
         * Writes a string to file and also writes a newline
         */
        int writeln(strview str) noexcept
        {
            return write(str.str, str.len)
                 + write("\n", 1);
        }
    #if RPP_ENABLE_UNICODE
        int writeln(ustrview str) noexcept
        {
            return write(str.str, int(sizeof(ustrview::char_t) * str.len))
                 + write(u"\n", sizeof(ustrview::char_t));
        }
    #endif // RPP_ENABLE_UNICODE

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
        template<class T, class... Args>
        int writeln(const T& first, const Args&... args) noexcept
        {
            string_buffer buf;
            buf.writeln(first, args...);
            return write(buf.ptr, buf.len);
        }
#endif

        /**
         * Truncate front part of the file
         * @param newLength The new length of the file, with any data in the front discarded
         */
        void truncate_front(int64 newLength) noexcept;

        /**
         * Truncate end of file
         * @param newLength The new length of the file, with any data in the end truncated
         */
        void truncate_end(int64 newLength) noexcept;

        /**
         * Calls OS truncate. This can be used to resize files. Behaviour depends on platform.
         * @param newLength The new length of the file
         */
        void truncate(int64 newLength) noexcept;

        /**
         * Preallocates a file and seeks to the given position
         * @param preallocSize Size to preallocate in bytes
         * @param seekPos Position to seek to after preallocation
         * @param seekMode Seek mode to use, default is SEEK_SET
         * @returns TRUE if preallocation was successful, FALSE if it failed
         */
        bool preallocate(int64 preallocSize, int64 seekPos = 0, int seekMode = SEEK_SET) noexcept;

        /**
         * Forcefully flushes any OS file buffers to send all data to the storage device
         * @warning Don't call this too haphazardly, or you will ruin your IO performance!
         */
        void flush() noexcept;

        /**
         * Creates a new file and fills it with the provided data.
         * Regular Windows IO buffering is ENABLED for WRITE.
         *
         * Openmode is mode::CREATENEW
         *
         * @param filename Name of the file to create and write to
         * @param buffer Buffer to write bytes from
         * @param bytesToWrite Number of bytes to write to the file
         * @return Number of bytes actually written to the file
         */
        static int write_new(strview filename, const void* buffer, int bytesToWrite) noexcept;

        static int write_new(strview filename, strview data) noexcept
        {
            return write_new(filename, data.str, data.len);
        }

        template<class T, class U>
        static int write_new(strview filename, const std::vector<T,U>& plainOldData) noexcept
        {
            return write_new(filename, plainOldData.data(), int(plainOldData.size()*sizeof(T)));
        }

    #if RPP_ENABLE_UNICODE
        static int write_new(ustrview filename, const void* buffer, int bytesToWrite) noexcept;

        static int write_new(ustrview filename, strview data) noexcept
        {
            return write_new(filename, data.str, data.len);
        }
        static int write_new(strview filename, ustrview data) noexcept
        {
            return write_new(filename, data.str, data.len * sizeof(char16_t));
        }
        static int write_new(ustrview filename, ustrview data) noexcept
        {
            return write_new(filename, data.str, data.len * sizeof(char16_t));
        }

        template<class T, class U>
        static int write_new(ustrview filename, const std::vector<T, U>& plainOldData) noexcept
        {
            return write_new(filename, plainOldData.data(), int(plainOldData.size() * sizeof(T)));
        }
    #endif // RPP_ENABLE_UNICODE

        /**
         * Seeks to the specified position in a file. Seekmode is
         * determined like in fseek: SEEK_SET, SEEK_CUR and SEEK_END
         *
         * @param filepos Position in file where to seek to
         * @param seekmode Seekmode to use: SEEK_SET, SEEK_CUR or SEEK_END
         * @return The new position in the file
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

        buffer_parser(load_buffer&& buf) noexcept : parser(buf.str, buf.len), dataOwner(std::move(buf)) {}

        // resets the parser state
        void reset() noexcept { parser::buffer = (strview)dataOwner; }

        explicit operator bool() const noexcept { return (bool)dataOwner; }
        static buffer_parser from_file(strview filename) noexcept {
            return { file::read_all(filename) };
        }
        static buffer_parser from_file(file& f) noexcept {
            return { f.read_all() };
        }
    };

    using buffer_line_parser    = buffer_parser<line_parser>;
    using buffer_bracket_parser = buffer_parser<bracket_parser>;
    using buffer_keyval_parser  = buffer_parser<keyval_parser>;

    ////////////////////////////////////////////////////////////////////////////////

} // namespace rpp
