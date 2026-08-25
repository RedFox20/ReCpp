/* Guards that <rpp/config.h> keeps the RPP_*_SIZE macros visible to a C consumer. */
#include <rpp/config.h>

/* a negative array size fails the build when a size macro is missing */
static char size_probe[(RPP_SHORT_SIZE > 0 && RPP_INT_SIZE > 0
                     && RPP_LONG_SIZE > 0 && RPP_LONG_LONG_SIZE > 0) ? 1 : -1];

int main(void) { (void)size_probe; return 0; }
