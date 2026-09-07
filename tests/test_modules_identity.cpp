/**
 * Takes a constant address with no rpp header in this translation unit, so the address comes
 * from the module alone. test_modules.cpp compares it against the header address.
 */
#if RPP_BUILD_WITH_MODULES

import rpp.math; // includes come first, the import goes last

// a namespace-scope constant without `inline` gets one copy per translation unit
const void* module_pi_addr() noexcept { return &rpp::PI; }
#else
const void* module_pi_addr() noexcept { return nullptr; }
#endif
