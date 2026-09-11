// C++20 umbrella module: `import rpp;` reaches every ReCpp module through the groups below.
// A macro never crosses a module, so the test macros still need <rpp/tests.macros.h>.
export module rpp;

export import rpp.core;
export import rpp.text;
export import rpp.numeric;
export import rpp.time;
export import rpp.containers;
export import rpp.io;
export import rpp.threading;
export import rpp.testing;
