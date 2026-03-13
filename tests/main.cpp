#include <rpp/tests.h>

// libc++ (used by Clang) has an alloc-dealloc-mismatch bug in __libcpp_refstring:
// the constructor allocates with `::operator new` but the destructor frees with `std::free`.
// This triggers ASan alloc-dealloc-mismatch when throwing std::runtime_error (and siblings)
// across coroutine suspension points. This is a libc++ bug, not a bug in our code.
// See: https://github.com/llvm/llvm-project/issues/59432
#if defined(__clang__) && defined(__has_feature)
    #if __has_feature(address_sanitizer)
        extern "C" const char* __asan_default_options() {
            return "alloc_dealloc_mismatch=0";
        }
    #endif
#endif

static bool is_verbose(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] == '-' && argv[i][1] == 'v')
            return true;
    }
    return false;
}

int main(int argc, char** argv)
{
    if (is_verbose(argc, argv))
    {
        printf("==== Rpp Tests ====\n");
        for (int i = 0; i < argc; ++i)
            printf("  -- arg %d: %s\n", i, argv[i]);
    }
    int result = rpp::test::run_tests(argc, argv);
    rpp::test::cleanup_all_tests();
    return result;
}
