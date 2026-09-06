// C++20 module interface unit for <rpp/timepoint.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "timepoint.h"

export module rpp.timepoint;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.config;

export using ::time_now_seconds;

export namespace rpp {
    using rpp::sleep_ms;
    using rpp::sleep_us;
    using rpp::sleep_ns;
    using rpp::Duration;
    using rpp::TimePoint;
    using rpp::sleep_for;
    using rpp::sleep_until;
    using rpp::ClockType;
    using rpp::MILLIS_PER_SEC;
    using rpp::MICROS_PER_SEC;
    using rpp::NANOS_PER_SEC;
    using rpp::NANOS_PER_MILLI;
    using rpp::NANOS_PER_MICRO;
    using rpp::NANOS_PER_YEAR;
    using rpp::NANOS_PER_DAY;
    using rpp::NANOS_PER_HOUR;
    using rpp::NANOS_PER_MINUTE;
    using rpp::seconds_f;
    using rpp::seconds;
    using rpp::millis_f;
    using rpp::millis;
    using rpp::micros_f;
    using rpp::micros;
    using rpp::nanos;
}

export namespace rpp::duration_literals {
    using rpp::duration_literals::operator""_s;
    using rpp::duration_literals::operator""_ms;
    using rpp::duration_literals::operator""_us;
    using rpp::duration_literals::operator""_ns;
}
// GENERATED EXPORTS END
