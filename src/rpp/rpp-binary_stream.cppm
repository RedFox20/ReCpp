// C++20 module interface unit for <rpp/binary_stream.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "binary_stream.h"

export module rpp.binary_stream;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::stream_source;
    using rpp::binary_stream;
    using rpp::operator<<;
    using rpp::endl;
    using rpp::operator>>;
    using rpp::binary_buffer;
    using rpp::socket_writer;
    using rpp::socket_reader;
    using rpp::file_writer;
    using rpp::file_reader;
}
// GENERATED EXPORTS END
