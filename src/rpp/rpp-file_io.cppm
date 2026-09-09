// C++20 module interface unit for <rpp/file_io.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "file_io.h"

export module rpp.file_io;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::load_buffer;
    using rpp::file;
    using rpp::buffer_parser;
    using rpp::buffer_line_parser;
    using rpp::buffer_bracket_parser;
    using rpp::buffer_keyval_parser;
}
// GENERATED EXPORTS END
