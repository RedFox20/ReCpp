/**
 * Basic Vector Math, Copyright (c) 2015 - Jorma Rebane
 */
#include "vec.h"
#include <cstdio>

namespace rpp
{
    using std::swap;

    // Clang doesn't support C++17 ODR-used constexpr simplifications
    // so we still require an out-of-class definition for them:
    #if __clang__
        const float2 Vector2::ZERO;
        const float2 Vector2::ONE;
        const float2 Vector2::RIGHT;
        const float2 Vector2::UP;
        // ----
        const double2 Vector2d::ZERO;
        const double2 Vector2d::ONE;
        const double2 Vector2d::RIGHT;
        const double2 Vector2d::UP;
        // ----
        const int2 Point::ZERO; // Outer declaration required
        // ----
        const float4 Rect::ZERO; // Outer declaration required
        // ----
        const float3 Vector3::ZERO;
        const float3 Vector3::ONE;
        
        const float3 Vector3::LEFT;
        const float3 Vector3::RIGHT;
        const float3 Vector3::UP;
        const float3 Vector3::DOWN;
        const float3 Vector3::FORWARD;
        const float3 Vector3::BACKWARD;

        const float3 Vector3::XAXIS;
        const float3 Vector3::YAXIS;
        const float3 Vector3::ZAXIS;
        
        const float3 Vector3::WHITE;
        const float3 Vector3::BLACK;
        const float3 Vector3::RED;
        const float3 Vector3::GREEN;
        const float3 Vector3::BLUE;
        const float3 Vector3::YELLOW;
        const float3 Vector3::ORANGE;
        const float3 Vector3::MAGENTA;
        const float3 Vector3::CYAN;
        const float3 Vector3::SWEETGREEN;
        const float3 Vector3::CORNFLOWERBLUE;
        // ----
        const double3 Vector3d::ZERO;
        // ----
        const float4 Vector4::ZERO;
        const float4 Vector4::ONE;
        
        const float4 Vector4::WHITE;
        const float4 Vector4::BLACK;
        const float4 Vector4::RED;
        const float4 Vector4::GREEN;
        const float4 Vector4::BLUE;
        const float4 Vector4::YELLOW;
        const float4 Vector4::ORANGE;
        const float4 Vector4::MAGENTA;
        const float4 Vector4::CYAN;
        const float4 Vector4::SWEETGREEN;
        const float4 Vector4::CORNFLOWERBLUE;
    #endif

    /////////////////////////////////////////////////////////////////////////////////////

    template<class T> static constexpr T inverse_length(const T magnitude, const T& x, const T& y)
    {
        const T len = sqrt(x*x + y*y);
        return nearlyZero(len) ? 0 : magnitude / len;
    }

    template<class T> static constexpr T inverse_length(const T magnitude, const T& x, const T& y, const T& z)
    {
        const T len = sqrt(x*x + y*y + z*z);
        return nearlyZero(len) ? 0 : magnitude / len;
    }

    /////////////////////////////////////////////////////////////////////////////////////

    void Vector2::print() const
    {
        char buffer[32];
        puts(toString(buffer));
    }
    
    const char* Vector2::toString() const
    {
        static char buffer[32];
        return toString(buffer, sizeof(buffer));
    }
    
    char* Vector2::toString(char* buffer) const
    {
        return toString(buffer, 32);
    }
    
    char* Vector2::toString(char* buffer, int size) const
    {
        snprintf(buffer, size, "{%.3g;%.3g}", x, y);
        return buffer;
    }
    
    void Vector2::set(float newX, float newY)
    {
        x = newX; y = newY;
    }

    float Vector2::length() const
    {
        return sqrt(x*x + y*y);
    }

    float Vector2::sqlength() const
    {
        return x*x + y*y;
    }

    void Vector2::normalize()
    {
        auto inv = inverse_length(1.0f, x, y);
        x *= inv; y *= inv;
    }

    void Vector2::normalize(const float magnitude)
    {
        auto inv = inverse_length(magnitude, x, y);
        x *= inv; y *= inv;
    }

    Vector2 Vector2::normalized() const
    {
        auto inv = inverse_length(1.0f, x, y);
        return { x*inv, y*inv };
    }

    Vector2 Vector2::normalized(const float magnitude) const
    {
        auto inv = inverse_length(magnitude, x, y);
        return { x*inv, y*inv };
    }

    float Vector2::dot(const Vector2& v) const
    {
        return x*v.x + y*v.y;
    }

    Vector2 Vector2::right(const Vector2& b, float magnitude) const
    {
        return Vector2{ y - b.y, b.x - x }.normalized(magnitude);

    }

    Vector2 Vector2::left(const Vector2& b, float magnitude) const
    {
        return Vector2{ b.y - y, x - b.x }.normalized(magnitude);
    }

    Vector2 Vector2::right(float magnitude) const
    {
        return Vector2{ y, -x }.normalized(magnitude);
    }

    Vector2 Vector2::left(float magnitude) const
    {
        return Vector2{ -y, x }.normalized(magnitude);
    }

    bool Vector2::almostZero() const
    {
        return nearlyZero(x) && nearlyZero(y);
    }

    bool Vector2::almostEqual(const Vector2& b) const
    {
        return nearlyZero(x - b.x) && nearlyZero(y - b.y);
    }

    /////////////////////////////////////////////////////////////////////////////////////

    void Vector2d::print() const
    {
        char buffer[32];
        puts(toString(buffer));
    }
    
    const char* Vector2d::toString() const
    {
        static char buffer[32];
        return toString(buffer, sizeof(buffer));
    }
    
    char* Vector2d::toString(char* buffer) const
    {
        return toString(buffer, 32);
    }
    
    char* Vector2d::toString(char* buffer, int size) const
    {
        snprintf(buffer, size, "{%.3g;%.3g}", x, y);
        return buffer;
    }
    
    void Vector2d::set(double newX, double newY)
    {
        x=newX; y=newY;
    }

    double Vector2d::length() const
    {
        return sqrt(x*x + y*y);
    }

    double Vector2d::sqlength() const
    {
        return x*x + y*y;
    }

    void Vector2d::normalize()
    {
        auto inv = inverse_length(1.0, x, y);
        x *= inv; y *= inv;
    }

    void Vector2d::normalize(const double magnitude)
    {
        auto inv = inverse_length(magnitude, x, y);
        x *= inv; y *= inv;
    }

    Vector2d Vector2d::normalized() const
    {
        auto inv = inverse_length(1.0, x, y);
        return{ x*inv, y*inv };
    }

    Vector2d Vector2d::normalized(const double magnitude) const
    {
        auto inv = inverse_length(magnitude, x, y);
        return { x*inv, y*inv };
    }

    double Vector2d::dot(const Vector2d& v) const
    {
        return x*v.x + y*v.y;
    }

    Vector2d Vector2d::right(const Vector2d& b, double magnitude) const
    {
        return Vector2d{ y - b.y, b.x - x }.normalized(magnitude);

    }

    Vector2d Vector2d::left(const Vector2d& b, double magnitude) const
    {
        return Vector2d{ b.y - y, x - b.x }.normalized(magnitude);
    }

    Vector2d Vector2d::right(double magnitude) const
    {
        return Vector2d{ y, -x }.normalized(magnitude);
    }

    Vector2d Vector2d::left(double magnitude) const
    {
        return Vector2d{ -y, x }.normalized(magnitude);
    }

    bool Vector2d::almostZero() const
    {
        return nearlyZero(x) && nearlyZero(y);
    }

    bool Vector2d::almostEqual(const Vector2d& b) const
    {
        return nearlyZero(x - b.x) && nearlyZero(y - b.y);
    }
    
    /////////////////////////////////////////////////////////////////////////////////////

    const char* Point::toString() const
    {
        static char buf[32]; return toString(buf, 32);
    }

    char* Point::toString(char* buffer) const
    {
        return toString(buffer, 32);
    }

    char* Point::toString(char* buffer, int size) const
    {
        snprintf(buffer, size, "{%d;%d}", x, y);
        return buffer;
    }

    /////////////////////////////////////////////////////////////////////////////////////

    void Rect::print() const
    {
        char buffer[64];
        puts(toString(buffer));
    }

    const char* Rect::toString() const
    {
        static char buffer[64];
        return toString(buffer, sizeof(buffer));
    }

    char* Rect::toString(char* buffer) const
    {
        return toString(buffer, 64);
    }

    char* Rect::toString(char* buffer, int bufSize) const
    {
        snprintf(buffer, bufSize, "{position %.3g;%.3g size %.3g;%.3g}", x, y, w, h);
        return buffer;
    }

    bool Rect::hitTest(const Vector2& position) const
    {
        return x <= position.x && y <= position.y && 
               position.x <= x + w && 
               position.y <= y + h;
    }

    bool Rect::hitTest(const float xPos, const float yPos) const
    {
        return x <= xPos && y <= yPos && 
               xPos <= x + w && 
               yPos <= y + h;
    }

    bool Rect::hitTest(const Rect& r) const
    {
        return x <= r.x && y <= r.y &&
               r.x + r.w <= x + w &&
               r.y + r.h <= y + h;
    }

    bool Rect::intersectsWith(const Rect& r) const
    {
        // this is just a collision test
        return x < r.right()  && right()  > r.x
            && y < r.bottom() && bottom() > r.y;
    }

    void Rect::extrude(float extrude)
    {
        x -= extrude;
        y -= extrude;
        if ((w += extrude*2) < 0.0f) w = 0.0f;
        if ((h += extrude*2) < 0.0f) h = 0.0f;
    }

    void Rect::extrude(const Vector2& extrude)
    {
        x -= extrude.x;
        y -= extrude.y;
        if ((w += extrude.x*2) < 0.0f) w = 0.0f;
        if ((h += extrude.y*2) < 0.0f) h = 0.0f;
    }

    Rect Rect::joined(const Rect& b) const
    {
        float newX = min(x, b.x);
        float newY = min(y, b.y);
        float newW = max(x + w, b.x + b.w) - newX;
        float newH = max(y + h, b.y + b.h) - newY;
        return{ newX, newY, newW, newH };
    }

    void Rect::join(const Rect & b)
    {
        float ax = x, ay = y;
        x = min(ax, b.x);
        w = max(ax + w, b.x + b.w) - x;
        y = min(ay, b.y);
        h = max(ay + h, b.y + b.h) - y;
    }

    void Rect::clip(const Rect& frame)
    {
        float r = right();
        float b = bottom();
        float fr = frame.right();
        float fb = frame.bottom();
        if (x < frame.x) x = frame.x; else if (x > fr) x = fr;
        if (y < frame.y) y = frame.y; else if (y > fb) y = fb;
        if (r > fr) w = fr - x;
        if (b > fb) h = fb - y;
    }

    /////////////////////////////////////////////////////////////////////////////////////

    void Vector3::set(float newX, float newY, float newZ)
    {
        x=newX; y=newY; z=newZ;
    }

    float Vector3::length() const
    {
        return sqrt(x*x + y*y + z*z);
    }

    float Vector3::sqlength() const
    {
        return x*x + y*y + z*z;
    }

    float Vector3::distanceTo(Vector3 v) const
    {
        float dx = x - v.x;
        float dy = y - v.y;
        float dz = z - v.z;
        return sqrt(dx*dx + dy*dy + dz*dz);
    }

    float Vector3::sqDistanceTo(Vector3 v) const
    {
        float dx = x - v.x;
        float dy = y - v.y;
        float dz = z - v.z;
        return dx*dx + dy*dy + dz*dz;
    }

    void Vector3::normalize()
    {
        auto inv = inverse_length(1.0f, x, y, z);
        x *= inv; y *= inv; z *= inv;
    }

    void Vector3::normalize(const float magnitude)
    {
        auto inv = inverse_length(magnitude, x, y, z);
        x *= inv; y *= inv; z *= inv;
    }

    Vector3 Vector3::normalized() const
    {
        auto inv = inverse_length(1.0f, x, y, z);
        return { x*inv, y*inv, z*inv };
    }
    Vector3 Vector3::normalized(const float magnitude) const
    {
        auto inv = inverse_length(magnitude, x, y, z);
        return { x*inv, y*inv, z*inv };
    }

    Vector3 Vector3::cross(Vector3 v) const
    {
        return { y*v.z - v.y*z, z*v.x - v.z*x, x*v.y - v.x*y };
    }

    float Vector3::dot(Vector3 v) const
    {
        return x*v.x + y*v.y + z*v.z;
    }

    Vector3 Vector3::mask() const
    {
        return { x > 0.0f ? 0.f : 1.f, 
                 y > 0.0f ? 0.f : 1.f, 
                 z > 0.0f ? 0.f : 1.f };
    }

    Vector3 Vector3::toEulerAngles() const
    {
        float pitchAdjacent = sqrt(x*x + z*z); 
        float pitch = atan2(pitchAdjacent, y);
        float yaw   = atan2(x, z);
        return Vector3{/*roll:*/0.0f, pitch, yaw};
    }

    void Vector3::print() const
    {
        char buffer[48];
        puts(toString(buffer));
    }

    const char* Vector3::toString() const
    {
        static char buffer[48];
        return toString(buffer, sizeof(buffer));
    }

    char* Vector3::toString(char* buffer) const
    {
        return toString(buffer, 48);
    }

    char* Vector3::toString(char* buffer, int size) const
    {
        snprintf(buffer, size, "{%.3g;%.3g;%.3g}", x, y, z);
        return buffer;
    }


    /** @return TRUE if this vector is almost zero, with all components abs < 0.0001 */
    bool Vector3::almostZero() const
    {
        return nearlyZero(x) && nearlyZero(y) && nearlyZero(z);
    }

    bool Vector3::almostEqual(Vector3 v) const
    {
        return nearlyZero(x - v.x) && nearlyZero(y - v.y) && nearlyZero(z - v.z);
    }

    Vector3 Vector3::smoothColor(Vector3 src, Vector3 dst, float ratio)
    {
        return { src.x * (1 - ratio) + dst.x * ratio,
                 src.y * (1 - ratio) + dst.y * ratio,
                 src.z * (1 - ratio) + dst.z * ratio };
    }

    Vector3 Vector3::parseColor(const strview& s) noexcept
    {
        if (!s)
            return WHITE;

        if (s[0] == '#')
            return Vector4::HEX(s).rgb;

        if (isalpha(s[0]))
            return Vector4::NAME(s).rgb;

        return Vector4::NUMBER(s).rgb;
    }

    /////////////////////////////////////////////////////////////////////////////////////
    
    void Vector3d::set(double newX, double newY, double newZ)
    {
        x=newX; y=newY; z=newZ;
    }

    double Vector3d::length() const
    {
        return sqrt(x*x + y*y + z*z);
    }

    double Vector3d::sqlength() const
    {
        return x*x + y*y + z*z;
    }

    double Vector3d::distanceTo(const Vector3d& v) const
    {
        double dx = x - v.x;
        double dy = y - v.y;
        double dz = z - v.z;
        return sqrt(dx*dx + dy*dy + dz*dz);
    }

    void Vector3d::normalize()
    {
        auto inv = inverse_length(1.0, x, y, z);
        x *= inv; y *= inv; z *= inv;
    }

    void Vector3d::normalize(const double magnitude)
    {
        auto inv = inverse_length(magnitude, x, y, z);
        x *= inv; y *= inv; z *= inv;
    }

    Vector3d Vector3d::normalized() const
    {
        auto inv = inverse_length(1.0, x, y, z);
        return { x*inv, y*inv, z*inv };
    }
    Vector3d Vector3d::normalized(const double magnitude) const
    {
        auto inv = inverse_length(magnitude, x, y, z);
        return { x*inv, y*inv, z*inv };
    }

    Vector3d Vector3d::cross(const Vector3d& b) const
    {
        return { y*b.z - b.y*z, z*b.x - b.z*x, x*b.y - b.x*y };
    }

    double Vector3d::dot(const Vector3d& b) const
    {
        return x*b.x + y*b.y + z*b.z;
    }
    
    void Vector3d::print() const
    {
        char buffer[48];
        puts(toString(buffer));
    }
    
    const char* Vector3d::toString() const
    {
        static char buffer[48];
        return toString(buffer, sizeof(buffer));
    }
    
    char* Vector3d::toString(char* buffer) const
    {
        return toString(buffer, 48);
    }
    
    char* Vector3d::toString(char* buffer, int size) const
    {
        snprintf(buffer, size, "{%.3g;%.3g;%.3g}", x, y, z);
        return buffer;
    }

    bool Vector3d::almostZero() const
    {
        return nearlyZero(x) && nearlyZero(y) && nearlyZero(z);
    }

    bool Vector3d::almostEqual(const Vector3d& v) const
    {
        return nearlyZero(x - v.x) && nearlyZero(y - v.y) && nearlyZero(z - v.z);
    }


    /////////////////////////////////////////////////////////////////////////////////////


    AngleAxis AngleAxis::fromVectors(Vector3 a, Vector3 b)
    {
        if (a.almostZero() || b.almostZero())
            return { Vector3::ZERO, 0.0f };
        a.normalize();
        b.normalize();
        float cosTheta = a.dot(b);
        if (rpp::almostEqual(cosTheta, 1.0f))
            return { Vector3::ZERO, 0.0f };
        float theta = rpp::degf(std::acos(cosTheta));
        Vector3 axis = a.cross(b).normalized();
        return { axis, -theta };
    }


    /////////////////////////////////////////////////////////////////////////////////////


    bool Vector4::almostZero() const
    {
        return nearlyZero(x) && nearlyZero(y) && nearlyZero(z) && nearlyZero(w);
    }

    bool Vector4::almostEqual(const Vector4& v) const
    {
        return nearlyZero(x - v.x) && nearlyZero(y - v.y) && nearlyZero(z - v.z) && nearlyZero(w - v.w);
    }

    void Vector4::set(float newX, float newY, float newZ, float newW)
    {
        x = newX; y = newY; z = newZ; w = newW;
    }

    float Vector4::dot(const Vector4& v) const
    {
        return x*v.x + y*v.y + z*v.z + w*v.w;
    }

    void Vector4::print() const
    {
        char buffer[48];
        puts(toString(buffer));
    }

    const char* Vector4::toString() const
    {
        static char buffer[64];
        return toString(buffer, sizeof(buffer));
    }

    char* Vector4::toString(char* buffer) const
    {
        return toString(buffer, 64);
    }

    char* Vector4::toString(char* buffer, int size) const
    {
        snprintf(buffer, size, "{%.3g;%.3g;%.3g;%.3g}", x, y, z, w);
        return buffer;
    }

    Vector3 Vector4::quatToEulerAngles() const
    {
        Vector3 angles = quatToEulerRadians();
        angles.x = degf(angles.x);
        angles.y = degf(angles.y);
        angles.z = degf(angles.z);
        return angles;
    }

    Vector3 Vector4::quatToEulerRadians() const
    {
        return {
            atan2(2 * (x*y + z*w), 1 - 2 * (y*y + z*z)),
            asin( 2 * (x*z - w*y)),
            atan2(2 * (x*w + y*z), 1 - 2 * (z*z + w*w))
        };
    }

    Vector4 Vector4::fromAngleAxis(float degrees, float x, float y, float z)
    {
        const float r = radf(degrees) * 0.5f;
        const float s = sinf(r);
        return Vector4(x*s, y*s, z*s, cosf(r));
    }

    Vector4 Vector4::fromRadianAxis(float radians, float x, float y, float z)
    {
        const float r = radians * 0.5f;
        const float s = sinf(r);
        return Vector4(x*s, y*s, z*s, cosf(r));
    }

    Vector4 Vector4::fromRotationAngles(const Vector3& rotationDegrees)
    {
        Vector4 q = fromAngleAxis(rotationDegrees.x, 1.0f, 0.0f, 0.0f);
                q = fromAngleAxis(rotationDegrees.y, 0.0f, 1.0f, 0.0f) * q;
        return      fromAngleAxis(rotationDegrees.z, 0.0f, 0.0f, 1.0f) * q;
    }

    Vector4 Vector4::fromRotationRadians(const Vector3& rotationRadians)
    {
        Vector4 q = fromRadianAxis(rotationRadians.x, 1.0f, 0.0f, 0.0f);
                q = fromRadianAxis(rotationRadians.y, 0.0f, 1.0f, 0.0f) * q;
        return      fromRadianAxis(rotationRadians.z, 0.0f, 0.0f, 1.0f) * q;
    }

    Vector4 Vector4::rotate(const Vector4& q) const
    {
        return {
            q.w*w - q.x*x - q.y*y - q.z*z,
            q.w*x + q.x*w + q.y*z - q.z*y,
            q.w*y + q.y*w + q.z*x - q.x*z,
            q.w*z + q.z*w + q.x*y - q.y*x
        };
    }

    Vector4 Vector4::smoothColor(Vector4 src, Vector4 dst, float ratio)
    {
        return {
            src.x * (1 - ratio) + dst.x * ratio,
            src.y * (1 - ratio) + dst.y * ratio,
            src.z * (1 - ratio) + dst.z * ratio,
            src.w * (1 - ratio) + dst.w * ratio
        };
    }

    Vector4 Vector4::HEX(const strview& s) noexcept
    {
        Color c = WHITE;
        if (s[0] == '#')
        {
            strview r = s.substr(1, 2);
            strview g = s.substr(3, 2);
            strview b = s.substr(5, 2);
            strview a = s.substr(7, 2);
            c.r = r.to_int_hex() / 255.0f;
            c.g = g.to_int_hex() / 255.0f;
            c.b = b.to_int_hex() / 255.0f;
            c.a = a ? a.to_int_hex() / 255.0f : 1.0f;
        }
        return c;
    }

    Vector4 Vector4::NAME(const strview& s) noexcept
    {
        if (s.equalsi("white"))  return WHITE;
        if (s.equalsi("black"))  return BLACK;
        if (s.equalsi("red"))    return RED;
        if (s.equalsi("green"))  return GREEN;
        if (s.equalsi("blue"))   return BLUE;
        if (s.equalsi("yellow")) return YELLOW;
        if (s.equalsi("orange")) return ORANGE;
        return WHITE;
    }

    Vector4 Vector4::NUMBER(strview s) noexcept
    {
        strview r = s.next(' ');
        strview g = s.next(' ');
        strview b = s.next(' ');
        strview a = s.next(' ');

        Color c = {
            r.to_float(),
            g.to_float(),
            b.to_float(),
            a ? a.to_float() : 1.0f
        };
        if (c.r > 1.0f) c.r /= 255.0f;
        if (c.g > 1.0f) c.g /= 255.0f;
        if (c.b > 1.0f) c.b /= 255.0f;
        if (c.a > 1.0f) c.a /= 255.0f;
        return c;
    }

    Vector4 Vector4::parseColor(const strview& s) noexcept
    {
        if (!s)
            return WHITE;

        if (s[0] == '#')
            return HEX(s);

        if (isalpha(s[0]))
            return NAME(s);

        return NUMBER(s);
    }


    /////////////////////////////////////////////////////////////////////////////////////


    static const Matrix3 IDENTITY3x3 = {
        { 1, 0, 0 },
        { 0, 1, 0 },
        { 0, 0, 1 },
    };

    const Matrix3& Matrix3::Identity()
    {
        return IDENTITY3x3;
    }

    Matrix3& Matrix3::loadIdentity()
    {
        return (*this = IDENTITY3x3);
    }

    Matrix3& Matrix3::multiply(const Matrix3& mb)
    {
        float a00 = m00, a01 = m01, a02 = m02;
        float a10 = m10, a11 = m11, a12 = m12;
        float a20 = m20, a21 = m21, a22 = m22;
        float B00 = m00, B01 = m01, B02 = m02;
        float B10 = m10, B11 = m11, B12 = m12;
        float B20 = m20, B21 = m21, B22 = m22;
        m00 = a00 * B00 + a10 * B01 + a20 * B02;
        m01 = a01 * B00 + a11 * B01 + a21 * B02;
        m02 = a02 * B00 + a12 * B01 + a22 * B02;
        m10 = a00 * B10 + a10 * B11 + a20 * B12;
        m11 = a01 * B10 + a11 * B11 + a21 * B12;
        m12 = a02 * B10 + a12 * B11 + a22 * B12;
        m20 = a00 * B20 + a10 * B21 + a20 * B22;
        m21 = a01 * B20 + a11 * B21 + a21 * B22;
        m22 = a02 * B20 + a12 * B21 + a22 * B22;
        return *this;
    }

    Matrix3 Matrix3::operator*(const Matrix3& mb) const
    {
        float a00 = m00, a01 = m01, a02 = m02;
        float a10 = m10, a11 = m11, a12 = m12;
        float a20 = m20, a21 = m21, a22 = m22;
        float B00 = m00, B01 = m01, B02 = m02;
        float B10 = m10, B11 = m11, B12 = m12;
        float B20 = m20, B21 = m21, B22 = m22;
        return {
            a00 * B00 + a10 * B01 + a20 * B02,
            a01 * B00 + a11 * B01 + a21 * B02,
            a02 * B00 + a12 * B01 + a22 * B02,
            a00 * B10 + a10 * B11 + a20 * B12,
            a01 * B10 + a11 * B11 + a21 * B12,
            a02 * B10 + a12 * B11 + a22 * B12,
            a00 * B20 + a10 * B21 + a20 * B22,
            a01 * B20 + a11 * B21 + a21 * B22,
            a02 * B20 + a12 * B21 + a22 * B22,
        };
    }

    Vector3 Matrix3::operator*(const Vector3& v) const
    {
        return {
            m00 * v.x + m10 * v.y + m20 * v.z,
            m01 * v.x + m11 * v.y + m21 * v.z,
            m02 * v.x + m12 * v.y + m22 * v.z
        };
    }
    
    // https://en.wikipedia.org/wiki/Rotation_matrix#Rotation_matrix_from_axis_and_angle
    Matrix3& Matrix3::fromAngleAxis(float angle, float x, float y, float z)
    {
        float radians = radf(angle);
        float c = cosf(radians);
        float s = sinf(radians);
        float c1 = 1 - c;
        m00 = c + x*x*c1;   m01 = x*y*c1 - z*s; m02 = x*z*c1 + y*s;
        m10 = y*x*c1 + z*s; m11 = c + y*y*c1;   m12 = y*z*c1 - x*s;
        m20 = z*x*c1 - y*s; m21 = z*y*c1 + x*s; m22 = c + z*z*c1;
        return *this;
    }

    Matrix3& Matrix3::transpose()
    {
        swap(m01, m10);
        swap(m02, m20);
        swap(m12, m21);
        return *this;
    }

    Matrix3 Matrix3::transposed() const
    {
        return {
            m00, m10, m20,
            m01, m11, m21,
            m02, m12, m22,
        };
    }

    float Matrix3::norm() const
    {
        float s = m00*m00 + m10*m10 + m20*m20 
                + m01*m01 + m11*m11 + m21*m21
                + m02*m02 + m12*m12 + m22*m22;
        return sqrtf(s);
    }

    float Matrix3::norm(const Matrix3& b) const
    {
        float f0 = m00-b.m00, f1 = m10-b.m10, f2 = m20-b.m20;
        float f3 = m01-b.m01, f4 = m11-b.m11, f5 = m21-b.m21;
        float f6 = m02-b.m02, f7 = m12-b.m12, f8 = m22-b.m22;
        float s = f0*f0 + f1*f1 + f2*f2 
                + f3*f3 + f4*f4 + f5*f5
                + f6*f6 + f7*f7 + f8*f8;
        return sqrtf(s);
    }

    bool Matrix3::isRotationMatrix() const
    {
        Matrix3 shouldBeIdentity = transposed() * (*this);
        return IDENTITY3x3.norm(shouldBeIdentity) < 1e-6;
    }

    Vector3 Matrix3::toEulerAngles() const
    {
        Vector3 angles = toEulerRadians();
        angles.x = degf(angles.x);
        angles.y = degf(angles.y);
        angles.z = degf(angles.z);
        return angles;
    }

    // https://www.learnopencv.com/rotation-matrix-to-euler-angles/
    Vector3 Matrix3::toEulerRadians() const
    {
        float sy = sqrtf(m00*m00 + m10*m10);
        Vector3 angles;
        if (sy < 1e-6) // if singular
        {
            angles.x = atan2(-m12, m11);
            angles.y = atan2(-m20, sy);
            angles.z = 0.0f;
        }
        else
        {
            angles.x = atan2(m21, m22);
            angles.y = atan2(-m20, sy);
            angles.z = atan2(m10, m00);
        }
        return angles;
    }

    Matrix3& Matrix3::fromRotationRadians(const Vector3& eulerRadians)
    {
        return (*this = createRotationFromRadians(eulerRadians));
    }

    Matrix3 Matrix3::createRotationFromRadians(const Vector3& eulerRadians)
    {
        float sinx = sin(eulerRadians.x);
        float cosx = cos(eulerRadians.x);
        float siny = sin(eulerRadians.y);
        float cosy = cos(eulerRadians.y);
        float sinz = sin(eulerRadians.z);
        float cosz = cos(eulerRadians.z);

        Matrix3 R_x {
            1,  0,     0,
            0,  cosx, -sinx,
            0,  sinx,  cosx
        };
        Matrix3 R_y {
            cosy,  0,  siny,
            0,     1,  0,
           -siny,  0,  cosy
        };
        Matrix3 R_z {
            cosz,  -sinz,  0,
            sinz,   cosz,  0,
            0,      0,     1
        };
        // Combined rotation matrix
        R_z.multiply(R_y);
        R_z.multiply(R_x);
        return R_z;
    }

    void Matrix3::print() const
    {
        char buffer[256];
        puts(toString(buffer));
    }

    const char* Matrix3::toString() const
    {
        static char buffer[256];
        return toString(buffer, sizeof(buffer));
    }

    char* Matrix3::toString(char* buffer) const
    {
        return toString(buffer, 256);
    }

    char* Matrix3::toString(char* buffer, int size) const
    {
        int len = snprintf(buffer, size_t(size), "{\n");
        for (auto v : r)
            len += snprintf(buffer+len, size_t(size-len), " %8.3f,%8.3f,%8.3f\n",
                                                            v.x  , v.y , v.z);
        snprintf(buffer+len, size_t(size-len), "}");
        return buffer;
    }


    /////////////////////////////////////////////////////////////////////////////////////


    static const Matrix4 IDENTITY4x4 = {
        { 1, 0, 0, 0 },
        { 0, 1, 0, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 0, 1 },
    };

    const Matrix4& Matrix4::Identity()
    {
        return IDENTITY4x4;
    }

    Matrix4& Matrix4::loadIdentity()
    {
        return (*this = IDENTITY4x4);
    }

    Matrix4& Matrix4::multiply(const Matrix4& mb)
    {
        const Vector4 a0 = r0;
        const Vector4 a1 = r1;
        const Vector4 a2 = r2;
        const Vector4 a3 = r3;
        const Vector4 b0 = mb.r0;
        const Vector4 b1 = mb.r1;
        const Vector4 b2 = mb.r2;
        const Vector4 b3 = mb.r3;
        r0 = (a0*b0.x + a1*b0.y) + (a2*b0.z + a3*b0.w);
        r1 = (a0*b1.x + a1*b1.y) + (a2*b1.z + a3*b1.w);
        r2 = (a0*b2.x + a1*b2.y) + (a2*b2.z + a3*b2.w);
        r3 = (a0*b3.x + a1*b3.y) + (a2*b3.z + a3*b3.w);
        return *this;
    }

    Matrix4 Matrix4::operator*(const Matrix4& mb) const
    {
        const Vector4 a0 = r0;
        const Vector4 a1 = r1;
        const Vector4 a2 = r2;
        const Vector4 a3 = r3;
        const Vector4 b0 = mb.r0;
        const Vector4 b1 = mb.r1;
        const Vector4 b2 = mb.r2;
        const Vector4 b3 = mb.r3;
        Matrix4 mc;
        mc.r0 = (a0*b0.x + a1*b0.y) + (a2*b0.z + a3*b0.w);
        mc.r1 = (a0*b1.x + a1*b1.y) + (a2*b1.z + a3*b1.w);
        mc.r2 = (a0*b2.x + a1*b2.y) + (a2*b2.z + a3*b2.w);
        mc.r3 = (a0*b3.x + a1*b3.y) + (a2*b3.z + a3*b3.w);
        return mc;
    }

    Vector3 Matrix4::operator*(const Vector3& v) const
    {
        return {
                (m00*v.x) + (m10*v.y) + (m20*v.z) + m30,
                (m01*v.x) + (m11*v.y) + (m21*v.z) + m31,
                (m02*v.x) + (m12*v.y) + (m22*v.z) + m32
        };
    }
    Vector4 Matrix4::operator*(const Vector4& v) const
    {
        return {
                (m00*v.x) + (m10*v.y) + (m20*v.z) + (m30*v.w),
                (m01*v.x) + (m11*v.y) + (m21*v.z) + (m31*v.w),
                (m02*v.x) + (m12*v.y) + (m22*v.z) + (m32*v.w),
                (m03*v.x) + (m13*v.y) + (m23*v.z) + (m33*v.w)
        };
    }

    Matrix4& Matrix4::translate(const Vector3& offset)
    {
        r3.xyz = this->operator*(offset);
        return *this;
    }

    //Matrix4& Matrix4::rotate(float angleDegs, const Vector3& rotationAxis)
    //{
    //    float rad = radf(angleDegs);
    //    float c = cosf(rad);
    //    const Vector3 axis  = rotationAxis.normalized();
    //    const Vector3 temp  = axis * (1.0f - c);
    //    const Vector3 sAxis = axis * sinf(rad);
    //    const Vector3 rot0 = {  c + temp.x*axis.x,
    //                            temp.x*axis.y + sAxis.z,
    //                            temp.x*axis.z - sAxis.y, };
    //    const Vector3 rot1 = {  temp.y*axis.x - sAxis.z,
    //                            c + temp.y*axis.y,
    //                            temp.y*axis.z + sAxis.x, };
    //    const Vector3 rot2 = {  temp.z*axis.x + sAxis.y,
    //                            temp.z*axis.y - sAxis.x,
    //                            c + temp.z*axis.z };

    //    const Vector4 new0 = (r0 * rot0.x) + (r1 * rot0.y) + (r2 * rot0.z);
    //    const Vector4 new1 = (r0 * rot1.x) + (r1 * rot1.y) + (r2 * rot1.z);
    //    const Vector4 new2 = (r0 * rot2.x) + (r1 * rot2.y) + (r2 * rot2.z);
    //    r0 = new0;
    //    r1 = new1;
    //    r2 = new2;
    //    return *this;
    //}

    Matrix4& Matrix4::rotate(float angleDegs, const Vector3& rotationAxis)
    {
        float a = radf(angleDegs);		
        float c = cos(a);
        float s = sin(a);
        Vector3 axis = rotationAxis.normalized();
        Vector3 temp = (1.0f - c) * axis;

        Matrix4 rot;
        rot.m00 = c + temp.x * axis.x;
        rot.m01 = 0 + temp.x * axis.y + s * axis.z;
        rot.m02 = 0 + temp.x * axis.z - s * axis.y;

        rot.m10 = 0 + temp.y * axis.x - s * axis.z;
        rot.m11 = c + temp.y * axis.y;
        rot.m12 = 0 + temp.y * axis.z + s * axis.x;

        rot.m20 = 0 + temp.z * axis.x + s * axis.y;
        rot.m21 = 0 + temp.z * axis.y - s * axis.x;
        rot.m22 = c + temp.z * axis.z;

        const Vector4 new0 = r0 * rot.m00 + r1 * rot.m01 + r2 * rot.m02;
        const Vector4 new1 = r0 * rot.m10 + r1 * rot.m11 + r2 * rot.m12;
        const Vector4 new2 = r0 * rot.m20 + r1 * rot.m21 + r2 * rot.m22;
        r0 = new0;
        r1 = new1;
        r2 = new2;
        return *this;
    }

    Matrix4& Matrix4::rotate(float angleDegs, float x, float y, float z)
    {
        return rotate(angleDegs, {x,y,z});
    }

    Matrix4& Matrix4::scale(const Vector3& scale)
    {
        m00 *= scale.x;
        m11 *= scale.y;
        m22 *= scale.z;
        return *this;
    }

    Matrix4& Matrix4::setOrtho(float left, float right, float bottom, float top)
    {
        const float far  = 1000.0f;
        const float near = -1000.0f;
        const float rl = right - left;
        const float tb = top - bottom;
        const float dt = far - near;
        m00 = 2.0f/rl; m01 = 0.0f;    m02 = 0.0f;     m03 = 0.0f;
        m10 = 0.0f;    m11 = 2.0f/tb; m12 = 0.0f;     m13 = 0.0f;
        m20 = 0.0f;    m21 = 0.0f;    m22 = -2.0f/dt; m23 = 0.0f;
        m30 = -(right+left)/rl; m31 = -(top+bottom)/tb; m32 = -(far+near)/dt; m33 = 1.0f;
        return *this;
    }

    Matrix4& Matrix4::setPerspective(float fov, float width, float height, float zNear, float zFar)
    {
        const float rad2 = radf(fov) * 0.5f;
        const float h = cosf(rad2) / sinf(rad2);
        const float w = (h * height) / width;
        const float range = zFar - zNear;
        m00 = w; m01 = 0; m02 = 0; m03 = 0;
        m10 = 0; m11 = h; m12 = 0; m13 = 0;
        m20 = 0; m21 = 0; m22 = -(zFar + zNear) / range; m23 = -1;
        m30 = 0; m31 = 0; m32 = (-2.0f * zFar * zNear) / range; m33 = 1;
        return *this;
    }

    Matrix4& Matrix4::setLookAt(const Vector3& eye, const Vector3& center, const Vector3& up)
    {
        const Vector3 f = (center - eye).normalized();
        const Vector3 s = f.cross(up.normalized()).normalized();
        const Vector3 u = s.cross(f);
        m00 = s.x; m01 = u.x; m02 = -f.x; m03 = 0.0f;
        m10 = s.y; m11 = u.y; m12 = -f.y; m13 = 0.0f;
        m20 = s.z; m21 = u.z; m22 = -f.z; m23 = 0.0f;
        m30 = -s.dot(eye); m31 = -u.dot(eye); m32 = f.dot(eye); m33 = 1.0f;
        return *this;
    }

    Matrix4& Matrix4::fromPosition(const Vector3& position)
    {
        *this = IDENTITY4x4;
        return translate(position);
    }

    Matrix4& Matrix4::fromRotation(const Vector3& rotationDegrees)
    {
        loadIdentity();
        rotate(rotationDegrees.x, Vector3::XAXIS);
        rotate(rotationDegrees.y, Vector3::YAXIS);
        rotate(rotationDegrees.z, Vector3::ZAXIS);

        //Matrix4 rotX = Identity();
        //Matrix4 rotY = Identity();
        //Matrix4 rotZ = Identity();
        //rotX.rotate(rotationDegrees.x, Vector3::XAXIS);
        //rotY.rotate(rotationDegrees.y, Vector3::YAXIS);
        //rotZ.rotate(rotationDegrees.z, Vector3::ZAXIS);
        //multiply(rotX).multiply(rotY).multiply(rotZ);

        //Vector4 q = Vector4::fromRotation(rotationDegrees);
        //m00 = 1 - 2 * q.y * q.y - 2 * q.z * q.z;
        //m01 = 2 * q.x * q.y + 2 * q.w * q.z;
        //m02 = 2 * q.x * q.z - 2 * q.w * q.y;
        //m03 = 0.0f;
        //m10 = 2 * q.x * q.y - 2 * q.w * q.z;
        //m11 = 1 - 2 * q.x * q.x - 2 * q.z * q.z;
        //m12 = 2 * q.y * q.z + 2 * q.w * q.x;
        //m13 = 0.0f;
        //m20 = 2 * q.x * q.z + 2 * q.w * q.y;
        //m21 = 2 * q.y * q.z - 2 * q.w * q.x;
        //m22 = 1 - 2 * q.x * q.x - 2 * q.y * q.y;
        //m23 = 0.0f;
        //m30 = 0.0f;
        //m31 = 0.0f;
        //m32 = 0.0f;
        //m33 = 1.0f;
        return *this;
    }

    Matrix4& Matrix4::fromScale(const Vector3& sc)
    {
        m00 = sc.x; m01 = 0.0f; m02 = 0.0f; m03 = 0.0f;
        m10 = 0.0f; m11 = sc.y; m12 = 0.0f; m13 = 0.0f;
        m20 = 0.0f; m21 = 0.0f; m22 = sc.z; m23 = 0.0f;
        m30 = 0.0f; m31 = 0.0f; m32 = 0.0f; m33 = 1.0f;
        return *this;
    }

    Vector3 Matrix4::getPositionColumn() const
    {
        return { m30, m31, m32 };
    }

    Matrix4& Matrix4::setAffine2D(const Vector2& pos, float zOrder, float rotDegrees, const Vector2& scale)
    {
        fromPosition({ pos.x, pos.y, zOrder });
        this->scale({scale.x, scale.y, 1.0f});
        this->rotate(rotDegrees, Vector3::UP);
        return *this;
    }

    Matrix4& Matrix4::setAffine2D(const Vector2& pos, float zOrder, float rotDegrees, 
                                  const Vector2& rotAxis, const Vector2& scale)
    {
        fromPosition({ pos.x,pos.y,zOrder });
        this->scale({scale.x, scale.y, 1.0f});
        this->translate({ rotAxis.x, rotAxis.y, 0.0f }); // setup rotation axis
        this->rotate(rotDegrees, Vector3::UP);
        this->translate({ -rotAxis.x, -rotAxis.y, 0.0f }); // undo rotation axis for final transform
        return *this;
    }
    
    Matrix4& Matrix4::setAffine3D(const Vector3& pos, const Vector3& scale, const Vector3& rotationDegrees)
    {
        // affine 3d: move, scale to size and then rotate
        fromPosition(pos);
        this->scale(scale);
        Matrix4 rotation = {};
        rotation.fromRotation(rotationDegrees);
        this->multiply(rotation);
        return *this;
    }

    Matrix4& Matrix4::transpose()
    {
        swap(m01, m10);
        swap(m02, m20);
        swap(m03, m30);
        swap(m12, m21);
        swap(m13, m31);
        swap(m23, m32);
        return *this;
    }

    Matrix4 Matrix4::transposed() const
    {
        return {
            m00, m10, m20, m30,
            m01, m11, m21, m31,
            m02, m12, m22, m32,
            m03, m13, m23, m33,
        };
    }

#if !RPP_SSE_INTRINSICS
    // from MESA implementation of the GLU library gluInvertMatrix source
    static inline void invert4x4(const float* m, float* inv)
    {
        inv[0] = m[5]  * m[10] * m[15] - 
                 m[5]  * m[11] * m[14] - 
                 m[9]  * m[6]  * m[15] + 
                 m[9]  * m[7]  * m[14] +
                 m[13] * m[6]  * m[11] - 
                 m[13] * m[7]  * m[10];

        inv[4] = -m[4]  * m[10] * m[15] + 
                  m[4]  * m[11] * m[14] + 
                  m[8]  * m[6]  * m[15] - 
                  m[8]  * m[7]  * m[14] - 
                  m[12] * m[6]  * m[11] + 
                  m[12] * m[7]  * m[10];

        inv[8] = m[4]  * m[9] * m[15] - 
                 m[4]  * m[11] * m[13] - 
                 m[8]  * m[5] * m[15] + 
                 m[8]  * m[7] * m[13] + 
                 m[12] * m[5] * m[11] - 
                 m[12] * m[7] * m[9];

        inv[12] = -m[4]  * m[9] * m[14] + 
                   m[4]  * m[10] * m[13] +
                   m[8]  * m[5] * m[14] - 
                   m[8]  * m[6] * m[13] - 
                   m[12] * m[5] * m[10] + 
                   m[12] * m[6] * m[9];

        inv[1] = -m[1]  * m[10] * m[15] + 
                  m[1]  * m[11] * m[14] + 
                  m[9]  * m[2] * m[15] - 
                  m[9]  * m[3] * m[14] - 
                  m[13] * m[2] * m[11] + 
                  m[13] * m[3] * m[10];

        inv[5] = m[0]  * m[10] * m[15] - 
                 m[0]  * m[11] * m[14] - 
                 m[8]  * m[2] * m[15] + 
                 m[8]  * m[3] * m[14] + 
                 m[12] * m[2] * m[11] - 
                 m[12] * m[3] * m[10];

        inv[9] = -m[0]  * m[9] * m[15] + 
                  m[0]  * m[11] * m[13] + 
                  m[8]  * m[1] * m[15] - 
                  m[8]  * m[3] * m[13] - 
                  m[12] * m[1] * m[11] + 
                  m[12] * m[3] * m[9];

        inv[13] = m[0]  * m[9] * m[14] - 
                  m[0]  * m[10] * m[13] - 
                  m[8]  * m[1] * m[14] + 
                  m[8]  * m[2] * m[13] + 
                  m[12] * m[1] * m[10] - 
                  m[12] * m[2] * m[9];

        inv[2] = m[1]  * m[6] * m[15] - 
                 m[1]  * m[7] * m[14] - 
                 m[5]  * m[2] * m[15] + 
                 m[5]  * m[3] * m[14] + 
                 m[13] * m[2] * m[7] - 
                 m[13] * m[3] * m[6];

        inv[6] = -m[0]  * m[6] * m[15] + 
                  m[0]  * m[7] * m[14] + 
                  m[4]  * m[2] * m[15] - 
                  m[4]  * m[3] * m[14] - 
                  m[12] * m[2] * m[7] + 
                  m[12] * m[3] * m[6];

        inv[10] = m[0]  * m[5] * m[15] - 
                  m[0]  * m[7] * m[13] - 
                  m[4]  * m[1] * m[15] + 
                  m[4]  * m[3] * m[13] + 
                  m[12] * m[1] * m[7] - 
                  m[12] * m[3] * m[5];

        inv[14] = -m[0]  * m[5] * m[14] + 
                   m[0]  * m[6] * m[13] + 
                   m[4]  * m[1] * m[14] - 
                   m[4]  * m[2] * m[13] - 
                   m[12] * m[1] * m[6] + 
                   m[12] * m[2] * m[5];

        inv[3] = -m[1] * m[6] * m[11] + 
                  m[1] * m[7] * m[10] + 
                  m[5] * m[2] * m[11] - 
                  m[5] * m[3] * m[10] - 
                  m[9] * m[2] * m[7] + 
                  m[9] * m[3] * m[6];

        inv[7] = m[0] * m[6] * m[11] - 
                 m[0] * m[7] * m[10] - 
                 m[4] * m[2] * m[11] + 
                 m[4] * m[3] * m[10] + 
                 m[8] * m[2] * m[7] - 
                 m[8] * m[3] * m[6];

        inv[11] = -m[0] * m[5] * m[11] + 
                   m[0] * m[7] * m[9] + 
                   m[4] * m[1] * m[11] - 
                   m[4] * m[3] * m[9] - 
                   m[8] * m[1] * m[7] + 
                   m[8] * m[3] * m[5];

        inv[15] = m[0] * m[5] * m[10] - 
                  m[0] * m[6] * m[9] - 
                  m[4] * m[1] * m[10] + 
                  m[4] * m[2] * m[9] + 
                  m[8] * m[1] * m[6] - 
                  m[8] * m[2] * m[5];

        float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

        det = 1.0f / det;
        inv[0] *= det;
        inv[1] *= det;
        inv[2] *= det;
        inv[3] *= det;
        inv[4] *= det;
        inv[5] *= det;
        inv[6] *= det;
        inv[7] *= det;
        inv[8] *= det;
        inv[9] *= det;
        inv[10] *= det;
        inv[11] *= det;
        inv[12] *= det;
        inv[13] *= det;
        inv[14] *= det;
        inv[15] *= det;
    }
#else
    // The original code as provided by Intel in
    // "Streaming SIMD Extensions - Inverse of 4x4 Matrix"
    // (ftp://download.intel.com/design/pentiumiii/sml/24504301.pdf)
    static inline void invert4x4(const float* src, float* dst)
    {
        __m128 row1 = {}, row3 = {}, tmp1 = {};

        tmp1 = _mm_loadh_pi(_mm_loadl_pi(tmp1, (__m64*)(src)), (__m64*)(src + 4));
        row1 = _mm_loadh_pi(_mm_loadl_pi(row1, (__m64*)(src + 8)), (__m64*)(src + 12));
        __m128 row0 = _mm_shuffle_ps(tmp1, row1, 0x88);
        row1 = _mm_shuffle_ps(row1, tmp1, 0xDD);
        tmp1 = _mm_loadh_pi(_mm_loadl_pi(tmp1, (__m64*)(src + 2)), (__m64*)(src + 6));
        row3 = _mm_loadh_pi(_mm_loadl_pi(row3, (__m64*)(src + 10)), (__m64*)(src + 14));
        __m128 row2 = _mm_shuffle_ps(tmp1, row3, 0x88);
        row3 = _mm_shuffle_ps(row3, tmp1, 0xDD);

        tmp1 = _mm_mul_ps(row2, row3);
        tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
        __m128 minor0 = _mm_mul_ps(row1, tmp1);
        __m128 minor1 = _mm_mul_ps(row0, tmp1);

        tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
        minor0 = _mm_sub_ps(_mm_mul_ps(row1, tmp1), minor0);
        minor1 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor1);
        minor1 = _mm_shuffle_ps(minor1, minor1, 0x4E);

        tmp1 = _mm_mul_ps(row1, row2);
        tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
        minor0 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor0);
        __m128 minor3 = _mm_mul_ps(row0, tmp1);

        tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
        minor0 = _mm_sub_ps(minor0, _mm_mul_ps(row3, tmp1));
        minor3 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor3);
        minor3 = _mm_shuffle_ps(minor3, minor3, 0x4E);

        tmp1 = _mm_mul_ps(_mm_shuffle_ps(row1, row1, 0x4E), row3);
        tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
        row2 = _mm_shuffle_ps(row2, row2, 0x4E);
        minor0 = _mm_add_ps(_mm_mul_ps(row2, tmp1), minor0);
        __m128 minor2 = _mm_mul_ps(row0, tmp1);

        tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
        minor0 = _mm_sub_ps(minor0, _mm_mul_ps(row2, tmp1));
        minor2 = _mm_sub_ps(_mm_mul_ps(row0, tmp1), minor2);
        minor2 = _mm_shuffle_ps(minor2, minor2, 0x4E);

        tmp1 = _mm_mul_ps(row0, row1);
        tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
        minor2 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor2);
        minor3 = _mm_sub_ps(_mm_mul_ps(row2, tmp1), minor3);

        tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
        minor2 = _mm_sub_ps(_mm_mul_ps(row3, tmp1), minor2);
        minor3 = _mm_sub_ps(minor3, _mm_mul_ps(row2, tmp1));

        tmp1 = _mm_mul_ps(row0, row3);
        tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
        minor1 = _mm_sub_ps(minor1, _mm_mul_ps(row2, tmp1));
        minor2 = _mm_add_ps(_mm_mul_ps(row1, tmp1), minor2);

        tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
        minor1 = _mm_add_ps(_mm_mul_ps(row2, tmp1), minor1);
        minor2 = _mm_sub_ps(minor2, _mm_mul_ps(row1, tmp1));

        tmp1 = _mm_mul_ps(row0, row2);
        tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0xB1);
        minor1 = _mm_add_ps(_mm_mul_ps(row3, tmp1), minor1);
        minor3 = _mm_sub_ps(minor3, _mm_mul_ps(row1, tmp1));

        tmp1 = _mm_shuffle_ps(tmp1, tmp1, 0x4E);
        minor1 = _mm_sub_ps(minor1, _mm_mul_ps(row3, tmp1));
        minor3 = _mm_add_ps(_mm_mul_ps(row1, tmp1), minor3);

        __m128 det = _mm_mul_ps(row0, minor0);
        det = _mm_add_ps(_mm_shuffle_ps(det, det, 0x4E), det);
        det = _mm_add_ss(_mm_shuffle_ps(det, det, 0xB1), det);

        tmp1 = _mm_rcp_ss(det);
        det = _mm_sub_ss(_mm_add_ss(tmp1, tmp1), _mm_mul_ss(det, _mm_mul_ss(tmp1, tmp1)));
        det = _mm_shuffle_ps(det, det, 0x00);

        minor0 = _mm_mul_ps(det, minor0);
        _mm_storel_pi((__m64*)(dst), minor0);
        _mm_storeh_pi((__m64*)(dst + 2), minor0);
        minor1 = _mm_mul_ps(det, minor1);
        _mm_storel_pi((__m64*)(dst + 4), minor1);
        _mm_storeh_pi((__m64*)(dst + 6), minor1);
        minor2 = _mm_mul_ps(det, minor2);
        _mm_storel_pi((__m64*)(dst + 8), minor2);
        _mm_storeh_pi((__m64*)(dst + 10), minor2);
        minor3 = _mm_mul_ps(det, minor3);
        _mm_storel_pi((__m64*)(dst + 12), minor3);
        _mm_storeh_pi((__m64*)(dst + 14), minor3);
    }
#endif

    Matrix4 Matrix4::inverse() const
    {
        Matrix4 inverse;
        invert4x4(this->m, inverse.m);
        return inverse;
    }

    void Matrix4::print() const
    {
        char buffer[256];
        puts(toString(buffer));
    }
    
    const char* Matrix4::toString() const
    {
        static char buffer[256];
        return toString(buffer, sizeof(buffer));
    }
    
    char* Matrix4::toString(char* buffer) const
    {
        return toString(buffer, 256);
    }
    char* Matrix4::toString(char* buffer, int size) const
    {
        int len = snprintf(buffer, size_t(size), "{\n");
        for (auto v : r)
            len += snprintf(buffer+len, size_t(size-len), " %8.3f,%8.3f,%8.3f,%8.3f\n",
                                                            v.x , v.y , v.z , v.w);
        snprintf(buffer+len, size_t(size-len), "}");
        return buffer;
    }


    /////////////////////////////////////////////////////////////////////////////////////


    Vector2 PerspectiveViewport::ViewProjectToScreen(Vector3 worldPos, const Matrix4& viewProjection) const
    {
        Vector3 clipSpacePoint = viewProjection * worldPos;
        float len = worldPos.x * viewProjection.m03 
                  + worldPos.y * viewProjection.m13
                  + worldPos.z * viewProjection.m23
                  + viewProjection.m33;

        if (!almostEqual(len, 1.0f))
            clipSpacePoint /= len;
        return { ( clipSpacePoint.x + 1.0f) * 0.5f * width,
                 (-clipSpacePoint.y + 1.0f) * 0.5f * height };
    }

    Vector3 PerspectiveViewport::InverseViewProjectToWorld(Vector2 screenPos, float depth, const Matrix4& inverseViewProjection) const
    {
        Vector3 source = {
            screenPos.x / (width  * 2.0f) - 1.0f,
            screenPos.y / (height * 2.0f) - 1.0f,
            (depth - zNear) / (zFar - zNear)
        };

        Vector3 worldPos = inverseViewProjection * source;
        float len = source.x * inverseViewProjection.m03
                  + source.y * inverseViewProjection.m13
                  + source.z * inverseViewProjection.m23
                  + inverseViewProjection.m33;

        if (!almostEqual(len, 1.0f))
            worldPos /= len;
        return worldPos;
    }


    /////////////////////////////////////////////////////////////////////////////////////

    float BoundingBox::volume() const noexcept
    {
        return width() * height() * depth();
    }

    Vector3 BoundingBox::center() const noexcept
    {
        return lerp(0.5f, min, max);
    }

    float BoundingBox::radius() const noexcept
    {
        return (max - min).length() * 0.5f;
    }

    float BoundingBox::diagonal() const noexcept
    {
        return (max - min).length();
    }

    Vector3 BoundingBox::compare(const BoundingBox& bb) const noexcept
    {
        //// local delta based
        //float dx = width()  / bb.width();
        //float dy = height() / bb.height();
        //float dz = depth()  / bb.depth();
        //return 1.0f / Vector3{ sqrtf(dx), sqrtf(dy), sqrtf(dz) };

        // max distance from origin
        auto maxdist = [](float a, float b) {
            return fmax(fabs(a), fabs(b));
        };
        float dx = maxdist(min.x, max.x) / maxdist(bb.min.x, bb.max.x);
        float dy = maxdist(min.y, max.y) / maxdist(bb.min.y, bb.max.y);
        float dz = maxdist(min.z, max.z) / maxdist(bb.min.z, bb.max.z);
        return 1.0f / Vector3{ dx*dx*dx, dy*dy*dy, dz*dz*dz };
    }

    void BoundingBox::join(const Vector3& v) noexcept
    {
        if      (v.x < min.x) min.x = v.x; 
        else if (v.x > max.x) max.x = v.x;
        if      (v.y < min.y) min.y = v.y; 
        else if (v.y > max.y) max.y = v.y;
        if      (v.z < min.z) min.z = v.z; 
        else if (v.z > max.z) max.z = v.z;
    }

    void BoundingBox::join(const BoundingBox& bbox) noexcept
    {
        if      (bbox.min.x < min.x) min.x = bbox.min.x;
        else if (bbox.max.x > max.x) max.x = bbox.max.x;
        if      (bbox.min.y < min.y) min.y = bbox.min.y;
        else if (bbox.max.y > max.y) max.y = bbox.max.y;
        if      (bbox.min.z < min.z) min.z = bbox.min.z;
        else if (bbox.max.z > max.z) max.z = bbox.max.z;
    }

    bool BoundingBox::contains(const Vector3& v) const noexcept
    {
        return min.x <= v.x && v.x <= max.x
            && min.y <= v.y && v.y <= max.y
            && min.z <= v.z && v.z <= max.z;
    }

    float BoundingBox::distanceTo(const Vector3& v) const noexcept
    {
        Vector3 closest = clamp(v, min, max);
        return v.distanceTo(closest);
    }

    void BoundingBox::grow(float growth) noexcept
    {
        min.x -= growth;
        min.y -= growth;
        min.z -= growth;
        max.x += growth;
        max.y += growth;
        max.z += growth;
    }

    BoundingBox BoundingBox::create(float radius) noexcept
    {
        float r2 = abs(radius * 0.5f);
        return { Vector3{-r2}, Vector3{r2} };
    }

    BoundingBox BoundingBox::create(const std::vector<Vector3>& points) noexcept
    {
        if (points.empty())
            return { Vector3::ZERO, Vector3::ZERO };

        auto* verts = points.data(); // better debug iteration performance
        size_t size = points.size();

        Vector3 min = verts[0];
        Vector3 max = min;
        for (size_t i = 1; i < size; ++i)
        {
            const Vector3 pos = verts[i];
            if      (pos.x < min.x) min.x = pos.x;
            else if (pos.x > max.x) max.x = pos.x;
            if      (pos.y < min.y) min.y = pos.y;
            else if (pos.y > max.y) max.y = pos.y;
            if      (pos.z < min.z) min.z = pos.z;
            else if (pos.z > max.z) max.z = pos.z;
        }
        return { min, max };
    }

    BoundingBox BoundingBox::create(const std::vector<Vector3>& points, const std::vector<IdVector3>& ids) noexcept
    {
        if (points.empty() || ids.empty())
            return { Vector3::ZERO, Vector3::ZERO };

        auto* verts = points.data(); // better debug iteration performance
        auto*  data = ids.data();
        size_t size = ids.size();

        Vector3 min = verts[data[0].ID];
        Vector3 max = min;
        for (size_t i = 1; i < size; ++i)
        {
            const Vector3 pos = verts[data[i].ID];
            if      (pos.x < min.x) min.x = pos.x;
            else if (pos.x > max.x) max.x = pos.x;
            if      (pos.y < min.y) min.y = pos.y;
            else if (pos.y > max.y) max.y = pos.y;
            if      (pos.z < min.z) min.z = pos.z;
            else if (pos.z > max.z) max.z = pos.z;
        }
        return { min, max };
    }

    BoundingBox BoundingBox::create(const std::vector<Vector3>& points, const std::vector<int>& ids) noexcept
    {
        if (points.empty() || ids.empty())
            return { Vector3::ZERO, Vector3::ZERO };

        auto* verts = points.data(); // better debug iteration performance
        auto*  data = ids.data();
        size_t size = ids.size();

        Vector3 min = verts[data[0]];
        Vector3 max = min;
        for (size_t i = 1; i < size; ++i)
        {
            const Vector3 pos = verts[data[i]];
            if      (pos.x < min.x) min.x = pos.x;
            else if (pos.x > max.x) max.x = pos.x;
            if      (pos.y < min.y) min.y = pos.y;
            else if (pos.y > max.y) max.y = pos.y;
            if      (pos.z < min.z) min.z = pos.z;
            else if (pos.z > max.z) max.z = pos.z;
        }
        return { min, max };
    }

    BoundingBox BoundingBox::create(const Vector3* vertexData, int vertexCount, int stride) noexcept
    {
        if (vertexCount == 0)
            return { Vector3::ZERO, Vector3::ZERO };

        Vector3 min = vertexData[0];
        Vector3 max = min;
        for (int i = 1; i < vertexCount; ++i)
        {
            vertexData = (Vector3*)((byte*)vertexData + stride);

            const Vector3 pos = *vertexData;
            if      (pos.x < min.x) min.x = pos.x;
            else if (pos.x > max.x) max.x = pos.x;
            if      (pos.y < min.y) min.y = pos.y;
            else if (pos.y > max.y) max.y = pos.y;
            if      (pos.z < min.z) min.z = pos.z;
            else if (pos.z > max.z) max.z = pos.z;
        }
        return { min, max };
    }

    /////////////////////////////////////////////////////////////////////////////////////

    // Thank you!
    // https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
    float Ray::intersectSphere(Vector3 sphereCenter, float sphereRadius) const noexcept
    {
        Vector3 L = sphereCenter - origin;
        float tca = L.dot(direction);
        if (tca < 0) return 0.0f; // L and rayDir point in opposite directions, so intersect is behind rayStart

        float sqRadius = sphereRadius*sphereRadius;
        float d2 = L.dot(L) - tca*tca;
        if (d2 > sqRadius) return 0.0f;
        float thc = sqrt(sqRadius - d2);
        float t0 = tca - thc; // solutions for t if the ray intersects 
        float t1 = tca + thc;

        if (t0 > t1) swap(t0, t1);
        if (t0 < 0) {
            t0 = t1; // if t0 is negative, let's use t1 instead 
            if (t0 < 0) t0 = 0.0f; // both t0 and t1 are negative 
        }
        return t0;
    }

    // M�ller�Trumbore ray-triangle intersection algorithm
    // https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
    float Ray::intersectTriangle(Vector3 v0, Vector3 v1, Vector3 v2) const noexcept
    {
        Vector3 e1 = v1 - v0;
        Vector3 e2 = v2 - v0;
        // Calculate planes normal vector
        Vector3 pvec = direction.cross(e2);
        float det = e1.dot(pvec);

        // Ray is parallel to plane
        if (det < 1e-8 && det > -1e-8)
            return 0.0f;

        float inv_det = 1 / det;
        Vector3 tvec = origin - v0;
        float u = tvec.dot(pvec) * inv_det;
        if (u < 0 || u > 1)
            return 0;

        Vector3 qvec = tvec.cross(e1);
        float v = direction.dot(qvec) * inv_det;
        if (v < 0 || u + v > 1)
            return 0;

        return e2.dot(qvec) * inv_det;
    }

} // namespace rpp

