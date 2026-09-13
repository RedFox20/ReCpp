// C++20 module interface unit for the rpp.numeric headers, owned by tools/gen_module_exports.py.
// The headers stay in the global module fragment, so an importer and an includer share one entity.
module;

#include "math.h"
#include "minmax.h"
#include "vec.h"
#include "sort.h"

export module rpp.numeric;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::PI;
    using rpp::PIf;
    using rpp::SQRT2;
    using rpp::SQRT2f;
    using rpp::radf;
    using rpp::degf;
    using rpp::clamp;
    using rpp::lerp;
    using rpp::lerpInverse;
    using rpp::nearlyZero;
    using rpp::almostEqual;
    using rpp::clampZero;
    using rpp::min;
    using rpp::max;
    using rpp::abs;
    using rpp::sqrt;
    using rpp::min3;
    using rpp::max3;
    using rpp::Vector2;
    using rpp::vec2;
    using rpp::operator+;
    using rpp::operator-;
    using rpp::operator*;
    using rpp::operator/;
    using rpp::Vector2d;
    using rpp::vec2d;
    using rpp::Point;
    using rpp::point2;
    using rpp::RectF;
    using rpp::Rect;
    using rpp::Recti;
    using rpp::Vector3d;
    using rpp::Vector3;
    using rpp::vec3;
    using rpp::vec3d;
    using rpp::AngleAxis;
    using rpp::Matrix3;
    using rpp::Matrix4;
    using rpp::Vector4;
    using rpp::vec4;
    using rpp::rect;
    using rpp::_Matrix3RowVis;
    using rpp::_Matrix4RowVis;
    using rpp::PerspectiveViewport;
    using rpp::Color;
    using rpp::Color3;
    using rpp::IdVector3;
    using rpp::BoundingBox;
    using rpp::BoundingSphere;
    using rpp::Ray;
    using rpp::to_string;
    using rpp::sort_comparison;
    using rpp::contiguous_container;
    using rpp::container_element_t;
    using rpp::insertion_sort;
    using rpp::sort;
}
// GENERATED EXPORTS END
