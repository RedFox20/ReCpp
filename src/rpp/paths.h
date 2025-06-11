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
    RPPAPI string merge_dirups(ustrview path) noexcept;

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
            impl* operator->() noexcept;
            const impl* operator->() const noexcept;
        };

        dummy s; // iterator state
        const string dir;       // original path used to construct this dir_iterator
        const string reldir;    // relative directory
        mutable string fulldir; // full path to directory we're iterating

    public:
        explicit dir_iterator(const strview& dir) noexcept : dir_iterator{ dir.to_string() } {}
        explicit dir_iterator(const string& dir) noexcept : dir_iterator{ string{dir} } {}
        explicit dir_iterator(string&& dir) noexcept;
        ~dir_iterator() noexcept;

        dir_iterator(dir_iterator&& it) = delete;
        dir_iterator(const dir_iterator&) = delete;
        dir_iterator& operator=(dir_iterator&& it) = delete;
        dir_iterator& operator=(const dir_iterator&) = delete;

        struct entry
        {
            const dir_iterator* it;
            const strview name;

            entry(const dir_iterator* it) noexcept : it{it}, name{it->name()}  {}
            string path() const noexcept { return path_combine(it->path(), name); }
            string full_path() const noexcept { return path_combine(it->full_path(), name); }

            /** @returns TRUE if this is a directory */
            bool is_dir() const noexcept { return it->is_dir(); }
            /** @returns TRUE if this is a regular file */
            bool is_file() const noexcept { return it->is_file(); }
            /** @returns TRUE if this is a symbolic link */
            bool is_symlink() const noexcept { return it->is_symlink(); }
            /** @returns TRUE if this is a block device, character device, named pipe or socket */
            bool is_device() const noexcept { return it->is_device(); }

            /** @returns TRUE if these are the special '.' or '..' dirs */
            bool is_special_dir() const noexcept { return name == "." || name == ".."; }

            // all files and directories that aren't "." or ".." are valid
            bool is_valid() const noexcept { return (is_file() || is_dir()) && !is_special_dir(); }

            // dirs that aren't "." or ".." are valid
            bool is_valid_dir() const noexcept { return is_dir() && !is_special_dir(); }

            bool add_path_to(std::vector<string>& out) const noexcept {
                if (is_valid()) {
                    out.emplace_back(path());
                    return true;
                }
                return false;
            }
            bool add_full_path_to(std::vector<string>& out) const noexcept {
                if (is_valid()) {
                    out.emplace_back(full_path());
                    return true;
                }
                return false;
            }
        };

        struct iter { // bare minimum for basic looping
            dir_iterator* it;
            bool operator==(const iter& other) const noexcept { return it == other.it; }
            bool operator!=(const iter& other) const noexcept { return it != other.it; }
            entry operator*() const noexcept { return { it }; }
            iter& operator++() noexcept {
                if (!it->next()) it = nullptr;
                return *this;
            }
        };

        explicit operator bool() const noexcept;
        bool next() noexcept; // find next directory entry
        bool is_dir() const noexcept; // if current entry is a dir
        bool is_file() const noexcept; // if current entry is a file
        bool is_symlink() const noexcept; // if current entry is a symbolic link
        bool is_device() const noexcept; // if current entry is a block device, character device, named pipe or socket
        strview name() const noexcept; // current entry name
        entry current() const noexcept { return { this }; }
        iter begin() noexcept { return { this }; }
        iter end() const noexcept { return { nullptr }; }

        // original path used to construct this dir_iterator
        const string& path() const noexcept { return dir; }

        // full path to directory we're iterating
        const string& full_path() const noexcept {
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
     * @code
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
     * @endcode
     */
    RPPAPI int list_dirs(std::vector<string>& out, strview dir, bool recursive = false, bool fullpath = false) noexcept;
    RPPAPI int list_dirs(std::vector<ustring>& out, ustrview dir, bool recursive = false, bool fullpath = false) noexcept;

    template<StringViewType T>
    inline auto list_dirs(T dir, bool recursive = false, bool fullpath = false) noexcept -> std::vector<typename T::string_t> {
        std::vector<typename T::string_t> out;
        list_dirs(out, dir, recursive, fullpath);
        return out;
    }

    template<StringViewType T>
    inline auto list_dirs_fullpath(T dir, bool recursive = false) noexcept -> std::vector<typename T::string_t> {
        return list_dirs(dir, recursive, true);
    }
    template<StringViewType T>
    inline auto list_dirs_fullpath_recursive(T dir) noexcept -> std::vector<typename T::string_t> {
        return list_dirs(dir, true, true);
    }


    /**
     * Lists all folders inside this directory as relative paths including input dir
     * @note By default: not recursive
     * @param out Destination vector for result folder names (not full folder paths!)
     * @param dir Relative or full path of this directory
     * @param recursive (default: false) If true, will perform a recursive search
     * @return Number of folders found
     * 
     * @code
     * @example [dir="test/ReCpp/"] [recursive=false] vector<string> {
     *     "test/ReCpp/bin",
     *     "test/ReCpp/src",
     *     "test/ReCpp/lib",
     * };
     * @example [dir="test/ReCpp/"] [recursive=true] vector<string> {
     *     "test/ReCpp/bin",
     *     "test/ReCpp/bin/data",
     *     "test/ReCpp/bin/data/models",
     *     "test/ReCpp/src",
     *     "test/ReCpp/src/mesh",
     *     "test/ReCpp/lib",
     * };
     * @endcode
     */
    RPPAPI int list_dirs_relpath(std::vector<string>& out, strview dir, bool recursive = false) noexcept;
    RPPAPI int list_dirs_relpath(std::vector<ustring>& out, ustrview dir, bool recursive = false) noexcept;

    template<StringViewType T>
    inline auto list_dirs_relpath(T dir, bool recursive = false) noexcept -> std::vector<typename T::string_t> {
        std::vector<string> out;
        list_dirs_relpath(out, dir, recursive);
        return out;
    }

    template<StringViewType T>
    inline auto list_dirs_relpath_recursive(T dir) noexcept -> std::vector<typename T::string_t> {
        return list_dirs_relpath<T>(dir, true);
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
     * @code
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
     * @endcode
     */
    RPPAPI int list_files(std::vector<string>& out, strview dir, strview suffix = {}, bool recursive = false, bool fullpath = false) noexcept;
    RPPAPI int list_files(std::vector<ustring>& out, ustrview dir, ustrview suffix = {}, bool recursive = false, bool fullpath = false) noexcept;

    template<StringViewType T>
    inline auto list_files(T dir, T suffix = {}, bool recursive = false, bool fullpath = false) noexcept -> std::vector<typename T::string_t> {
        std::vector<typename T::string_t> out;
        list_files(out, dir, suffix, recursive, fullpath);
        return out;
    }

    template<StringViewType T>
    inline auto list_files_fullpath(T dir, T suffix = {}) noexcept -> std::vector<typename T::string_t> {
        return list_files(dir, suffix, false, true);
    }
    template<StringViewType T>
    inline auto list_files_recursive(T dir, T suffix = {}) noexcept -> std::vector<typename T::string_t> {
        return list_files(dir, suffix, true, false);
    }
    template<StringViewType T>
    inline auto list_files_fullpath_recursive(T dir, T suffix = {}) noexcept -> std::vector<typename T::string_t> {
        return list_files(dir, suffix, true, true);
    }


    /**
     * Lists all files inside this directory that have the specified extension (default: all files)
     * as relative paths including input dir
     * @note By default: not recursive
     * @param out Destination vector for result folder names (not full folder paths!)
     * @param dir Relative or full path of this directory
     * @param suffix Filter files by suffix, ex: ".txt" or "_mask.jpg", default ("") lists all files
     * @param recursive (default: false) If true, will perform a recursive search
     * @return Number of files found that match the extension
     * @code
     *     vector<string> relativePaths          = list_files_relpath(folder);
     *     vector<string> recursiveRelativePaths = list_files_relpath_recursive(folder);
     * @endcode
     * @code
     * @example [dir="test/ReCpp/"] [recursive=false] vector<string> {
     *     "test/ReCpp/main.cpp",
     *     "test/ReCpp/file_io.h",
     *     "test/ReCpp/file_io.cpp",
     * };
     * @example [dir="test/ReCpp/"] [recursive=true] vector<string> {
     *     "test/ReCpp/main.cpp",
     *     "test/ReCpp/file_io.h",
     *     "test/ReCpp/file_io.cpp",
     *     "test/ReCpp/mesh/mesh.h",
     *     "test/ReCpp/mesh/mesh.cpp",
     *     "test/ReCpp/mesh/mesh_obj.cpp",
     *     "test/ReCpp/mesh/mesh_fbx.cpp",
     * };
     * @endcode
     */
    RPPAPI int list_files_relpath(std::vector<string>& out, strview dir, strview suffix = {}, bool recursive = false) noexcept;
    RPPAPI int list_files_relpath(std::vector<ustring>& out, ustrview dir, ustrview suffix = {}, bool recursive = false) noexcept;

    template<StringViewType T>
    inline auto list_files_relpath(T dir, T suffix = {}, bool recursive = false) noexcept -> std::vector<typename T::string_t> {
        std::vector<typename T::string_t> out;
        list_files_relpath(out, dir, suffix, recursive);
        return out;
    }

    template<StringViewType T>
    inline auto list_files_relpath_recursive(T dir, T suffix = {}) noexcept -> std::vector<typename T::string_t> {
        return list_files_relpath(dir, suffix, true);
    }


    /**
     * Recursively lists all files under this directory and its subdirectories 
     * that match the list of suffixex
     * @param dir Relative or full path of root directory
     * @param suffixes Filter files by suffixes, ex: {"txt","_old.cfg","_mask.jpg"}
     * @param recursive [false] If true, the listing is done recursively
     * @param fullpath  [false] If true, full paths will be resolved
     * @return vector of resulting relative file paths
     */
    RPPAPI std::vector<string> list_files(strview dir, const std::vector<strview>& suffixes, bool recursive = false, bool fullpath = false) noexcept;
    RPPAPI std::vector<ustring> list_files(ustrview dir, const std::vector<ustrview>& suffixes, bool recursive = false, bool fullpath = false) noexcept;

    template<StringViewType T>
    inline auto list_files_recursive(T dir, const std::vector<T>& suffixes, bool fullpath = false) noexcept -> std::vector<typename T::string_t> {
        return list_files(dir, suffixes, true, fullpath);
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
    RPPAPI int list_alldir(std::vector<string>& outDirs, std::vector<string>& outFiles, strview dir,
                           bool recursive = false, bool fullpath = false) noexcept;
    RPPAPI int list_alldir(std::vector<ustring>& outDirs, std::vector<ustring>& outFiles, ustrview dir,
                           bool recursive = false, bool fullpath = false) noexcept;

    /**
     * Lists all files and folders inside a dir as relative paths to input dir
     * @note By default: not recursive
     * @param outDirs All found directories relative to input dir
     * @param outFiles All found files
     * @param dir Directory to search in
     * @param recursive (default: false) If true, will perform a recursive search
     * @return Number of files and folders found that match the extension
     * Example:
     * @code
     *     string dir = "ReCpp";
     *     vector<string> dirs, files;
     *     list_alldir_relpath_recursive(dirs, files, dir);
     *     // dirs:  { "ReCpp/.git", "ReCpp/.git/logs", ... }
     *     // files: { "ReCpp/.git/config", ..., "ReCpp/.gitignore", ... }
     * @endcode
     */
    RPPAPI int list_alldir_relpath(std::vector<string>& outDirs, std::vector<string>& outFiles, strview dir, bool recursive = false) noexcept;
    RPPAPI int list_alldir_relpath(std::vector<ustring>& outDirs, std::vector<ustring>& outFiles, ustrview dir, bool recursive = false) noexcept;


    template<StringViewType T>
    inline int list_alldir_relpath_recursive(std::vector<typename T::string_t>& outDirs, std::vector<typename T::string_t>& outFiles, T dir) noexcept {
        return list_alldir_relpath(outDirs, outFiles, dir, true);
    }


    /**
     * @return The current working directory of the application.
     *         An extra slash is always appended.
     *         Path is always normalized to forward slashes /
     * @example Linux:   "/home/jorma/Projects/ReCpp/"
     * @example Windows: "C:/Projects/ReCpp/"
     */
    RPPAPI string working_dir() noexcept;


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
    RPPAPI bool change_dir(const char* new_wd) noexcept;
    inline bool change_dir(const string& new_wd) noexcept { return change_dir(new_wd.c_str()); }
    inline bool change_dir(const strview new_wd) noexcept { return change_dir(new_wd.to_cstr()); }

    /**
     * @return The system temporary directory for storing misc files
     * @note For windows this is: %USERPROFILE%/AppData/Local/Temp/
     * @note for iOS this is $TMPDIR: %APPSTORAGE%/tmp/
     * A trailing slash is always appended to the path
     * Path is always normalized to forward slashes /
     */
    RPPAPI string temp_dir() noexcept;
    inline ustring temp_dirw() noexcept { return to_ustring(temp_dir()); }
    
    /**
     * @return The system home directory for this user
     * @note ENV $HOME is used
     * A trailing slash is always appended to the path.
     * Path is always normalized to forward slashes /
     */
    RPPAPI string home_dir() noexcept;
    RPPAPI ustring home_dirw() noexcept;

    ////////////////////////////////////////////////////////////////////////////////

} // namespace rpp
