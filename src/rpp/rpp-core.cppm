// Group umbrella: the foundational utilities every other group builds on.
// tools/gen_module_exports.py checks that the groups partition every module.
export module rpp.core;

export import rpp.config;
export import rpp.debugging;
export import rpp.source_loc;
export import rpp.traits;
export import rpp.type_traits;
export import rpp.predicates;
export import rpp.scopeguard;
export import rpp.delegate;
export import rpp.proc_utils;
export import rpp.stack_trace;
export import rpp.endian;
export import rpp.bitutils;
