// C++20 umbrella: `import rpp;` reaches every header-backed module through the groups below.
// No header backs rpp.std, so a file which wants the std names imports that one as well.
export module rpp;

export import rpp.core;
export import rpp.text;
export import rpp.numeric;
export import rpp.time;
export import rpp.containers;
export import rpp.io;
export import rpp.threading;
export import rpp.testing;
