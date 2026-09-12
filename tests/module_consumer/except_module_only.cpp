// Imports rpp.std and includes nothing at all, because <new> alone declares std::exception
// and would keep the catch below compiling after the module dropped that export.
#ifdef MAMA_HAS_MODULES
import rpp.std;

int main()
{
    // every name here comes from the module, so dropping any one of them fails the build
    try { throw std::runtime_error{"x"}; }
    catch (const std::exception& e) { if (e.what()[0] != 'x') return 1; }

    try { throw std::logic_error{"y"}; }
    catch (const std::exception& e) { if (e.what()[0] != 'y') return 2; }
    return 0;
}
#else
int main() { return 0; } // the header build does not exercise the module
#endif
