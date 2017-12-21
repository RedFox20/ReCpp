#pragma once
/**
 * Manual cleanup tasks for non-destructor types, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 * @code
 *  void example()
 *  {
 *      void* h = clibrary_create_handle();
 *      FILE* f = fopen("example", "rb");
 *      scope_guard([&]{
 *          if (f) fclose(f);
 *          clibrary_free_handle(h);
 *      });
 *  }
 * @endcode
 */
namespace rpp
{
    struct unique_flag {
        bool v = true;
        explicit operator bool() const { return v; }
        unique_flag() = default;
        unique_flag(unique_flag&& b) noexcept : v{b.v} {}
        unique_flag& operator=(unique_flag&& uf) noexcept { v = uf.v; uf.v = false; return *this; }
        unique_flag(const unique_flag&)            = delete;
        unique_flag& operator=(const unique_flag&) = delete;
    };
    template<class Func> struct scope_finalizer {
        Func f;
        unique_flag v;
        ~scope_finalizer() { if (v) f(); }
    };
    template<class Func> scope_finalizer<Func> make_scope_guard(Func&& f) {
        return { forward<Func>(f) };
    }
}

#define scope_guard(lambda) auto scopeFinalizer__##__LINE__ = rpp::make_scope_guard(lambda)
