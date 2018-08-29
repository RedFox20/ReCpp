#include "../rpp/tests.h"

int main(int argc, char** argv)
{
    printf("==== Rpp Tests ====\n");
    for (int i = 0; i < argc; ++i)
        printf("  -- arg %d: %s\n", i, argv[i]);

    return rpp::test::run_tests(argc, argv);
}
