// Builds one source through the exported modules and through the headers, and both must agree.
// Every #include comes first: GCC 14 re-parses a std header that follows an import, see AGENTS.md.
#include <rpp/config.h>
#include <rpp/debugging.macros.h> // a module exports no macro, so LogInfo comes from the header
#include <cstdio>
#include <string>
#include <vector>
#ifndef MAMA_HAS_MODULES
#  include <rpp/strview.h>
#  include <rpp/debugging.h>
#  define BUILT_WITH "HEADERS"
#endif

#ifdef MAMA_HAS_MODULES
import rpp.strview;
import rpp.debugging;
#  define BUILT_WITH "MODULES"
#endif

using namespace rpp::literals;

static int failed = 0;
static std::string g_log_captured; // the module-only formatted-log check captures into this

static void check(const char* what, bool ok)
{
    printf("  %-28s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) ++failed;
}

static std::vector<std::string> tokenize(rpp::strview line, char delim)
{
    std::vector<std::string> tokens;
    rpp::strview token;
    while (line.next(token, delim))
        tokens.push_back(std::string{token.str, (size_t)token.len});
    return tokens;
}

int main()
{
    printf("consumer built with %s\n", BUILT_WITH);

    // the core use: split a few strings into tokens
    struct test_case { const char* input; std::vector<std::string> expect; };
    const test_case cases[] = {
        { "alpha,beta,gamma", { "alpha", "beta", "gamma" } },
        { "one,,three",       { "one", "", "three" } },   // strview::next keeps an empty field
        { "single",           { "single" } },
    };
    for (const test_case& c : cases)
    {
        std::vector<std::string> tokens = tokenize(rpp::strview{c.input}, ',');
        printf("  %-18s ->", c.input);
        for (const std::string& t : tokens) printf(" [%s]", t.c_str());
        printf("%s\n", tokens == c.expect ? "" : "   MISMATCH");
        if (tokens != c.expect) ++failed;
    }

    // the re-exported surface, one check per family
    check("_sv literal",        rpp::strview{"abc"} == "abc"_sv);
    check("operator== char*",   rpp::strview{"abc"} == "abc");
    check("operator!= char*",   rpp::strview{"abc"} != "abd");
    check("operator<",          rpp::strview{"abc"} < rpp::strview{"abd"});
    check("operator+",          std::string{rpp::strview{"ab"} + rpp::strview{"cd"}} == "abcd");
    check("to_int",             rpp::to_int("42") == 42);
    check("to_int64",           rpp::to_int64("9000000000") == 9000000000LL);
    check("to_double",          rpp::to_double("2.5") == 2.5);
    check("strview::to_int",    rpp::strview{"17"}.to_int() == 17);
    check("strview::to_float",  rpp::strview{"1.5"}.to_float() == 1.5f);
    check("strequals",          rpp::strequals("abc", "abc", 3));
    check("strequalsi",         rpp::strequalsi("ABC", "abc", 3));
    check("strcontains",        rpp::strcontains("hello", 5, 'e'));
    { rpp::string s = "ABC"; check("to_lower", rpp::to_lower(s) == "abc"); }
    { rpp::string s = "abc"; check("to_upper", rpp::to_upper(s) == "ABC"); }
    check("concat",             rpp::concat(rpp::strview{"a"}, rpp::strview{"b"}) == "ab");
    check("type aliases",       sizeof(rpp::int64) == 8 && sizeof(rpp::uint) == 4);

    { // the numeric to_string overloads: strview.h declares them outside the unicode guard
        char buffer[32];
        int written = rpp::to_string(buffer, 42);
        check("to_string int", rpp::strview(buffer, written) == "42");
    }
    { // rpp.debugging, the second exported module. A misspelled export fails to import
        LogSeverity previous = GetLogSeverityFilter();
        SetLogSeverityFilter(LogSeverityError);
        check("rpp.debugging filter", GetLogSeverityFilter() == LogSeverityError);
        SetLogSeverityFilter(previous);
        // the macros expand to the exported _LogInfo, so this proves both halves reach a consumer
        LogInfo("consumer imported rpp.debugging");
        check("rpp.debugging shorten", rpp::strview{rpp::shorten_filename("a/b/c.cpp")} != "");
    }
    { // a formatted log macro expands to rpp::__wrap<rpp::__clean_type<T>>, the module-only helper path
        SetLogHandler([](LogSeverity, const char* msg, int len) { g_log_captured.assign(msg, (size_t)len); });
        LogSeverity previous = GetLogSeverityFilter();
        SetLogSeverityFilter(LogSeverityInfo);
        std::string name = "wrapped";
        const char* tag = "tag";
        rpp::strview view = "view";
        LogError("i=%d s=%s c=%s v=%s", 7, name, tag, view); // wraps int, std::string, const char*, strview
        SetLogSeverityFilter(previous);
        SetLogHandler(nullptr);
        check("formatted log macro", g_log_captured.find("i=7 s=wrapped c=tag v=view") != std::string::npos);
    }

    { // line_parser walks a multi-line buffer
        rpp::line_parser parser{"first\nsecond\nthird"};
        rpp::strview line; int lines = 0;
        while (parser.read_line(line)) ++lines;
        check("line_parser", lines == 3);
    }
    { // operator>> reads a value out of a strview
        rpp::strview src{"123"}; int value = 0; src >> value;
        check("operator>>", value == 123);
    }
    { // keyval_parser and bracket_parser, the two remaining exported parsers
        rpp::keyval_parser kv{rpp::strview{"key=value\nother=2\n"}};
        rpp::strview key, value; int pairs = 0;
        while (kv.read_next(key, value)) ++pairs;
        check("keyval_parser", pairs == 2);

        rpp::bracket_parser bp{rpp::strview{"root {\n  child 1\n}\n"}};
        rpp::strview bkey, bvalue; int reads = 0;
        while (bp.read_keyval(bkey, bvalue) != -1) ++reads;
        check("bracket_parser", reads >= 1);
    }
    { // the POD variant converts to a strview
        rpp::strview_ pod{"pod", 3};
        check("strview_ POD", rpp::strview{pod} == "pod");
    }
    { // traits and the concept the facade re-exports
        using traits = rpp::strview_traits<rpp::strview>;
        check("strview_traits", sizeof(traits::strview_t) == sizeof(rpp::strview));
        check("StringViewType", rpp::StringViewType<rpp::strview>);
    }
    { // the low-level number and utf helpers
        char buffer[32]; int written = rpp::_tostring(buffer, 4242);
        check("_tostring", rpp::strview(buffer, written) == "4242");
        check("to_inthx", rpp::to_inthx("ff", 2) == 255);
        check("utf8len", rpp::utf8len("abc") == 3);
        check("is_likely_utf8", !rpp::is_likely_utf8("abc", 3));
    }

#if RPP_ENABLE_UNICODE
    { // the facade re-exports these behind the same #if, so they need their own checks
        rpp::ustrview u{u"abc"};
        check("ustrview", u.len == 3 && rpp::strequals(u.str, u"abc", 3));
        check("utf16len", rpp::utf16len(u"abcd") == 4);
        check("to_string", rpp::to_string(u) == "abc");
        check("to_ustring", rpp::to_ustring("abc") == u"abc");
    }
#endif

    if (failed) { printf("FAIL: %d check(s)\n", failed); return 1; }
    printf("OK: every check passed\n");
    return 0;
}
