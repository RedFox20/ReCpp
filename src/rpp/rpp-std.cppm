// Stands in for `import std;`, which gcc-14 does not ship. It carries the std names ReCpp
// puts in a public signature, so a consumer can import instead of including each header.
// The names sit in five parts, this unit re-exports all five, and a file which wants one
// part imports it by name. Five, because gcc-14 cannot write one, see BUGS.md B24.

export module rpp.std;

export import rpp.std.text;
export import rpp.std.containers;
export import rpp.std.memory;
export import rpp.std.threading;
export import rpp.std.core;
