// Includes <string> first, then imports every module which names std::string on its surface.
// gcc-14 writes a .gcm no such importer can read, and test_modules.cpp misses it, see BUGS.md B8.
#ifdef MAMA_HAS_MODULES
#include <string> // the include which makes gcc reconcile std::__cxx11 against the module

import rpp.strview; // includes come first, the imports go last
import rpp.sprint;
import rpp.paths;
import rpp.file_io;
import rpp.sockets;
import rpp.tests;
import rpp.binary_stream;
import rpp.binary_serializer;
import rpp.thread_pool;

static std::string module_trace() { return "trace"; }

int main()
{
    std::string s = rpp::to_string(42);
    rpp::binary_buffer buf;
    buf << std::string{"nine"}; // the module names std::string on this operator
    rpp::pool_trace_provider trace = &module_trace; // and on this alias
    bool ok = s == "42"
           && rpp::file_ext("a/b.txt") == "txt"
           && rpp::ipaddress{"127.0.0.1:80"}.port() == 80
           && rpp::file{"this_file_does_not_exist"}.bad()
           && buf.read_string() == "nine"
           && trace() == "trace";
    return ok ? 0 : 1;
}
#else
int main() { return 0; } // the header build does not exercise the module
#endif
