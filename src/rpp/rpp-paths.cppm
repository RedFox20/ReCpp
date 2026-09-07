// C++20 module interface unit for <rpp/paths.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "paths.h"

export module rpp.paths;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.config;
export import rpp.sprint;
export import rpp.strview;

export namespace rpp {
    using rpp::file_exists;
    using rpp::is_symlink;
    using rpp::folder_exists;
    using rpp::file_or_folder_exists;
    using rpp::create_symlink;
    using rpp::file_info;
    using rpp::file_size;
    using rpp::file_sizel;
    using rpp::file_created;
    using rpp::file_accessed;
    using rpp::file_modified;
    using rpp::delete_file;
    using rpp::rename_file;
    using rpp::move_file;
    using rpp::copy_file;
    using rpp::copy_file_mode;
    using rpp::copy_file_if_needed;
    using rpp::copy_file_into_folder;
    using rpp::create_folder;
    using rpp::delete_mode;
    using rpp::delete_folder;
    using rpp::full_path;
    using rpp::merge_dirups;
    using rpp::file_name;
    using rpp::file_nameext;
    using rpp::file_ext;
    using rpp::file_replace_ext;
    using rpp::file_name_append;
    using rpp::file_name_replace;
    using rpp::file_nameext_replace;
    using rpp::folder_name;
    using rpp::folder_path;
    using rpp::normalize;
    using rpp::normalized;
    using rpp::path_combine;
    using rpp::string_list;
    using rpp::dir_iter_base;
    using rpp::directory_iter;
    using rpp::directory_entry;
    using rpp::dir_iterator;
    using rpp::dir_entry;
    using rpp::list_dir_flags;
    using rpp::dir_current;
    using rpp::dir_relpath;
    using rpp::dir_relpath_current;
    using rpp::dir_recursive;
    using rpp::dir_relpath_recursive;
    using rpp::dir_fullpath;
    using rpp::dir_fullpath_recursive;
    using rpp::dir_relpath_combine;
    using rpp::dir_relpath_combine_recursive;
    using rpp::operator|;
    using rpp::list_dirs;
    using rpp::list_files;
    using rpp::list_alldir;
    using rpp::working_dir;
    using rpp::module_dir;
    using rpp::module_path;
    using rpp::change_dir;
    using rpp::temp_dir;
    using rpp::home_dir;
#if RPP_ENABLE_UNICODE
    using rpp::ustring_list;
    using rpp::udir_iterator;
    using rpp::udir_entry;
    using rpp::working_diru;
    using rpp::temp_diru;
    using rpp::home_diru;
#endif
}
// GENERATED EXPORTS END
