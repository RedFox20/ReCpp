// The std smart pointer names a public ReCpp signature writes. `rpp.std` re-exports this part.
// <memory> owns a unit, because gcc-14 cannot write one which also carries <vector>, B24.
module;

#include <memory>

export module rpp.std.memory;

export namespace std {
    using std::unique_ptr;
    using std::shared_ptr;
    using std::weak_ptr;
    using std::make_unique;
    using std::make_shared;
    using std::default_delete;
}

// A std::shared_ptr also needs <memory> to link, see BUGS.md B22.
