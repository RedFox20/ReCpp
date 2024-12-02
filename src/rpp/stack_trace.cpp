#include "stack_trace.h"
#include <cstdarg> // va_list
#include <csignal> // SIGSEGV
#include <cstdio>  // fprintf
#include <cstring> // strlen

#include "mutex.h"
#include "minmax.h"

#if _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN 1
#  endif
#  include <windows.h>
#  include <dbghelp.h>
#  include <tchar.h>
#  include <cstdio>
#  include <TlHelp32.h>
#  include <Psapi.h>
#  include <vector>
#  undef min
#  undef max
#else
#  include <cstdlib>  // malloc/free
#  include <cxxabi.h> // __cxa_demangle
#  include <unwind.h> // _Unwind_Backtrace
#  if RPP_LIBDW_ENABLED
#    if __has_include(<elfutils/libdwfl.h>)
#      define HAS_LIBDW 1
#    else
#      warning RPP_LIBDW_ENABLED but <elfutils/libdwfl.h> was not found so LIBDW was disabled, resulting in bad rpp::stack_trace
#    endif
#  endif
#  if HAS_LIBDW
#    include <elfutils/libdw.h>
#    include <elfutils/libdwfl.h>
#    include <dwarf.h>
#    include <unistd.h> // getpid
#    include <vector>
#    include <unordered_map>
#    include "sort.h" // rpp::insertion_sort
#  else
#    include <dlfcn.h>
#  endif
#endif

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////////////////
    // Trivial convenience wrappers

    std::string format_trace(const std::vector<uint64_t>& callstack) noexcept
    {
        return rpp::format_trace(std::string{}, callstack);
    }

    std::string stack_trace(size_t maxDepth) noexcept
    {
        return stack_trace(nullptr, 0, maxDepth, 2);
    }
    std::string stack_trace(const char* message, size_t maxDepth) noexcept
    {
        return stack_trace(message, strlen(message), maxDepth, 2);
    }
    std::string stack_trace(const std::string& message, size_t maxDepth) noexcept
    {
        return stack_trace(message.data(), message.size(), maxDepth, 2);
    }

    void print_trace(size_t maxDepth) noexcept
    {
        std::string s = stack_trace(nullptr, 0, maxDepth, 2);
        fwrite(s.c_str(), s.size(), 1, stderr);
    }
    void print_trace(const char* message, size_t maxDepth) noexcept
    {
        std::string s = stack_trace(message, strlen(message), maxDepth, 2);
        fwrite(s.c_str(), s.size(), 1, stderr);
    }
    void print_trace(const std::string& message, size_t maxDepth) noexcept
    {
        std::string s = stack_trace(message.data(), message.size(), maxDepth, 2);
        fwrite(s.c_str(), s.size(), 1, stderr);
    }

    std::runtime_error error_with_trace(const char* message, size_t maxDepth) noexcept
    {
        return std::runtime_error{ stack_trace(message, strlen(message), maxDepth, 2) };
    }
    std::runtime_error error_with_trace(const std::string& message, size_t maxDepth) noexcept
    {
        return std::runtime_error{ stack_trace(message.data(), message.size(), maxDepth, 2) };
    }

    traced_exception::traced_exception(const char* message) noexcept
        : runtime_error(error_with_trace(message))
    {
    }
    traced_exception::traced_exception(const std::string& message) noexcept
        : runtime_error(error_with_trace(message))
    {
    }

    using signal_handler_t = void (*)(int sig);

    static signal_handler_t OldHandler;

    void register_segfault_tracer()
    {
        OldHandler = signal(SIGSEGV, [](int sig)
        {
            if (OldHandler) OldHandler(sig);
            throw traced_exception("SIGSEGV");
        });
    }
    void register_segfault_tracer(std::nothrow_t)
    {
        OldHandler = signal(SIGSEGV, [](int sig)
        {
            if (OldHandler) OldHandler(sig);
            print_trace();
        });
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Common utils

    struct FuncnameCleaner
    {
        char buf[64];
        const char* ptr;
        int len;
        template<size_t N, size_t M> bool replace(const char(&what)[N], const char(&with)[M]) noexcept
        {
            if (memcmp(ptr, what, N - 1) != 0) return false;
            memcpy(&buf[len], with, M - 1);
            len += M - 1;
            return true;
        }
        bool try_replace_lambda() noexcept
        {
            if (!replace("<lambda", "lambda")) return false;
            ptr += sizeof("<lambda");
            for (int i = 0; i < 64 && *ptr && *ptr != '>'; ++i)
                ++ptr;
            return true;
        }
        template<size_t N> bool skip(const char(&what)[N]) noexcept
        {
            if (memcmp(ptr, what, N - 1) != 0) return false;
            ptr += N - 2; // - 2 because next() will bump the pointer
            return true;
        }
        const char* clean_name(const std::string& longFuncName) noexcept
        {
            if (longFuncName.empty()) return "(null)";
            ptr = longFuncName.data() - 1;
            len = 0;
            constexpr int MAX = sizeof(buf) - 4; // reserve space for 4 chars "...\0"
            for (;;)
            {
                if (len >= MAX) {
                    buf[len++] = '.'; buf[len++] = '.'; buf[len++] = '.'; // "..."
                    break;
                }
                char ch = *++ptr;
                if (ch == '<') {
                    if (try_replace_lambda()) continue;
                }
                // clean all std:: symbols
                if (ch == 's' && skip("std::")) continue;
                if (ch == 'r' && skip("rpp::")) continue;
#if !_MSC_VER // also skip __1:: to clean the symbols on clang
                if (ch == '_' && skip("__1::")) continue;
#else
                if (ch == ' ' && skip(" __cdecl")) continue;
#endif
                buf[len++] = ch;
            }
            if (buf[len - 1] == ']') --len; // remove Objective-C method ending bracket
            buf[len++] = '\0';
            return buf;
        }
    };

    class CallstackFormatter
    {
        int len = 0;
        static constexpr int MAX = 8192 - 1;
        char buf[MAX + 1] = "";
    public:
        std::string to_string() const noexcept { return { buf, buf + len }; }
        void writeln(const char* message, size_t n) noexcept
        {
            memcpy(&buf[len], message, n);
            len += (int)n;
            buf[len++] = '\n';
        }
        void writef(const char* fmt, ...) noexcept
        {
            va_list ap; va_start(ap, fmt);
            int n = vsnprintf(&buf[len], size_t(MAX - len), fmt, ap);
            va_end(ap);
            if (n > 0) {
                len += n;
            }
            else { // overflow
                len = sizeof(buf) - 1;
                buf[len] = '\0';
            }
        }
        void writeln(const CallstackEntry& c) noexcept
        {
            if (len >= MAX)
                return;
            FuncnameCleaner fc{};
            if (c.line) writef("  at %20s:%-4d  in  %s\n", c.short_path(), c.line, fc.clean_name(c.name));
            else        writef("  at %20s:%-4s  in  %s\n", c.short_path(), "??", fc.clean_name(c.name));
        }
    };

    const char* CallstackEntry::short_path() const noexcept
    {
        const std::string& longFilePath = !file.empty() ? file : module;
        if (longFilePath.empty()) return "(null)";
        const char* ptr = longFilePath.data();
        const char* eptr = ptr + longFilePath.size();
        for (int n = 1; ptr < eptr; --eptr)
            if (*eptr == '/' || *eptr == '\\')
                if (--n <= 0) return ++eptr;
        return eptr;
    }

    std::string CallstackEntry::clean_name() const noexcept
    {
        FuncnameCleaner fc{};
        fc.clean_name(name);
        return std::string{fc.buf, fc.buf + fc.len};
    }

    std::string CallstackEntry::to_string() const noexcept
    {
        CallstackFormatter fmt;
        FuncnameCleaner fc{};
        if (line) fmt.writef("%20s:%-4d  in  %s\n", short_path(), line, fc.clean_name(name));
        else      fmt.writef("%20s:%-4s  in  %s\n", short_path(), "??", fc.clean_name(name));
        return fmt.to_string();
    }

#if !_WIN32
    ///////////////////////////////////////////////////////////////////////////////////////////
    // CLANG Implementation

    /**
     * @brief Demangler for GCC/Clang, uses __cxa_demangle which can reallocate the buffer
     */
    struct Demangler
    {
        size_t len = 128;
        char* buf = (char*)malloc(128);
        ~Demangler() noexcept { free(buf); }
        const char* Demangle(const char* symbol) noexcept
        {
            char* demangled = __cxxabiv1::__cxa_demangle(symbol, buf, &len, nullptr);
            if (demangled) buf = demangled; // __cxa_demangle may realloc our buf
            return demangled ? demangled : symbol;
        }
    };

    ///////////////////////////////////////////////////////////////////////////////////////////

#if HAS_LIBDW
    
    struct FlatDie
    {
        Dwarf_Addr lo;
        Dwarf_Addr hi;
        Dwarf_Die* die;

        bool in_range(Dwarf_Addr x)          const noexcept { return lo <= x && x <= hi; }
        bool overlaps_with(const FlatDie& f) const noexcept { return in_range(f.lo) || in_range(f.hi); }
        bool nearly_adjacent(const FlatDie& f) const noexcept
        {
            if      (f.lo > hi) { return (f.lo - hi) <= 256; }
            else if (f.hi < lo) { return (lo - f.hi) <= 256; }
            return false;
        }
        bool try_merge(const FlatDie& f) noexcept
        {
            // same address ranges but different Compilation Units. Should be safe to merge:
            if (lo == f.lo && hi == f.hi)
                return true;

            // @todo Add more safe merge options
            return false;
        }
        void merge(const FlatDie& f) noexcept
        {
            lo = std::min(lo, f.lo);
            hi = std::max(hi, f.hi);
        }
        void print() const noexcept
        {
            const char* file = nullptr;
            int line = 0;
            Dwarf_Lines* lines;
            size_t nlines;
            if (dwarf_getsrclines (die, &lines, &nlines) == 0 && nlines > 0) {
                if (Dwarf_Line* src = dwarf_onesrcline(lines, 0)) {
                    file = dwarf_linesrc(src, nullptr, nullptr);
                    dwarf_lineno(src, &line);
                }
            }
            printf("  %p - %p  %p  %s:%d\n", (void*)lo, (void*)hi, die, file, line);
        }
    };

    struct ModuleMap
    {
        int max = 0;
        Dwarf_Addr bias = 0;
        std::vector<FlatDie> flatmap;
        std::vector<Dwarf_Die*> rootDies;

        void init(Dwfl_Module* mod) noexcept
        {
            flatmap.reserve(1024*16);
            rootDies.reserve(1024);

            Dwarf_Die* rootDie = nullptr;
            while ((rootDie = dwfl_module_nextcu(mod, rootDie, &bias))) {
                rootDies.push_back(rootDie);
                init_traverse(rootDie, rootDie);
            }

            max = int(flatmap.size()) - 1;

            rpp::insertion_sort(flatmap.data(), flatmap.size(), 
                [](const FlatDie& a, const FlatDie& b) {
                    return a.lo < b.lo && a.hi < b.hi;
                }
            );
    //        print_flatmap();
        }

        // void print_flatmap() noexcept
        // {
        //     for (const FlatDie& f : flatmap)
        //         f.print();
        //     printf("total entries: %zu\n", flatmap.size());
        //     printf("total root CU DIE: %zu\n", rootDies.size());
        // }

        void push_die(const FlatDie& f) noexcept
        {
            if (!flatmap.empty() && flatmap.back().try_merge(f))
                return;
            flatmap.push_back(f);
        }

        void insert_die_ranges(Dwarf_Die* rootDie, Dwarf_Die* node) noexcept
        {
            Dwarf_Addr base, low, high;
            ptrdiff_t offset = 0;
            while ((offset = dwarf_ranges(node, offset, &base, &low, &high)) > 0)
            {
                push_die({low, high, rootDie});
            }
        }

        void init_traverse(Dwarf_Die* rootDie, Dwarf_Die* parent) noexcept
        {
            Dwarf_Die node{};
            if (dwarf_child(parent, &node))
                return;
            do {
                int tag = dwarf_tag(&node);
                if (tag == DW_TAG_subprogram || tag == DW_TAG_inlined_subroutine)
                    insert_die_ranges(rootDie, &node);
                else if (tag == DW_TAG_namespace)
                    init_traverse(rootDie, &node);
            } while (dwarf_siblingof(&node, &node) == 0);
        }

        Dwarf_Die* find(Dwarf_Addr addr) noexcept
        {
            Dwarf_Addr pc = addr - bias;
            int imax = max;
            int imin = 0;
            const FlatDie* dies = flatmap.data();
            while (imin < imax)
            {
                int imid = (imin + imax) >> 1;
                if (dies[imid].hi < pc)
                    imin = imid + 1;
                else
                    imax = imid;
            }
            if (imax == imin) {
                const FlatDie& f = dies[imin];
                if (f.lo <= pc && pc < f.hi)
                {
    //                printf("%p match: ", (void*)addr); f.print();
                    return f.die;
                }
    //            printf("%p die range mismatch: ", (void*)addr); f.print();
                return nullptr; // watf
            }
            return nullptr;
        }
    };

    static Dwarf_Die* custom_module_addrdie(Dwfl_Module* mod, Dwarf_Addr addr, Dwarf_Addr* outBias) noexcept
    {
        static std::unordered_map<Dwfl_Module*, ModuleMap> moduleDies;
        ModuleMap* map;
        auto it = moduleDies.find(mod);
        if (it == moduleDies.end())
        {
            static rpp::mutex sync;
            std::lock_guard guard {sync};
            map = &moduleDies[mod];
            map->init(mod);
        }
        else map = &it->second;

        if (Dwarf_Die* die = map->find(addr)) {
            *outBias = map->bias;
            return die;
        }
        return nullptr;
    }

    static Dwfl* init_dwfl() noexcept
    {
        static Dwfl_Callbacks dwfl_callbacks {
                .find_elf       = &dwfl_linux_proc_find_elf,
                .find_debuginfo = &dwfl_standard_find_debuginfo,
                .section_address = nullptr,
                .debuginfo_path  = nullptr,
        };
        Dwfl* dwfl = dwfl_begin(&dwfl_callbacks);

        // gather all linked modules
        dwfl_report_begin(dwfl);
        dwfl_linux_proc_report (dwfl, getpid());
        dwfl_report_end(dwfl, nullptr, nullptr);
        return dwfl;
    }

    static CallstackEntry resolve_trace(Demangler& d, void* addr) noexcept
    {
        static Dwfl* dwfl = init_dwfl();
        CallstackEntry cse; cse.addr = (uint64_t)addr;
        if (Dwfl_Module* mod = dwfl_addrmodule(dwfl, cse.addr))
        {
            cse.module = dwfl_module_info(mod, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            cse.name   = dwfl_module_addrname(mod, cse.addr);
            cse.name   = d.Demangle(cse.name.c_str());

            Dwarf_Addr bias = 0;
            Dwarf_Die* die = dwfl_module_addrdie(mod, cse.addr, &bias);
            if (!die) die = custom_module_addrdie(mod, cse.addr, &bias);

            if (die)
            {
                if (Dwarf_Line* src = dwarf_getsrc_die(die, cse.addr - bias))
                {
                    cse.file = dwarf_linesrc(src, nullptr, nullptr);
                    dwarf_lineno(src, &cse.line);
                }
            }
        }
        return cse;
    }

#elif !HAS_LIBDW

    static CallstackEntry resolve_trace(Demangler& d, void* addr)
    {
        Dl_info info { nullptr, nullptr, nullptr, nullptr };
        dladdr(addr, &info);
        return {
            .addr = (uint64_t)addr,
            .line = 0,
            .name = d.Demangle(info.dli_sname),
            .file = nullptr,
            .module = info.dli_fname
        };
    }

#endif // HAS_LIBDW

    struct BacktraceState
    {
        void** current;
        void** end;
    };

    RPPAPI CallstackEntry get_address_info(uint64_t addr) noexcept
    {
        // TODO: need to optimize this to not allocate memory on every call
        Demangler d;
        return resolve_trace(d, (void*)addr);
    }

    static _Unwind_Reason_Code backtrace_cb(_Unwind_Context* context, void* arg) noexcept
    {
        auto* s = (BacktraceState*)arg;
        int ip_before_instruction;
        if (uintptr_t ip = _Unwind_GetIPInfo(context, &ip_before_instruction))
        {
            if (!ip_before_instruction)
                ip -= 1;
            if (s->current == s->end)
                return _URC_END_OF_STACK;
            *s->current++ = (void*)ip;
        }
        return _URC_NO_REASON;
    }

    RPPAPI std::vector<uint64_t> get_callstack(size_t maxDepth, size_t entriesToSkip) noexcept
    {
        maxDepth = rpp::min(maxDepth, static_cast<size_t>(256));

        void* callstack[maxDepth];
        BacktraceState state { callstack, callstack + maxDepth };
        // _Unwind is called directly to reduce the number of frames in the stack trace
        _Unwind_Backtrace(backtrace_cb, &state);
        size_t count = size_t(state.current - callstack);

        std::vector<uint64_t> addresses;
        if (entriesToSkip >= count)
            return addresses;
        addresses.reserve(count - entriesToSkip);
        for (size_t i = entriesToSkip; i < count; ++i)
        {
            addresses.push_back((uint64_t)callstack[i]);
        }
        return addresses;
    }

    RPPAPI std::string format_trace(const std::string& message, const std::vector<uint64_t>& callstack) noexcept
    {
        CallstackFormatter fmt;
        if (!message.empty()) fmt.writeln(message.c_str(), message.size());

        Demangler d;
        for (uint64_t addr : callstack)
        {
            fmt.writeln(resolve_trace(d, (void*)addr));
        }
        return fmt.to_string();
    }

    RPPAPI std::string stack_trace(const char* message, size_t messageLen, size_t maxDepth, size_t entriesToSkip) noexcept
    {
        maxDepth = rpp::min(maxDepth, static_cast<size_t>(256));

        void* callstack[maxDepth];
        BacktraceState state { callstack, callstack + maxDepth };
        // _Unwind is called directly to reduce the number of frames in the stack trace
        _Unwind_Backtrace(backtrace_cb, &state);
        size_t count = size_t(state.current - callstack);

        CallstackFormatter fmt;
        if (messageLen) fmt.writeln(message, messageLen);

        Demangler d;
        for (size_t i = entriesToSkip; i < count; ++i)
        {
            fmt.writeln(resolve_trace(d, callstack[i]));
        }
        return fmt.to_string();
    }

#else // VC++

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Windows implementation

    static rpp::recursive_mutex DbgHelpMutex; // DbgHelp.dll is single-threaded

    static void OnDebugError(const char* what, int lastError, uint64_t offset) noexcept
    {
        if (lastError)
        {
            char error[1024];
            int len = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, lastError, 0, error, 1024, nullptr);
            if (len > 2 && error[len - 2] == '\r') error[len -= 2] = '\0';
            fprintf(stderr, "Error %s addr: 0x%016llx error: (%d) %s\n", what, offset, lastError, error);
        }
        else
        {
            fprintf(stderr, "Error %s addr: 0x%016llx\n", what, offset);
        }
    }

    static const wchar_t* CombinePath(wchar_t (&combined)[1024], const wchar_t* folder, const wchar_t* file) noexcept
    {
        wcscpy_s(combined, folder);
        wcscat_s(combined, file);
        return combined;
    }
    static bool FileExists(const wchar_t* file) noexcept { return GetFileAttributesW(file) != INVALID_FILE_ATTRIBUTES; }
    static bool FileExists(const wchar_t* folder, const wchar_t* file) noexcept
    {
        wchar_t path[1024];
        return FileExists(CombinePath(path, folder, file));
    }
    static HMODULE LoadDbgHelp() noexcept
    {
        // Dynamically load the Entry-Points for dbghelp.dll:
        // But before we do this, we first check if the ".local" file exists (Dll Hell redirection)
        wchar_t exePath[1024]; // @note Try to avoid dynamic allocations in case the crash is because of OOM
        if (GetModuleFileNameW(nullptr, exePath, 1024) > 0 && !FileExists(exePath, L".local"))
        {
            wchar_t programFiles[1024];
            GetEnvironmentVariableW(L"ProgramFiles", programFiles, 1024);

            // ".local" file does not exist, so we can try to load the dbghelp.dll from the "Debugging Tools for Windows"
            const wchar_t* platformTags[] = {
                #if _M_IX86
                    L" (x86)",
                #elif _M_X64
                    L" (x64)",
                #elif _M_IA64
                    L" (ia64)",
                #endif
                    L"",
                #if _M_X64 || _M_IA64
                    L" 64-Bit",
                #endif
            };
            for (const wchar_t* tag : platformTags)
            {
                wchar_t path[1024];
                swprintf_s(path, L"%s\\Debugging Tools for Windows%s\\dbghelp.dll", programFiles, tag);
                if (FileExists(path))
                    return LoadLibraryW(path);
            }
        }
        return LoadLibraryW(L"dbghelp.dll"); // if not already loaded, try to load a default-one
    }

    template<class Func> bool LoadProc(Func& func, const char* proc) noexcept
    {
        static HMODULE DbgHelp = LoadDbgHelp();
        return DbgHelp ? (func = (Func)GetProcAddress(DbgHelp, proc)) != nullptr : false;
    }
    static decltype(SymCleanup)*                  pSymCleanup;
    static decltype(SymFunctionTableAccess64)*    pSymFunctionTableAccess64;
    static decltype(SymGetLineFromAddr64)*        pSymGetLineFromAddr64;
    static decltype(SymGetModuleBase64)*          pSymGetModuleBase64;
    static decltype(SymGetModuleInfo64)*          pSymGetModuleInfo64;
    static decltype(SymGetOptions)*               pSymGetOptions;
    static decltype(SymGetSymFromAddr64)*         pSymGetSymFromAddr64;
    static decltype(SymInitialize)*               pSymInitialize;
    static decltype(SymLoadModule64)*             pSymLoadModule64;
    static decltype(SymLoadModuleExW)*            pSymLoadModuleExW;
    static decltype(SymSetOptions)*               pSymSetOptions;
    static decltype(StackWalk64)*                 pStackWalk64;
    static decltype(UnDecorateSymbolName)*        pUnDecorateSymbolName;
    static decltype(SymGetSearchPath)*            pSymGetSearchPath;
    static decltype(SymGetLineFromInlineContext)* pSymGetLineFromInlineContext;
    static decltype(SymAddrIncludeInlineTrace)*   pSymAddrIncludeInlineTrace;
    static decltype(SymGetLineFromName64)*        pSymGetLineFromName64;

    static bool SymInit(void* hProcess) noexcept
    {
        if (pSymInitialize)
            return true;
        if (!LoadProc(pSymInitialize, "SymInitialize"))
        {
            OnDebugError("loading SymInitialize from dbghelp.dll", GetLastError(), 0);
            SetLastError(ERROR_DLL_INIT_FAILED);
            return false;
        }
        LoadProc(pSymCleanup,                  "SymCleanup");
        LoadProc(pStackWalk64,                 "StackWalk64");
        LoadProc(pSymGetOptions,               "SymGetOptions");
        LoadProc(pSymSetOptions,               "SymSetOptions");
        LoadProc(pSymFunctionTableAccess64,    "SymFunctionTableAccess64");
        LoadProc(pSymGetLineFromAddr64,        "SymGetLineFromAddr64");
        LoadProc(pSymGetModuleBase64,          "SymGetModuleBase64");
        LoadProc(pSymGetModuleInfo64,          "SymGetModuleInfo64");
        LoadProc(pSymGetSymFromAddr64,         "SymGetSymFromAddr64");
        LoadProc(pUnDecorateSymbolName,        "UnDecorateSymbolName");
        LoadProc(pSymLoadModule64,             "SymLoadModule64");
        LoadProc(pSymLoadModuleExW,            "SymLoadModuleExW");
        LoadProc(pSymGetSearchPath,            "SymGetSearchPath");
        LoadProc(pSymGetLineFromInlineContext, "SymGetLineFromInlineContext");
        LoadProc(pSymAddrIncludeInlineTrace,   "SymAddrIncludeInlineTrace");
        LoadProc(pSymGetLineFromName64,        "SymGetLineFromName64");

        if (!pSymInitialize(hProcess, nullptr, false))
        {
            OnDebugError("SymInitialize", GetLastError(), 0);
            SetLastError(ERROR_DLL_INIT_FAILED);
            return false;
        }

        DWORD symOptions = pSymGetOptions();
        symOptions |= SYMOPT_LOAD_LINES;
        symOptions |= SYMOPT_DEFERRED_LOADS;
        symOptions |= SYMOPT_FAIL_CRITICAL_ERRORS;
        pSymSetOptions(symOptions);
        return true;
    }

    static DWORD LoadModule(void* hProcess, const char* img, const char* mod, uint64_t baseAddr, int size) noexcept
    {
        if (pSymLoadModule64(hProcess, nullptr, img, mod, baseAddr, size))
            return ERROR_SUCCESS;
        return GetLastError();
    }

    static DWORD LoadModule(void* hProcess, const WCHAR* img, const WCHAR* mod, uint64_t baseAddr, int size) noexcept
    {
        if (pSymLoadModuleExW(hProcess, nullptr, img, mod, baseAddr, size, nullptr, 0))
            return ERROR_SUCCESS;
        return GetLastError();
    }

    static bool GetModuleListTH32(void* hProcess, uint32_t pid) noexcept
    {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
        if (hSnap == HANDLE(-1))
            return false;

        MODULEENTRY32 me; me.dwSize = sizeof(me);
        bool keepGoing = !!Module32First(hSnap, &me);
        int count = 0;
        while (keepGoing)
        {
#       if UNICODE
            LoadModule(hProcess, me.szExePath, me.szModule, (DWORD64)me.modBaseAddr, me.modBaseSize);
        #else
            LoadModule(hProcess, me.szExePath, me.szModule, (DWORD64)me.modBaseAddr, me.modBaseSize);
        #endif
            ++count;
            keepGoing = !!Module32Next(hSnap, &me);
        }
        CloseHandle(hSnap);
        return count > 0;
    }

    static bool GetModuleListPSAPI(void* hProcess) noexcept
    {
        DWORD cbNeeded;
        if (!EnumProcessModules(hProcess, nullptr, 0, &cbNeeded))
            return false;

        size_t totalModules = cbNeeded / sizeof(HMODULE);
        HMODULE* modules = (HMODULE*)_alloca(sizeof(HMODULE)*totalModules);

        MODULEINFO mi;
        char imageFile[4096];
        char moduleName[4096];

        for (size_t i = 0; i < totalModules; ++i)
        {
            GetModuleInformation(hProcess, modules[i], &mi, sizeof(mi)); // base address, size

            imageFile[0] = 0;  // image file name
            GetModuleFileNameExA(hProcess, modules[i], imageFile, sizeof(imageFile));

            moduleName[0] = 0; // module name
            GetModuleBaseNameA(hProcess, modules[i], moduleName, sizeof(moduleName));

            DWORD result = LoadModule(hProcess, imageFile, moduleName, (DWORD64)mi.lpBaseOfDll, mi.SizeOfImage);
            if (result != ERROR_SUCCESS)
                OnDebugError("LoadModule", result, 0);
        }
        return totalModules > 0;
    }

    static bool LoadModules(void* hProcess, uint32_t processId) noexcept
    {
        if (GetModuleListTH32(hProcess, processId)) // first try toolhelp32
            return true;
        return GetModuleListPSAPI(hProcess); // then try psapi
    }

    static const char* GetModuleNameFromAddr(void* hProcess, uint64_t baseAddr, char (&nameBuffer)[32]) noexcept
    {
        IMAGEHLP_MODULE64 m = { sizeof(IMAGEHLP_MODULE64) };
        if (!pSymGetModuleInfo64(hProcess, baseAddr, &m)) return nullptr;
        strcpy_s(nameBuffer, m.ModuleName);
        return nameBuffer;
    }

    static DWORD InitStackFrame(STACKFRAME64& s, const CONTEXT& c) noexcept
    {
        #ifdef _M_IX86
            s.AddrPC.Offset    = c.Eip;
            s.AddrPC.Mode      = AddrModeFlat;
            s.AddrFrame.Offset = c.Ebp;
            s.AddrFrame.Mode   = AddrModeFlat;
            s.AddrStack.Offset = c.Esp;
            s.AddrStack.Mode   = AddrModeFlat;
            return IMAGE_FILE_MACHINE_I386;
        #elif _M_X64
            s.AddrPC.Offset    = c.Rip;
            s.AddrPC.Mode      = AddrModeFlat;
            s.AddrFrame.Offset = c.Rsp;
            s.AddrFrame.Mode   = AddrModeFlat;
            s.AddrStack.Offset = c.Rsp;
            s.AddrStack.Mode   = AddrModeFlat;
            return IMAGE_FILE_MACHINE_AMD64;
        #elif _M_IA64
            s.AddrPC.Offset     = c.StIIP;
            s.AddrPC.Mode       = AddrModeFlat;
            s.AddrFrame.Offset  = c.IntSp;
            s.AddrFrame.Mode    = AddrModeFlat;
            s.AddrBStore.Offset = c.RsBSP;
            s.AddrBStore.Mode   = AddrModeFlat;
            s.AddrStack.Offset  = c.IntSp;
            s.AddrStack.Mode    = AddrModeFlat;
            return IMAGE_FILE_MACHINE_IA64;
        #else
            #error "Platform not supported!"
            return 0;
        #endif
    }

    static CallstackEntry get_address_info(HANDLE process, uint64_t addr) noexcept
    {
        char pSymBuf[sizeof(IMAGEHLP_SYMBOL64) + 512] = { 0 };
        IMAGEHLP_SYMBOL64* pSym = (IMAGEHLP_SYMBOL64*)pSymBuf;
        pSym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
        pSym->MaxNameLength = 512;

        CallstackEntry cse;
        cse.addr = addr;

        DWORD64 offsetFromSymbol = 0;
        if (pSymGetSymFromAddr64(process, addr, &offsetFromSymbol, pSym))
        {
            if (pSym->Name) cse.name = pSym->Name;
        }
        else
        {
            DWORD err = GetLastError();
            if (err != ERROR_INVALID_ADDRESS)
                OnDebugError("SymGetSymFromAddr64", err, addr);
        }

        DWORD offsetFromLine = 0;
        IMAGEHLP_LINE64 lineInfo = { 0 };
        lineInfo.SizeOfStruct = sizeof(lineInfo);
        if (pSymGetLineFromAddr64(process, addr, &offsetFromLine, &lineInfo))
        {
            if (lineInfo.LineNumber > 1000000) // @note Not sure yet why this happens with C++ StdLib lambdas
                lineInfo.LineNumber = 0;

            cse.line = lineInfo.LineNumber;
            if (lineInfo.FileName) cse.file = lineInfo.FileName;
        }

        char moduleNameBuf[32];
        if (const char* module = GetModuleNameFromAddr(process, addr, moduleNameBuf))
            cse.module = module;
        return cse;
    }

    RPPAPI CallstackEntry get_address_info(uint64_t addr) noexcept
    {
        HANDLE process = GetCurrentProcess();
        // DbgHelp.dll is single-threaded, so we need to synchronize here
        std::lock_guard lock { DbgHelpMutex };
        return get_address_info(process, addr);
    }

    RPPAPI std::string format_trace(const std::string& message, const std::vector<uint64_t>& callstack) noexcept
    {
        HANDLE process = GetCurrentProcess();
        CallstackFormatter fmt;
        if (!message.empty()) fmt.writeln(message.c_str(), message.size());

        // DbgHelp.dll is single-threaded, so we need to synchronize here
        std::lock_guard lock { DbgHelpMutex };

        for (uint64_t addr : callstack)
        {
            fmt.writeln(get_address_info(process, addr));
        }
        return fmt.to_string();
    }

    struct ThreadContext
    {
        HANDLE process = nullptr;
        HANDLE hThread = nullptr;
        CONTEXT c { 0 };
        STACKFRAME64 s = { 0 };
        DWORD imageType;
    };

    static const char* GetThreadContext(ThreadContext& tc) noexcept
    {
        tc.hThread = GetCurrentThread();
        tc.process = GetCurrentProcess();

        static bool modulesLoaded;
        if (!modulesLoaded)
        {
            std::lock_guard lock { DbgHelpMutex };
            if (!modulesLoaded)
            {
                if (!SymInit(tc.process))
                    return "<stack_trace:SymInitFailed>";
                modulesLoaded = LoadModules(tc.process, GetCurrentProcessId());
            }
        }

        tc.c.ContextFlags = CONTEXT_FULL; // capture the current context

        if (GetThreadId(tc.hThread) == GetCurrentThreadId())
        {
            RtlCaptureContext(&tc.c);
        }
        else
        {
            SuspendThread(tc.hThread);
            if (GetThreadContext(tc.hThread, &tc.c) == false)
            {
                ResumeThread(tc.hThread);
                return "<stack_trace:GetThreadContextFailed>";
            }
        }

        tc.imageType = InitStackFrame(tc.s, tc.c);
        return nullptr;
    }

    static void ReleaseThreadContext(ThreadContext& tc) noexcept
    {
        if (tc.hThread)
            ResumeThread(tc.hThread);
    }

    static size_t walk_callstack(ThreadContext& tc, DWORD64* outCallstack, size_t maxDepth, size_t entriesToSkip) noexcept
    {
        constexpr size_t maxRecursionCount = 128;
        size_t curRecursionCount = 0;
        size_t count = 0;

		entriesToSkip += 1; // skip the current frame

        for (size_t frameNum = 0; count < maxDepth; ++frameNum)
        {
            // get next stack frame (StackWalk64(), SymFunctionTableAccess64(), SymGetModuleBase64())
            if (!pStackWalk64(tc.imageType, tc.process, tc.hThread, &tc.s, &tc.c, nullptr, pSymFunctionTableAccess64, pSymGetModuleBase64, nullptr))
            {
                OnDebugError("StackWalk64", 0, tc.s.AddrPC.Offset);
                break;
            }
            if (frameNum < entriesToSkip)
                continue;

            if (tc.s.AddrPC.Offset == tc.s.AddrReturn.Offset)
            {
                if (curRecursionCount > maxRecursionCount)
                {
                    OnDebugError("StackWalk64-Endless-Callstack", 0, tc.s.AddrPC.Offset);
                    break;
                }
                ++curRecursionCount;
            }
            else
            {
                curRecursionCount = 0;
            }

            outCallstack[count++] = tc.s.AddrPC.Offset;

            if (tc.s.AddrReturn.Offset == 0) // no return? then we're done!
            {
                SetLastError(ERROR_SUCCESS);
                break;
            }
        }
        return count;
    }

    RPPAPI std::vector<uint64_t> get_callstack(size_t maxDepth, size_t entriesToSkip) noexcept
    {
        ThreadContext tc;
        if (GetThreadContext(tc))
            return {};

        maxDepth = rpp::min(maxDepth, 256llu);
        uint64_t* callstack = (uint64_t*)_malloca(maxDepth * sizeof(uint64_t));
        size_t count;
        {
            // DbgHelp.dll is single-threaded, so we need to synchronize here
            std::lock_guard lock { DbgHelpMutex };
            count = walk_callstack(tc, callstack, maxDepth, entriesToSkip);
        }

        // copy the callstack to a vector
        std::vector<uint64_t> addresses { callstack, callstack + count };
        _freea(callstack);

        ReleaseThreadContext(tc);
        return addresses;
    }

    RPPAPI std::string stack_trace(const char* message, size_t messageLen, size_t maxDepth, size_t entriesToSkip) noexcept
    {
        ThreadContext tc;
        if (const char* error = GetThreadContext(tc))
            return error;

        CallstackFormatter fmt;
        if (messageLen) fmt.writeln(message, messageLen);

        maxDepth = rpp::min(maxDepth, 256llu);
        uint64_t* callstack = (uint64_t*)_malloca(maxDepth * sizeof(uint64_t));
        {
            // DbgHelp.dll is single-threaded, so we need to synchronize here
            std::lock_guard lock { DbgHelpMutex };

            // walk the stack
            size_t count = walk_callstack(tc, callstack, maxDepth, entriesToSkip);

            // format the callstack
            for (size_t i = 0; i < count; ++i)
            {
                fmt.writeln(get_address_info(tc.process, callstack[i]));
            }
        }
		_freea(callstack);
        ReleaseThreadContext(tc);
        return fmt.to_string();
    }

    #endif

    ///////////////////////////////////////////////////////////////////////////////////////////


}
