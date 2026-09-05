// C++20 module interface unit for <rpp/vec.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "vec.h"

export module rpp.vec;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.math;
export import rpp.strview;

export namespace rpp {
    using rpp::Vector2;
    using rpp::vec2;
    using rpp::operator+;
    using rpp::operator-;
    using rpp::operator*;
    using rpp::operator/;
    using rpp::clamp;
    using rpp::lerp;
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
}
// GENERATED EXPORTS END
