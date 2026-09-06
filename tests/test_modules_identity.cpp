/**
 * Takes a trait address with no rpp header in this translation unit, so the address comes
 * from the module alone. test_modules.cpp compares it against the header address.
 */
#if RPP_BUILD_WITH_MODULES
#include <vector>

import rpp.type_traits; // includes come first, the import goes last

// gcc gives a variable template without `inline` one copy per translation unit
const void* module_is_container_addr() noexcept { return &rpp::is_container<std::vector<int>>; }
#else
const void* module_is_container_addr() noexcept { return nullptr; }
#endif
