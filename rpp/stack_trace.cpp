#include "stack_trace.h"
#include <cstdarg> // va_list
#include <csignal> // SIGSEGV
#include <cstdio>  // fprintf

#if _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#include <dbghelp.h>
#include <tchar.h>
#include <stdio.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <vector>
#include <mutex>
#else
#include <cstdlib>  // malloc/free
#include <cxxabi.h> // __cxa_demangle
#include <unwind.h> // _Unwind_Backtrace
# if __has_include(<elfutils/libdwfl.h>)
#  define HAS_LIBDW 1
# endif
# if HAS_LIBDW
#  include <elfutils/libdw.h>
#  include <elfutils/libdwfl.h>
#  include <dwarf.h>
#  include <unistd.h> // getpid
#  include <vector>
#  include <unordered_map>
#  include <mutex>
# else
#  include <dlfcn.h>
# endif
#endif

namespace rpp
{
    using namespace std;
    ///////////////////////////////////////////////////////////////////////////////////////////
    // Trivial convenience wrappers

    string stack_trace(int maxDepth) noexcept
    {
        return stack_trace(nullptr, 0, maxDepth, 2);
    }
    string stack_trace(const char* message, int maxDepth) noexcept
    {
        return stack_trace(message, strlen(message), maxDepth, 2);
    }
    string stack_trace(const string& message, int maxDepth) noexcept
    {
        return stack_trace(message.data(), message.size(), maxDepth, 2);
    }

    void print_trace(int maxDepth) noexcept
    {
        string s = stack_trace(nullptr, 0, maxDepth, 2);
        fwrite(s.c_str(), s.size(), 1, stderr);
    }
    void print_trace(const char* message, int maxDepth) noexcept
    {
        string s = stack_trace(message, strlen(message), maxDepth, 2);
        fwrite(s.c_str(), s.size(), 1, stderr);
    }
    void print_trace(const string& message, int maxDepth) noexcept
    {
        string s = stack_trace(message.data(), message.size(), maxDepth, 2);
        fwrite(s.c_str(), s.size(), 1, stderr);
    }

    runtime_error error_with_trace(const char* message, int maxDepth) noexcept
    {
        return runtime_error{ stack_trace(message, strlen(message), maxDepth, 2) };
    }
    runtime_error error_with_trace(const string& message, int maxDepth) noexcept
    {
        return runtime_error{ stack_trace(message.data(), message.size(), maxDepth, 2) };
    }

    traced_exception::traced_exception(const char* message) noexcept
        : runtime_error(error_with_trace(message))
    {
    }
    traced_exception::traced_exception(const string& message) noexcept
        : runtime_error(error_with_trace(message))
    {
    }

    void register_segfault_tracer()
    {
        signal(SIGSEGV, [](int)
        {
            throw traced_exception("SIGSEGV");
        });
    }
    void register_segfault_tracer(nothrow_t)
    {
        signal(SIGSEGV, [](int)
        {
            print_trace();
        });
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Common utils

    struct CallstackEntry
    {
        uint64_t addr = 0;  // if 0, we have no valid entry
        int      line = 0;
        const char* name = nullptr;
        const char* file = nullptr;   // if nullptr, we have no valid file info, try using module name instead
        const char* module = nullptr;

        const char* short_path() const // @return short file or module path
        {
            const char* longFilePath = file ? file : module;
            if (longFilePath == nullptr) return "(null)";
            const char* eptr = longFilePath + strlen(longFilePath);
            for (int n = 1; longFilePath < eptr; --eptr)
                if (*eptr == '/' || *eptr == '\\')
                    if (--n <= 0) return ++eptr;
            return eptr;
        }
    };

    class FuncnameCleaner
    {
        char buf[64];
        const char* ptr;
        int len;
        template<size_t N, size_t M> bool replace(const char(&what)[N], const char(&with)[M]) {
            if (memcmp(ptr, what, N - 1) != 0) return false;
            memcpy(&buf[len], with, M - 1);
            len += M - 1;
            return true;
        }
        bool try_replace_lambda() {
            if (!replace("<lambda", "lambda")) return false;
            ptr += sizeof("<lambda");
            for (int i = 0; i < 64 && *ptr && *ptr != '>'; ++i)
                ++ptr;
            return true;
        }
        template<size_t N> bool skip(const char(&what)[N]) {
            if (memcmp(ptr, what, N - 1) != 0) return false;
            ptr += N - 2; // - 2 because next() will bump the pointer
            return true;
        }
    public:
        const char* clean_name(const char* longFuncName) {
            if (longFuncName == nullptr) return "(null)";
            ptr = longFuncName - 1;
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
        std::string to_string() const { return { buf, buf + len }; }
        void writeln(const char* message, size_t n) {
            memcpy(&buf[len], message, n);
            len += (int)n;
            buf[len++] = '\n';
        }
        void writef(const char* fmt, ...) {
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
        void writeln(const CallstackEntry& cse) {
            if (len >= MAX)
                return;
            FuncnameCleaner fc{};
            if (cse.line) writef("  at %20s:%-4d  in  %s\n", cse.short_path(), cse.line, fc.clean_name(cse.name));
            else          writef("  at %20s:%-4s  in  %s\n", cse.short_path(), "??", fc.clean_name(cse.name));
        }
    };


#if !_WIN32
    ///////////////////////////////////////////////////////////////////////////////////////////
    // CLANG Implementation

    struct Demangler
    {
        size_t len = 128;
        char* buf = (char*)malloc(128);
        ~Demangler() { free(buf); }
        const char* Demangle(const char* symbol)
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

        bool in_range(Dwarf_Addr x)          const { return lo <= x && x <= hi; }
        bool overlaps_with(const FlatDie& f) const { return in_range(f.lo) || in_range(f.hi); }
        bool nearly_adjacent(const FlatDie& f) const
        {
            if      (f.lo > hi) { return (f.lo - hi) <= 256; }
            else if (f.hi < lo) { return (lo - f.hi) <= 256; }
            return false;
        }
        bool try_merge(const FlatDie& f)
        {
            // same address ranges but different Compilation Units. Should be safe to merge:
            if (lo == f.lo && hi == f.hi)
                return true;

            // @todo Add more safe merge options
            return false;
        }
        void merge(const FlatDie& f)
        {
            lo = std::min(lo, f.lo);
            hi = std::max(hi, f.hi);
        }
        void print() const
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
        vector<FlatDie> flatmap;
        vector<Dwarf_Die*> rootDies;

        void init(Dwfl_Module* mod)
        {
            flatmap.reserve(1024*16);
            rootDies.reserve(1024);

            Dwarf_Die* rootDie = nullptr;
            while ((rootDie = dwfl_module_nextcu(mod, rootDie, &bias))) {
                rootDies.push_back(rootDie);
                init_traverse(rootDie, rootDie);
            }

            max = int(flatmap.size()) - 1;

            sort(flatmap.data(), flatmap.data()+flatmap.size(), [](const FlatDie& a, const FlatDie& b) {
                return a.lo < b.lo && a.hi < b.hi;
            });
    //        print_flatmap();
        }

        void print_flatmap()
        {
            for (const FlatDie& f : flatmap)
                f.print();
            printf("total entries: %zu\n", flatmap.size());
            printf("total root CU DIE: %zu\n", rootDies.size());
        }

        void push_die(const FlatDie& f)
        {
            if (!flatmap.empty() && flatmap.back().try_merge(f))
                return;
            flatmap.push_back(f);
        }

        void insert_die_ranges(Dwarf_Die* rootDie, Dwarf_Die* node)
        {
            Dwarf_Addr base, low, high;
            ptrdiff_t offset = 0;
            while ((offset = dwarf_ranges(node, offset, &base, &low, &high)) > 0)
            {
                push_die({low, high, rootDie});
            }
        }

        void init_traverse(Dwarf_Die* rootDie, Dwarf_Die* parent)
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

        Dwarf_Die* find(Dwarf_Addr addr)
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

    static Dwarf_Die* custom_module_addrdie(Dwfl_Module* mod, Dwarf_Addr addr, Dwarf_Addr* outBias)
    {
        static unordered_map<Dwfl_Module*, ModuleMap> moduleDies;
        ModuleMap* map;
        auto it = moduleDies.find(mod);
        if (it == moduleDies.end())
        {
            static mutex sync;
            lock_guard<mutex> guard {sync};
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

    static Dwfl* init_dwfl()
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

    static CallstackEntry resolve_trace(Demangler& d, void* addr)
    {
        static Dwfl* dwfl = init_dwfl();
        CallstackEntry cse; cse.addr = (uint64_t)addr;
        if (Dwfl_Module* mod = dwfl_addrmodule(dwfl, cse.addr))
        {
            cse.module = dwfl_module_info(mod, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            cse.name   = dwfl_module_addrname(mod, cse.addr);
            cse.name   = d.Demangle(cse.name);

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
        Dl_info info { nullptr };
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

    std::string stack_trace(const char* message, size_t messageLen, int maxDepth, int entriesToSkip) noexcept
    {
        Demangler d;
        void* callstack[maxDepth];

        struct BacktraceState
        {
            void** current;
            void** end;
        } state { callstack, callstack + maxDepth };

        _Unwind_Backtrace([](_Unwind_Context* context, void* arg)
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
        }, &state);

        auto count = int(state.current - callstack);

        CallstackFormatter fmt;
        if (messageLen) fmt.writeln(message, messageLen);

        for (int i = entriesToSkip; i < count; ++i)
        {
            fmt.writeln(resolve_trace(d, callstack[i]));
        }
        return fmt.to_string();
    }

#else // VC++

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Windows implementation

    static mutex DbgHelpMutex; // DbgHelp.dll is single-threaded

    static void OnDebugError(const char* what, int lastError, uint64_t offset)
    {
        if (lastError)
        {
            char error[1024];
            int len = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, lastError, 0, error, 1024, nullptr);
            if (error[len - 2] == '\r') error[len -= 2] = '\0';
            fprintf(stderr, "Error %s addr: 0x%016llx error: (%d) %s\n", what, offset, lastError, error);
        }
        else
        {
            fprintf(stderr, "Error %s addr: 0x%016llx\n", what, offset);
        }
    }

    static const wchar_t* CombinePath(wchar_t (&combined)[1024], const wchar_t* folder, const wchar_t* file)
    {
        wcscpy_s(combined, folder);
        wcscat_s(combined, file);
        return combined;
    }
    static bool FileExists(const wchar_t* file) { return GetFileAttributesW(file) != INVALID_FILE_ATTRIBUTES; }
    static bool FileExists(const wchar_t* folder, const wchar_t* file)
    {
        wchar_t path[1024];
        return FileExists(CombinePath(path, folder, file));
    }
    static HMODULE LoadDbgHelp()
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

    template<class Func> bool LoadProc(Func& func, const char* proc)
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
    static decltype(SymSetOptions)*               pSymSetOptions;
    static decltype(StackWalk64)*                 pStackWalk64;
    static decltype(UnDecorateSymbolName)*        pUnDecorateSymbolName;
    static decltype(SymGetSearchPath)*            pSymGetSearchPath;
    static decltype(SymGetLineFromInlineContext)* pSymGetLineFromInlineContext;
    static decltype(SymAddrIncludeInlineTrace)*   pSymAddrIncludeInlineTrace;
    static decltype(SymGetLineFromName64)*        pSymGetLineFromName64;

    static bool SymInit(void* hProcess)
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

    static DWORD LoadModule(void* hProcess, const char* img, const char* mod, uint64_t baseAddr, int size)
    {
        if (pSymLoadModule64(hProcess, nullptr, img, mod, baseAddr, size))
            return ERROR_SUCCESS;
        return GetLastError();
    }

    static bool GetModuleListTH32(void* hProcess, uint32_t pid)
    {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
        if (hSnap == HANDLE(-1))
            return false;

        MODULEENTRY32 me; me.dwSize = sizeof(me);
        bool keepGoing = !!Module32First(hSnap, &me);
        int count = 0;
        while (keepGoing)
        {
            LoadModule(hProcess, me.szExePath, me.szModule, (DWORD64)me.modBaseAddr, me.modBaseSize);
            ++count;
            keepGoing = !!Module32Next(hSnap, &me);
        }
        CloseHandle(hSnap);
        return count > 0;
    }

    static bool GetModuleListPSAPI(void* hProcess)
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

    static bool LoadModules(void* hProcess, uint32_t processId)
    {
        if (GetModuleListTH32(hProcess, processId)) // first try toolhelp32
            return true;
        return GetModuleListPSAPI(hProcess); // then try psapi
    }

    static const char* GetModuleNameFromAddr(void* hProcess, uint64_t baseAddr, char (&nameBuffer)[32])
    {
        IMAGEHLP_MODULE64 m = { sizeof(IMAGEHLP_MODULE64) };
        if (!pSymGetModuleInfo64(hProcess, baseAddr, &m)) return nullptr;
        return strcpy(nameBuffer, m.ModuleName);
    }

    static DWORD InitStackFrame(STACKFRAME64& s, const CONTEXT& c)
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

    std::string stack_trace(const char* message, size_t messageLen, int maxDepth, int entriesToSkip) noexcept
    {
        HANDLE hThread = GetCurrentThread();
        HANDLE process = GetCurrentProcess();
        DWORD processId = GetCurrentProcessId();

        static bool modulesLoaded;
        if (!modulesLoaded)
        {
            if (!SymInit(process))
                return "<stack_trace:SymInitFailed>";
            modulesLoaded = LoadModules(process, processId);
        }

        CONTEXT c = { 0 }; // capture the current context
        c.ContextFlags = CONTEXT_FULL;

        if (GetThreadId(hThread) == GetCurrentThreadId())
        {
            RtlCaptureContext(&c);
        }
        else
        {
            SuspendThread(hThread);
            if (GetThreadContext(hThread, &c) == false)
            {
                ResumeThread(hThread);
                return "<stack_trace:GetThreadContextFailed>";
            }
        }

        STACKFRAME64 s = { 0 };
        DWORD imageType = InitStackFrame(s, c);

        char pSymBuf[sizeof(IMAGEHLP_SYMBOL64) + 512] = { 0 };
        IMAGEHLP_SYMBOL64* pSym = (IMAGEHLP_SYMBOL64 *)pSymBuf;
        pSym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
        pSym->MaxNameLength = 512;

        char moduleNameBuf[32];
        int curRecursionCount = 0;
        constexpr int maxRecursionCount = 128;

        CallstackFormatter fmt;
        if (messageLen) fmt.writeln(message, messageLen);

        // DbgHelp.dll is single-threaded, so we need to synchronize here
        std::lock_guard<std::mutex> guard { DbgHelpMutex };

        for (int frameNum = 0; frameNum < maxDepth; ++frameNum)
        {
            // get next stack frame (StackWalk64(), SymFunctionTableAccess64(), SymGetModuleBase64())
            if (!pStackWalk64(imageType, process, hThread, &s, &c, nullptr, pSymFunctionTableAccess64, pSymGetModuleBase64, nullptr))
            {
                OnDebugError("StackWalk64", 0, s.AddrPC.Offset);
                break;
            }
            if (frameNum < entriesToSkip)
                continue;

            CallstackEntry cse { s.AddrPC.Offset };

            if (s.AddrPC.Offset == s.AddrReturn.Offset)
            {
                if (curRecursionCount > maxRecursionCount)
                {
                    OnDebugError("StackWalk64-Endless-Callstack", 0, s.AddrPC.Offset);
                    break;
                }
                ++curRecursionCount;
            }
            else curRecursionCount = 0;

            DWORD64 offsetFromSymbol = 0;
            if (pSymGetSymFromAddr64(process, s.AddrPC.Offset, &offsetFromSymbol, pSym))
            {
                cse.name = pSym->Name;
            }
            else OnDebugError("SymGetSymFromAddr64", GetLastError(), s.AddrPC.Offset);

            DWORD offsetFromLine = 0;
            IMAGEHLP_LINE64 lineInfo = { 0 };
            lineInfo.SizeOfStruct = sizeof(lineInfo);
            if (pSymGetLineFromAddr64(process, s.AddrPC.Offset, &offsetFromLine, &lineInfo))
            {
                if (lineInfo.LineNumber > 1000000) // @note Not sure yet why this happens with C++ StdLib lambdas
                    lineInfo.LineNumber = 0;

                cse.line = lineInfo.LineNumber;
                cse.file = lineInfo.FileName;
            }

            cse.module = GetModuleNameFromAddr(process, s.AddrPC.Offset, moduleNameBuf);
            fmt.writeln(cse);

            if (s.AddrReturn.Offset == 0) // no return? then we're done!
            {
                SetLastError(ERROR_SUCCESS);
                break;
            }
        }
        ResumeThread(hThread);
        return fmt.to_string();
    }

    #endif

    ///////////////////////////////////////////////////////////////////////////////////////////


}
