// Stands in for `import std;`, which gcc-14 does not ship, and re-exports every part below.
// The parts exist because gcc-14 cannot write one unit carrying them all, see BUGS.md B24.

export module rpp.std;

export import rpp.std.text;
export import rpp.std.containers;
export import rpp.std.memory;
export import rpp.std.threading;
export import rpp.std.core;
