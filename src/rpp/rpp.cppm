// C++20 umbrella: `import rpp;` reaches every header-backed group below, and a file which
// wants the std names imports rpp.std as well. No header backs that one.
// rpp.testing stays out, because it overflows the gcc-14 source location budget through this
// unit on C++23. A test file writes `import rpp; import rpp.testing;`, see BUGS.md B25.
export module rpp;

export import rpp.core;
export import rpp.text;
export import rpp.numeric;
export import rpp.time;
export import rpp.containers;
export import rpp.io;
export import rpp.threading;
