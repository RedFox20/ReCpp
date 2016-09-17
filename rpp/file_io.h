#pragma once
#include <string>
#include <vector>
#include <time.h> // time_t
#include "strview.h"

namespace rpp /* ReCpp */
{
    using namespace std; // we love std; you should too.

    enum IOFlags {
        READONLY,			// opens an existing file for reading
        READWRITE,			// opens an existing file for read/write
        CREATENEW,			// creates new file for writing
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
        char* ptr;  // dynamic or internal buffer
        int   len;    // buffer size in bytes

        load_buffer() : ptr(0), len(0) {}
        // takes ownership of a malloc-ed pointer and frees it when out of scope
        load_buffer(char* buffer, int size) : ptr(buffer), len(size) {}
        ~load_buffer();

        load_buffer(const load_buffer& rhs)            = delete; // NOCOPY
        load_buffer& operator=(const load_buffer& rhs) = delete; // NOCOPY

        load_buffer(load_buffer&& mv);
        load_buffer& operator=(load_buffer&& mv);

        // acquire the data pointer of this load_buffer, making the caller own the buffer
        char* steal_ptr();

        template<class T> operator T*() { return (T*)ptr; }
        int size()      const { return len; }
        char* data()    const { return ptr; }
        operator bool() const { return ptr != nullptr; }
    };


    /**
     * Stores a load buffer for line parsing
     */
    struct buffer_line_parser : public line_parser
    {
        load_buffer buf;
        buffer_line_parser(load_buffer&& buf) : line_parser(buf.ptr, buf.len), buf(move(buf))
        {
        }
        buffer_line_parser(const buffer_line_parser&) = delete; // NOCOPY
        buffer_line_parser& operator=(const buffer_line_parser&) = delete;
        operator bool() const { return (bool)buf; }
    };


    /**
     * Stores a load buffer for bracket parsing
     */
    struct buffer_bracket_parser : public bracket_parser
    {
        load_buffer buf;
        buffer_bracket_parser(load_buffer&& buf) : bracket_parser(buf.ptr, buf.len), buf(move(buf))
        {
        }
        buffer_bracket_parser(const buffer_bracket_parser&) = delete; // NOCOPY
        buffer_bracket_parser& operator=(const buffer_bracket_parser&) = delete;
        operator bool() const { return (bool)buf; }
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

        file() : Handle(0), Mode(READONLY)
        {
        }

        /**
         * Opens an existing file for reading with mode = READONLY
         * Creates a new file for reading/writing with mode = READWRITE
         * @param filename File name to open or create
         * @param mode File open mode
         */
        file(const char*   filename, IOFlags mode = READONLY);
        file(const string& filename, IOFlags mode = READONLY);
        file(const strview filename, IOFlags mode = READONLY);
        file(file&& f);
        ~file();

        file& operator=(file&& f);

        file(const file& f) = delete;
        file& operator=(const file& f) = delete;
    public:

        /**
         * Opens an existing file for reading with mode = READONLY
         * Creates a new file for reading/writing with mode = READWRITE
         * @param filename File name to open or create
         * @param mode File open mode
         * @return TRUE if file open/create succeeded, FALSE if failed
         */
        bool open(const char*   filename, IOFlags mode = READONLY);
        bool open(const string& filename, IOFlags mode = READONLY);
        bool open(const strview filename, IOFlags mode = READONLY);
        void close();

        /**
         * @return TRUE if file handle is valid (file exists or has been created)
         */
        bool good() const;
        operator bool() const { return good(); }

        /**
         * @return TRUE if the file handle is INVALID
         */
        bool bad() const;

        /**
         * @return Size of the file in bytes
         */
        int size() const;

        /**
         * @return 64-bit unsigned size of the file in bytes
         */
        uint64 sizel() const;

        /**
         * Reads a block of bytes from the file. Standard OS level
         * IO buffering is performed.
         *
         * @param buffer Buffer to read bytes to
         * @param bytesToRead Number of bytes to read from the file
         * @return Number of bytes actually read from the file
         */
        int read(void* buffer, int bytesToRead);

        /**
         * Reads the entire contents of the file into a load_buffer
         * unbuffered_file is used internally
         */
        load_buffer read_all();

        /**
         * Reads the entire contents of the file into a load_buffer
         * The file is opened as READONLY, unbuffered_file is used internally
         */
        static load_buffer read_all(const char*   filename);
        static load_buffer read_all(const string& filename);
        static load_buffer read_all(const strview filename);

        /**
         * Writes a block of bytes to the file. Regular Windows IO
         * buffering is ENABLED for WRITE.
         *
         * @param buffer Buffer to write bytes from
         * @param bytesToWrite Number of bytes to write to the file
         * @return Number of bytes actually written to the file
         */
        int write(const void* buffer, int bytesToWrite);

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
        static int write_new(const char*   filename, const void* buffer, int bytesToWrite);
        static int write_new(const string& filename, const void* buffer, int bytesToWrite);
        static int write_new(const strview filename, const void* buffer, int bytesToWrite);

        /**
         * Seeks to the specified position in a file. Seekmode is
         * determined like in fseek: SEEK_SET, SEEK_CUR and SEEK_END
         *
         * @param filepos Position in file where to seek to
         * @param seekmode Seekmode to use: SEEK_SET, SEEK_CUR or SEEK_END
         * @return Current position in the file
         */
        int seek(int filepos, int seekmode = SEEK_SET);
        uint64 seekl(uint64 filepos, int seekmode = SEEK_SET);

        /**
         * @return Current position in the file
         */
        int tell() const;

        /**
         * Get multiple time info from this file handle
         */
        void time_info(time_t* outCreated, time_t* outAccessed, time_t* outModified) const;

        /**
         * @return File creation time
         */
        time_t time_created() const;

        /**
         * @return File access time - when was this file last accessed?
         */
        time_t time_accessed() const;

        /**
         * @return File write time - when was this file last modified
         */
        time_t time_modified() const;
    };


    bool file_exists(const char*   filename);
    bool file_exists(const string& filename);
    bool file_exists(const strview filename);

    bool folder_exists(const char*   folder);
    bool folder_exists(const string& folder);
    bool folder_exists(const strview folder);

    void file_info(const char* filename, uint64_t* filesize, time_t* created, 
                                         time_t*   accessed, time_t* modified);

    uint file_size(const char*   filename);
    uint file_size(const string& filename);
    uint file_size(const strview filename);

    uint64_t file_sizel(const char*   filename);
    uint64_t file_sizel(const string& filename);
    uint64_t file_sizel(const strview filename);

    time_t file_created(const char*   filename);
    time_t file_created(const string& filename);
    time_t file_created(const strview filename);

    time_t file_accessed(const char*   filename);
    time_t file_accessed(const string& filename);
    time_t file_accessed(const strview filename);

    time_t file_modified(const char*   filename);
    time_t file_modified(const string& filename);
    time_t file_modified(const strview filename);

    bool delete_file(const char*   filename);
    bool delete_file(const string& filename);
    bool delete_file(const strview filename);

    bool create_folder(const char*   foldername);
    bool create_folder(const string& foldername);
    bool create_folder(const strview foldername);

    /**
     * Deletes a folder, by default only if it's empty.
     * @param recursiveDelete If TRUE, all subdirectories and files will also be deleted (permanently)
     * @return TRUE if the folder was deleted
     */
    bool delete_folder(const char*   foldername, bool recursiveDelete = false);
    bool delete_folder(const string& foldername, bool recursiveDelete = false);
    bool delete_folder(const strview foldername, bool recursiveDelete = false);


    /**
     * @brief Static container for directory utility functions
     */
    struct path
    {
        /**
         * Lists all folders inside this directory
         * @param out Destination vector for result folder names (not full folder paths!)
         * @param dir Relative or full path of this directory
         * @return Number of folders found
         */
        static int list_dirs(vector<string>& out, strview dir);

        /**
         * Lists all files inside this directory that have the specified extension (default: all files)
         * @param out Destination vector for result file names (not full file paths!)
         * @param dir Relative or full path of this directory
         * @param ext Filter files by extension, ex: "txt", default ("") lists all files
         * @return Number of files found that match the extension
         */
        static int list_files(vector<string>& out, strview dir, strview ext = {});

        /**
         * Lists all files and folders inside a dir
         */
        static int list_alldir(vector<string>& outdirs, vector<string>& outfiles, strview dir);

        /**
         * @return The current working directory of the application
         */
        static string working_dir();

        /**
         * @brief Set the working directory of the application to a new value
         */
        static void set_working_dir(const string& new_wd);

        /**
         *  @brief Transform a relative path to a full path name
         */
        static string fullpath(const string& relativePath);

        /**
         *  @brief Transform a relative path to a full path name
         */
        static string fullpath(const char* relativePath);

        /**
         * @brief Extract the filename from a path name
         */
        static string filename(const string& someFilePath);

        /**
         * @brief Extract the filename from a path name
         */
        static string filename(const char* someFilePath);

        /**
         * @brief Extract the filepart from a filename
         *        Ex: "/dir/file.ext" ==> "file"
         */
        static string filename_namepart(const string& someFilePath);

        /**
         * @brief Extract the filepart from a filename
         *        Ex: "/dir/file.ext" ==> "file"
         */
        static string filename_namepart(const char* someFilePath);

        /**
         * @brief Extract the foldername from a path name
         */
        static string foldername(const string& someFolderPath);

        /**
         * @brief Extract the foldername from a path name
         */
        static string foldername(const char* someFolderPath);

        /**
         * @brief Extracts the full folder path from a filePath
         *        Ex: "/full/dir/file.ext" ==> "/full/dir/"
         */
        static string folder_path(const strview filePath);

        /**
         * @brief Normalizes the path string to use a specific type of slash
         * @note This does not perform fullpath expansion.
         */
        static string& normalize(string& pathString, char separator = '/');
    };


} // namespace rpp