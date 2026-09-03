// C++20 module interface unit for <rpp/scope_guard.h>.
// A module cannot export a macro, so scope_guard() needs the header.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "scope_guard.h"

// the name drops the underscore, because MSVC expands the scope_guard macro in a module directive
export module rpp.scopeguard;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::scope_finalizer;
    using rpp::make_scope_guard;
}
// GENERATED EXPORTS END
