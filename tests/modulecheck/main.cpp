/**
 * Runs every module-only consumer check. A check returns 0 on success.
 * The build itself is the main gate: a missing export list entry fails to compile.
 */
#include <cstdio>

int check_debugging();

int main()
{
    if (int r = check_debugging()) { printf("check_debugging FAILED: %d\n", r); return r; }
    printf("RppModuleChecks: all checks passed\n");
    return 0;
}
