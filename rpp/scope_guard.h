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
 *  // more complicated example dealing with legacy C API-s
 *  bool example2() nothrow 
 *  {
 *      void* h = clibrary_create_handle();
 *      scope_guard([&]{ clibrary_free_handle(h); });
 *      if (!h)
 *          return false;
 *
 *      if (FILE* f = fopen("example", "rb"))
 *      {
 *          scope_guard([&]{ fclose(f); });
 *          if (check_file_header(f) == false)
 *              return false;
 *          
 *          binary_record br;
 *          if (read_record(&br) == false)
 *              return false;
 *          // ...
 *          return true;
 *      }
 *  }
 * @endcode
 */
namespace rpp
{
    template<class Func> struct scope_finalizer
    {
        Func func;
        bool valid = true;
        ~scope_finalizer() { if (valid) func(); }
        scope_finalizer(Func&& func)          noexcept : func{(Func&&)func} {}
        scope_finalizer(scope_finalizer&& sf) noexcept : func{(Func&&)sf.func}
        {
            sf.valid = false;
        }
        scope_finalizer& operator=(scope_finalizer&& sf) noexcept
        {
            func = (Func&&)sf.func;
            valid = sf.valid;
            sf.valid = false;
            return *this;
        }
        scope_finalizer(const scope_finalizer&) = delete;
        scope_finalizer& operator=(const scope_finalizer&) = delete;

    };

    template<class Func> scope_finalizer<Func> make_scope_guard(Func&& f)
    {
        return scope_finalizer<Func>{ (Func&&)f };
    }
}

#define scope_guard(lambda) auto scopeFinalizer__##__LINE__ = rpp::make_scope_guard(lambda)
