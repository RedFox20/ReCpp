// C++20 umbrella: `import rpp;` reaches every group below. An importer includes its own std headers.
// rpp.testing stays out, because it overflows the gcc-14 location budget here, see BUGS.md B25.
export module rpp;

export import rpp.core;
export import rpp.text;
export import rpp.numeric;
export import rpp.time;
export import rpp.containers;
export import rpp.io;
export import rpp.threading;
