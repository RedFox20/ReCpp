// Imports rpp.config with no header, so the build fails if the module exports no aliases.
#ifdef MAMA_HAS_MODULES
import rpp.config;
int main() { return (sizeof(rpp::int64) == 8 && sizeof(rpp::uint) == 4) ? 0 : 1; }
#else
int main() { return 0; } // the header build does not exercise the module
#endif
