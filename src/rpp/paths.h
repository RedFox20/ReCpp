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
#include <ctime> // time_t
#include <vector>
#if __has_include("sprint.h")
#  include "sprint.h"
#endif

namespace rpp /* ReCpp */
{

    ////////////////////////////////////////////////////////////////////////////////


    /**
     * @return TRUE if the file exists, arg ex: "dir/file.ext"
     */
    RPPAPI bool file_exists(strview filename) noexcept;
    RPPAPI bool file_exists(ustrview filename) noexcept;

    /**
     * @return TRUE if the file is a symlink
     */
    RPPAPI bool is_symlink(strview filename) noexcept;
    RPPAPI bool is_symlink(ustrview filename) noexcept;

    /**
     * @return TRUE if the folder exists, arg ex: "root/dir" or "root/dir/"
     */
    RPPAPI bool folder_exists(strview folder) noexcept;
    RPPAPI bool folder_exists(ustrview filename) noexcept;

    /**
     * @return TRUE if either a file or a folder exists at the given path
     */
    RPPAPI bool file_or_folder_exists(strview fileOrFolder) noexcept;
    RPPAPI bool file_or_folder_exists(ustrview fileOrFolder) noexcept;

    /**
     * @brief Creates a symbolic link to a file or folder
     * @param target Destination where the link will point to
     * @param link   Name of the link to create
     */
    RPPAPI bool create_symlink(strview target, strview link) noexcept;
    RPPAPI bool create_symlink(ustrview target, ustrview link) noexcept;

    /**
     * @brief Gets basic information of a file
     * @param filename Name of the file, ex: "dir/file.ext"
     * @param filesize (optional) If not null, writes the long size of the file
     * @param created  (optional) If not null, writes the file creation date
     * @param accessed (optional) If not null, writes the last file access date
     * @param modified (optional) If not null, writes the last file modification date
     * @return TRUE if the file exists and required data was retrieved from the OS
     */
    RPPAPI bool file_info(strview filename, int64*  filesize, time_t* created,
                                            time_t* accessed, time_t* modified) noexcept;

    RPPAPI bool file_info(ustrview filename, int64*  filesize, time_t* created,
                                             time_t* accessed, time_t* modified) noexcept;

    /**
     * @brief Gets basic information of a file
     * @param fd File handle, either Winapi HANDLE or POSIX fd
     * @param filesize (optional) If not null, writes the long size of the file
     * @param created  (optional) If not null, writes the file creation date
     * @param accessed (optional) If not null, writes the last file access date
     * @param modified (optional) If not null, writes the last file modification date
     * @return TRUE if the file exists and required data was retrieved from the OS
     */
    RPPAPI bool file_info(intptr_t fd, int64*  filesize, time_t* created,
                                       time_t* accessed, time_t* modified) noexcept;

    /**
     * @return Short size of a file
     */
    RPPAPI int file_size(strview filename) noexcept;
    RPPAPI int file_size(ustrview filename) noexcept;

    /**
     * @return Long size of a file
     */
    RPPAPI int64 file_sizel(strview filename) noexcept;
    RPPAPI int64 file_sizel(ustrview filename) noexcept;

    /**
     * @return File creation date
     */
    RPPAPI time_t file_created(strview filename) noexcept;
    RPPAPI time_t file_created(ustrview filename) noexcept;

    /**
     * @return Last file access date
     */
    RPPAPI time_t file_accessed(strview filename) noexcept;
    RPPAPI time_t file_accessed(ustrview filename) noexcept;

    /**
     * @return Last file modification date
     */
    RPPAPI time_t file_modified(strview filename) noexcept;
    RPPAPI time_t file_modified(ustrview filename) noexcept;

    /**
     * @brief Deletes a single file, ex: "root/dir/file.ext"
     * @return TRUE if the file was actually deleted (can fail due to file locks or access rights)
     */
    RPPAPI bool delete_file(strview filename) noexcept;
    RPPAPI bool delete_file(ustrview filename) noexcept;

    /**
     * @brief Copies sourceFile to destinationFile, overwriting the previous file!
     * @note File access rights are also copied
     * @return TRUE if sourceFile was opened && destinationFile was created && copied successfully
     */
    RPPAPI bool copy_file(strview sourceFile, strview destinationFile) noexcept;
    RPPAPI bool copy_file(ustrview sourceFile, ustrview destinationFile) noexcept;

    /**
     * @brief Copies file access rights from sourceFile to destinationFile
     * @return TRUE if file access rights were copied successfully
     */
    RPPAPI bool copy_file_mode(strview sourceFile, strview destinationFile) noexcept;
    RPPAPI bool copy_file_mode(ustrview sourceFile, ustrview destinationFile) noexcept;

    /**
     * @brief Copies sourceFile to destinationFile IF destinationFile doesn't exist
     * @return TRUE if destinationFile exists or copy_file return true
     */
    RPPAPI bool copy_file_if_needed(strview sourceFile, strview destinationFile) noexcept;
    RPPAPI bool copy_file_if_needed(ustrview sourceFile, ustrview destinationFile) noexcept;

    /**
     * @brief Copies sourceFile into destinationFolder, overwriting the previous file!
     * @return TRUE if sourceFile was opened && destinationFolder was created && copied successfully
     */
    RPPAPI bool copy_file_into_folder(strview sourceFile, strview destinationFolder) noexcept;
    RPPAPI bool copy_file_into_folder(ustrview sourceFile, ustrview destinationFolder) noexcept;

    /**
     * Creates a folder, recursively creating folders that do not exist
     * @param foldername Relative or Absolute path
     * @return TRUE if the final folder was actually created (can fail due to access rights)
     */
    RPPAPI bool create_folder(strview foldername) noexcept;
    RPPAPI bool create_folder(ustrview foldername) noexcept;


    enum class delete_mode
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
    RPPAPI bool delete_folder(strview foldername, delete_mode mode = delete_mode::non_recursive) noexcept;
    RPPAPI bool delete_folder(ustrview foldername, delete_mode mode = delete_mode::non_recursive) noexcept;

    /**
     * @brief Resolves a relative path to a full path name using filesystem path resolution
     * @result "path" ==> "C:\Projects\Test\path" 
     */
    RPPAPI string full_path(strview path) noexcept;
    RPPAPI ustring full_path(ustrview path) noexcept;

    /**
     * @brief Merges all ../ inside of a path
     * @result  ../lib/../bin/file.txt ==> ../bin/file.txt
     */
    RPPAPI string merge_dirups(strview path) noexcept;
    RPPAPI ustring merge_dirups(ustrview path) noexcept;

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
    RPPAPI ustrview file_name(ustrview path) noexcept;

    /**
     * @brief Extract the file part (with ext) from a file path
     * @result /root/dir/file.ext ==> file.ext
     * @result /root/dir/file     ==> file
     * @result /root/dir/         ==> 
     * @result file.ext           ==> file.ext
     */
    RPPAPI strview file_nameext(strview path) noexcept;
    RPPAPI ustrview file_nameext(ustrview path) noexcept;

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
    RPPAPI ustrview file_ext(ustrview path) noexcept;

    /**
     * @brief Replaces the current file path extension
     *        Usage: file_replace_ext("dir/file.old", "new");
     * @result /dir/file.old ==> /dir/file.new
     * @result /dir/file     ==> /dir/file.new
     * @result /dir/         ==> /dir/
     * @result file.old      ==> file.new
     */
    RPPAPI string file_replace_ext(strview path, strview ext) noexcept;
    RPPAPI ustring file_replace_ext(ustrview path, ustrview ext) noexcept;
    
    /**
     * @brief Changes only the file name by appending a string, leaving directory and extension untouched
     * @result /dir/file.txt ==> /dir/fileadd.txt
     * @result /dir/file     ==> /dir/fileadd
     * @result /dir/         ==> /dir/
     * @result file.txt      ==> fileadd.txt
     */
    RPPAPI string file_name_append(strview path, strview add) noexcept;
    RPPAPI ustring file_name_append(ustrview path, ustrview add) noexcept;
    
    /**
     * @brief Replaces only the file name of the path, leaving directory and extension untouched
     * @result /dir/file.txt ==> /dir/replaced.txt
     * @result /dir/file     ==> /dir/replaced
     * @result /dir/         ==> /dir/
     * @result file.txt      ==> replaced.txt
     */
    RPPAPI string file_name_replace(strview path, strview newFileName) noexcept;
    RPPAPI ustring file_name_replace(ustrview path, ustrview newFileName) noexcept;
    
    /**
     * @brief Replaces the file name and extension, leaving directory untouched
     * @result /dir/file.txt ==> /dir/replaced.bin
     * @result /dir/file     ==> /dir/replaced
     * @result /dir/         ==> /dir/
     * @result file.txt      ==> replaced.bin
     */
    RPPAPI string file_nameext_replace(strview path, strview newFileNameAndExt) noexcept;
    RPPAPI ustring file_nameext_replace(ustrview path, ustrview newFileNameAndExt) noexcept;

    /**
     * @brief Extract the foldername from a path name
     * @result /root/dir/file.ext ==> dir
     * @result /root/dir/file     ==> dir
     * @result /root/dir/         ==> dir
     * @result dir/               ==> dir
     * @result file.ext           ==> 
     */
    RPPAPI strview folder_name(strview path) noexcept;
    RPPAPI ustrview folder_name(ustrview path) noexcept;

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
    RPPAPI ustrview folder_path(ustrview path) noexcept;

    /**
     * @brief Normalizes the path string to use a specific type of slash
     * @note This does not perform full path expansion.
     * @note The string is modified in-place !careful!
     *
     * @result \root/dir\\file.ext ==> /root/dir/file.ext
     */
    RPPAPI string& normalize(string& path, char sep = '/') noexcept;
    RPPAPI char*   normalize(char* path, char sep = '/') noexcept;
    RPPAPI ustring&  normalize(ustring& path, char16_t sep = u'/') noexcept;
    RPPAPI char16_t* normalize(char16_t* path, char16_t sep = u'/') noexcept;

    /**
     * @brief Normalizes the path string to use a specific type of slash
     * @note A copy of the string is made
     */
    RPPAPI string normalized(strview path, char sep = '/') noexcept;
    RPPAPI ustring normalized(ustrview path, char16_t sep = u'/') noexcept;
    
    /**
     * @brief Efficiently combines path strings, removing any repeated / or \
     * @result path_combine("tmp", "file.txt")   ==> "tmp/file.txt"
     * @result path_combine("tmp/", "file.txt")  ==> "tmp/file.txt"
     * @result path_combine("tmp/", "/file.txt") ==> "tmp/file.txt"
     * @result path_combine("tmp/", "/folder//") ==> "tmp/folder"
     */
    RPPAPI string path_combine(strview path1, strview path2) noexcept;
    RPPAPI string path_combine(strview path1, strview path2, strview path3) noexcept;
    RPPAPI string path_combine(strview path1, strview path2, strview path3, strview path4) noexcept;

#if RPP_ENABLE_UNICODE
    /**
     * @brief Efficiently combines two WideChar path strings, removing any repeated / or \
     */
    RPPAPI ustring path_combine(ustrview path1, ustrview path2) noexcept;
    RPPAPI ustring path_combine(ustrview path1, ustrview path2, ustrview path3) noexcept;
    RPPAPI ustring path_combine(ustrview path1, ustrview path2, ustrview path3, ustrview path4) noexcept;
#endif
    ////////////////////////////////////////////////////////////////////////////////

    using string_list = std::vector<string>;
    using ustring_list = std::vector<ustring>;

    // common base class which doesn't rely on specific string type
    class RPPAPI dir_iter_base
    {
    public:
        struct impl;
        struct state {
            #if _WIN32
                void* hFind;
                char ffd[592]; // WIN32_FIND_DATAW
            #else
                void* d;
                void* e;
                string dirname;
            #endif
            impl* operator->() noexcept;
            const impl* operator->() const noexcept;
        };
        state s; // iterator state

		dir_iter_base(strview dir) noexcept;
        dir_iter_base(ustrview dir) noexcept;
        ~dir_iter_base() noexcept;

        explicit operator bool() const noexcept;
        bool next() noexcept; // find next directory entry
        bool is_dir() const noexcept; // if current entry is a dir
        bool is_file() const noexcept; // if current entry is a file
        bool is_symlink() const noexcept; // if current entry is a symbolic link
        bool is_device() const noexcept; // if current entry is a block device, character device, named pipe or socket

        // gets specific type of string, performing necessary string conversions
        template<StringViewType T>
        struct name_util
        {
            static typename T::string_t get(const dir_iter_base* it) noexcept;
        };
    };

    /**
     * Basic and minimal directory iterator.
     * @note This is not recursive!
     * Example usage:
     * @code
     *     for (dir_entry e : dir_iterator { dir })
     *         if (e.is_dir) e.add_path_to(out);
     * @endcode
     */
    template<StringViewType T>
    class RPPAPI directory_iter : public dir_iter_base
    {
    public:
        using view_t = T;
		using char_t = typename T::char_t;
        using string_t = typename T::string_t;

    private:
        const string_t dir;       // original path used to construct this dir_iterator
        mutable string_t fulldir; // full path to directory we're iterating

    public:
        explicit directory_iter(view_t dir) noexcept
            : dir_iter_base{dir}
            , dir{dir.to_string()}
        {
        }
        explicit directory_iter(const string_t& dir) noexcept
            : dir_iter_base{dir}
            , dir{dir}
        {
        }
        explicit directory_iter(string_t&& dir) noexcept
            : dir_iter_base{view_t{dir}}
            , dir{std::move(dir)}
        {
        }
        ~directory_iter() noexcept = default;

        directory_iter(directory_iter&& it) = delete;
        directory_iter(const directory_iter&) = delete;
        directory_iter& operator=(directory_iter&& it) = delete;
        directory_iter& operator=(const directory_iter&) = delete;

        struct entry
        {
            const directory_iter* it;
            const string_t name; // name of the entry, file, dir, etc.

            entry(const directory_iter* it) noexcept
                : it{it}, name{it->name()}
            {}

            /** @returns "{input_dir}/{entry.name}" */
            string_t path() const noexcept { return rpp::path_combine(it->path(), name); }

            /** @returns absolute path to "{input_dir}/{entry.name}" */
            string_t full_path() const noexcept { return rpp::path_combine(it->full_path(), name); }

            /** @returns TRUE if current iterator state points to a valid FILE or DIR */
            bool is_valid() const noexcept { return (is_file() || is_dir()); }

            /** @returns TRUE if this is a directory */
            bool is_dir() const noexcept { return it->is_dir(); }
            /** @returns TRUE if this is a regular file */
            bool is_file() const noexcept { return it->is_file(); }
            /** @returns TRUE if this is a symbolic link */
            bool is_symlink() const noexcept { return it->is_symlink(); }
            /** @returns TRUE if this is a block device, character device, named pipe or socket */
            bool is_device() const noexcept { return it->is_device(); }

            bool add_path_to(std::vector<string_t>& out) const noexcept {
                if (is_valid()) {
                    out.emplace_back(path());
                    return true;
                }
                return false;
            }
            bool add_full_path_to(std::vector<string_t>& out) const noexcept {
                if (is_valid()) {
                    out.emplace_back(full_path());
                    return true;
                }
                return false;
            }
        };

        struct iter { // bare minimum for basic looping
            directory_iter* it;
            bool operator==(const iter& other) const noexcept { return it == other.it; }
            bool operator!=(const iter& other) const noexcept { return it != other.it; }
            entry operator*() const noexcept { return { it }; }
            iter& operator++() noexcept {
                if (!it->next()) it = nullptr;
                return *this;
            }
        };

        entry current() const noexcept { return { this }; }
        iter begin() noexcept { return { this->operator bool() ? this : nullptr }; }
        iter end() const noexcept { return { nullptr }; }

        /** @returns Current entry name, can be file, dir, etc */
        string_t name() const noexcept
        {
            return dir_iter_base::name_util<view_t>::get(this);
        }

        /** @returns original path used to construct this dir_iterator */
        const string_t& path() const noexcept { return dir; }

        /** @returns full path to directory we're iterating */
        const string_t& full_path() const noexcept
        {
            return fulldir.empty() ? (fulldir = rpp::full_path(dir)) : fulldir;
        }
	}; // directory_iter<T>

    template<StringViewType T>
    using directory_entry = typename directory_iter<T>::entry;

	// ascii version of the directory iterator
    using dir_iterator = directory_iter<strview>;
    using dir_entry    = directory_entry<strview>;

	// unicode version of the directory iterator
    using udir_iterator = directory_iter<ustrview>;
    using udir_entry    = directory_entry<ustrview>;

    ////////////////////////////////////////////////////////////////////////////////

    enum list_dir_flags
    {
        dir_current = 0, // default: only current directory
        dir_relpath = 0, // default: relative paths
        dir_relpath_current = 0,

        dir_recursive = (1 << 1), // recursively lists through directories
        dir_relpath_recursive = dir_relpath | dir_recursive, // recursive and relpaths

        dir_fullpath  = (1 << 2), // emits fullpath instead of relative paths
        dir_fullpath_recursive = dir_recursive | dir_fullpath, // both recursive and fullpath

        dir_relpath_combine = (1 << 3), // lists relpath, but outputs: path_combine(input_dir, relpath)
        dir_relpath_combine_recursive = dir_relpath_combine | dir_recursive,
    };

    inline list_dir_flags operator|(list_dir_flags a, list_dir_flags b) noexcept { return list_dir_flags(a | b); }

    /**
     * Lists all folders inside this directory
     * @param out Destination vector for result folder names (not full folder paths!)
     * @param dir Relative or full path of this directory
     * @param flags enum list_dir_flags:
     *        dir_recursive       - will perform a recursive search
     *        dir_fullpath        - returned paths will be fullpaths instead of relative
     *        dir_relpath_combine - combines input_dir with relpaths
     * @return Number of folders found
     * 
     * @code
     * @example [dir_recursive=false] [dir_fullpath=false] vector<string> {
     *     "bin",
     *     "src",
     *     "lib",
     * };
     * @example [dir_recursive=true] [dir_fullpath=false] vector<string> {
     *     "bin",
     *     "bin/data",
     *     "bin/data/models",
     *     "src",
     *     "src/mesh",
     *     "lib",
     * };
     * @example [dir_recursive=false] [dir_fullpath=true] vector<string> {
     *     "/projects/ReCpp/bin",
     *     "/projects/ReCpp/src",
     *     "/projects/ReCpp/lib",
     * };
     * @example [dir_recursive=true] [dir_fullpath=true] vector<string> {
     *     "/projects/ReCpp/bin",
     *     "/projects/ReCpp/bin/data",
     *     "/projects/ReCpp/bin/data/models",
     *     "/projects/ReCpp/src",
     *     "/projects/ReCpp/src/mesh",
     *     "/projects/ReCpp/lib",
     * };
     * @endcode
     */
    RPPAPI int list_dirs( string_list& out,  strview dir, list_dir_flags flags = {}) noexcept;
    RPPAPI int list_dirs(ustring_list& out, ustrview dir, list_dir_flags flags = {}) noexcept;
    inline string_list  list_dirs( strview dir, list_dir_flags flags = {}) noexcept {  string_list out; list_dirs(out, dir, flags); return out; }
    inline ustring_list list_dirs(ustrview dir, list_dir_flags flags = {}) noexcept { ustring_list out; list_dirs(out, dir, flags); return out; }


    /**
     * Lists all files inside this directory that have the specified extension (default: all files)
     * @param out Destination vector for result file names.
     * @param dir Relative or full path of this directory
     * @param suffix Filter files by suffix, ex: ".txt" or "_mask.jpg", default ("") lists all files
     * @param flags enum list_dir_flags:
     *        dir_recursive       - will perform a recursive search
     *        dir_fullpath        - returned paths will be fullpaths instead of relative
     *        dir_relpath_combine - combines input_dir with relpaths
     * @return Number of files found that match the extension
     * Example:
     * @code
     *     vector<string> relativePaths          = list_files(folder);
     *     vector<string> recursiveRelativePaths = list_files_recursive(folder);
     * @endcode
     * @code
     * @example [dir_recursive=false] [dir_fullpath=false] vector<string> {
     *     "main.cpp",
     *     "file_io.h",
     *     "file_io.cpp",
     * };
     * @example [dir_recursive=false] [dir_fullpath=true] vector<string> {
     *     "/projects/ReCpp/main.cpp",
     *     "/projects/ReCpp/file_io.h",
     *     "/projects/ReCpp/file_io.cpp",
     * };
     * @example [dir_recursive=true] [dir_fullpath=false] vector<string> {
     *     "main.cpp",
     *     "file_io.h",
     *     "file_io.cpp",
     *     "mesh/mesh.h",
     *     "mesh/mesh.cpp",
     *     "mesh/mesh_obj.cpp",
     *     "mesh/mesh_fbx.cpp",
     * };
     * @example [dir_recursive=true] [dir_fullpath=true] vector<string> {
     *     "/projects/ReCpp/main.cpp",
     *     "/projects/ReCpp/file_io.h",
     *     "/projects/ReCpp/file_io.cpp",
     *     "/projects/ReCpp/mesh/mesh.h",
     *     "/projects/ReCpp/mesh/mesh.cpp",
     *     "/projects/ReCpp/mesh/mesh_obj.cpp",
     *     "/projects/ReCpp/mesh/mesh_fbx.cpp",
     * };
     * @endcode
     */
    RPPAPI int list_files( string_list& out,  strview dir,  strview suffix, list_dir_flags flags = {}) noexcept;
    RPPAPI int list_files(ustring_list& out, ustrview dir, ustrview suffix, list_dir_flags flags = {}) noexcept;
    inline string_list  list_files( strview dir,  strview suffix, list_dir_flags flags = {}) noexcept {  string_list out; list_files(out, dir, suffix, flags); return out; }
    inline ustring_list list_files(ustrview dir, ustrview suffix, list_dir_flags flags = {}) noexcept { ustring_list out; list_files(out, dir, suffix, flags); return out; }


    /**
     * Recursively lists all files under this directory and its subdirectories that match the list of suffixes.
     * @param dir Relative or full path of root directory
     * @param suffixes Filter files by suffixes, ex: {"txt","_old.cfg","_mask.jpg"}
     * @param flags enum list_dir_flags:
     *        dir_recursive       - will perform a recursive search
     *        dir_fullpath        - returned paths will be fullpaths instead of relative
     *        dir_relpath_combine - combines input_dir with relpaths
     * @return vector of resulting relative file paths
     */
    RPPAPI int list_files(string_list&  out,  strview dir, const std::vector<strview>&  suffixes, list_dir_flags flags = {}) noexcept;
    RPPAPI int list_files(ustring_list& out, ustrview dir, const std::vector<ustrview>& suffixes, list_dir_flags flags = {}) noexcept;
    inline string_list  list_files( strview dir, const std::vector<strview>&  suffixes, list_dir_flags flags = {}) noexcept { string_list out;  list_files(out, dir, suffixes, flags); return out; }
    inline ustring_list list_files(ustrview dir, const std::vector<ustrview>& suffixes, list_dir_flags flags = {}) noexcept { ustring_list out; list_files(out, dir, suffixes, flags); return out; }


    /**
     * Lists all files and folders inside a dir.
     * @param outDirs All found directories relative to input dir
     * @param outFiles All found files
     * @param dir Directory to search in
     * @param flags enum list_dir_flags:
     *        dir_recursive       - will perform a recursive search
     *        dir_fullpath        - returned paths will be fullpaths instead of relative
     *        dir_relpath_combine - combines input_dir with relpaths
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
    RPPAPI int list_alldir( string_list& outDirs,  string_list& outFiles,  strview dir, list_dir_flags flags = {}) noexcept;
    RPPAPI int list_alldir(ustring_list& outDirs, ustring_list& outFiles, ustrview dir, list_dir_flags flags = {}) noexcept;


    /**
     * @return The current working directory of the application.
     *         An extra slash is always appended.
     *         Path is always normalized to forward slashes /
     * @example Linux:   "/home/jorma/Projects/ReCpp/"
     * @example Windows: "C:/Projects/ReCpp/"
     */
    RPPAPI string working_dir() noexcept;
    inline ustring working_diru() noexcept { return to_ustring(working_dir()); }


    /**
     * @return Directory where the current module in which Rpp was linked to is located.
     *         For DLL-s which link RPP statically, this is the folder where the DLL is located.
     * @param moduleObject [null] If RPP is a DLL then a platform specific
     *                            object must be passed for identification to work.
     *                            This is only if the dynamic module is not in the same binary
     *                            as RPP, such as external dynamic libaries.
     * 
     * An extra slash is always appended.
     * Path is always normalized to forward slashes /
     * 
     * @code
     * string modulePath = rpp::module_dir(); //   "/path/to/project/bin/"
     * @endcode
     * 
     * Specifying external dynamic modules:
     * @code
     * // macOS/iOS
     * string modulePath = rpp::module_dir(MyLibraryObject.class);
     * 
     * // Win32
     * string modulePath = rpp::module_dir(GetModuleHandle("fbxsdk"));
     * @endcode
     */
    RPPAPI string module_dir(void* moduleObject = nullptr) noexcept;


    /**
     * Identical to `module_dir` but also includes the DLL/EXE name,
     * resulting in a full path to enclosing binary.
     * Path is always normalized to forward slashes /
     */
    RPPAPI string module_path(void* moduleObject = nullptr) noexcept;


    /**
     * Calls chdir() to set the working directory of the application to a new value
     * @return TRUE if chdir() is successful
     */
    RPPAPI bool change_dir(strview new_wd) noexcept;
    RPPAPI bool change_dir(ustrview new_wd) noexcept;

    /**
     * @return The system temporary directory for storing misc files
     * @note For windows this is: %USERPROFILE%/AppData/Local/Temp/
     * @note for iOS this is $TMPDIR: %APPSTORAGE%/tmp/
     * A trailing slash is always appended to the path
     * Path is always normalized to forward slashes /
     */
    RPPAPI string temp_dir() noexcept;
    inline ustring temp_diru() noexcept { return to_ustring(temp_dir()); }
    
    /**
     * @return The system home directory for this user
     * @note ENV $HOME is used
     * A trailing slash is always appended to the path.
     * Path is always normalized to forward slashes /
     */
    RPPAPI string home_dir() noexcept;
    RPPAPI ustring home_diru() noexcept;

    ////////////////////////////////////////////////////////////////////////////////

} // namespace rpp
