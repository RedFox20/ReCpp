#pragma once
/**
 * Basic Vector Math, Copyright (c) 2015 - Jorma Rebane
 */
#ifndef RPP_VECTORMATH_H
#define RPP_VECTORMATH_H
#include "strview.h"

#undef M_PI
#undef M_SQRT2

#pragma warning(push)
#pragma warning(disable:4201) // nameless struct/union warning

namespace rpp
{
    using namespace std;
    ///////////////////////////////////////////////////////////////////////////////

    constexpr double M_PI    = 3.14159265358979323846264338327950288;
    constexpr double M_SQRT2 = 1.41421356237309504880; // sqrt(2)

    /** @return Radians from degrees */
    static constexpr float radf(float degrees)
    {
        return float((degrees * M_PI) / 180.0); // rads=(degs*PI)/180
    }

    /** @brief Clamps a value between:  min <= value <= max */
    template<class T> static constexpr T clamp(T value, T min, T max) 
    {
        return value < min ? min : (value < max ? value : max);
    }

    /**
     * @brief Math utility, Linear Interpolation
     * @param start Starting bound of the linear range
     * @param end Ending bound of the linear range
     * @param position Multiplier value such as 0.5, 1.0 or 1.5
     */
    static constexpr float lerp(float start, float end, float position)
    {
        return start + (end-start)*position;
    }

    static constexpr bool nearlyZero(const float value, const float epsilon = 0.001f)
    {
        return -epsilon < value && value < epsilon;
    }

    ///////////////////////////////////////////////////////////////////////////////

    /** @brief Proxy to allow constexpr initialization inside Vector2 */
    struct float2
    {
        float x, y;
        constexpr float2(float x, float y) : x(x), y(y) {}
    };

    /** @brief 2D Vector for UI calculations */
    struct Vector2
    {
        float x, y;
        static constexpr float2 ZERO  = { 0.0f, 0.0f }; // 0 0
        static constexpr float2 ONE   = { 1.0f, 1.0f }; // 1 1
        static constexpr float2 RIGHT = { 1.0f, 0.0f }; // X-axis
        static constexpr float2 UP    = { 0.0f, 1.0f }; // Y-axis, OpenGL UP
    
        Vector2() {}
        explicit constexpr Vector2(float xy) : x(xy),  y(xy)  {}
        constexpr Vector2(float x, float y)  : x(x),   y(y)   {}
        constexpr Vector2(const float2& v)   : x(v.x), y(v.y) {}
    
        /** Print the Vector2 */
        void print() const;
    
        /** @return Temporary static string from this Vector */
        const char* toString() const;
    
        char* toString(char* buffer) const;
        char* toString(char* buffer, int size) const;
        template<int SIZE> char* toString(char(&buffer)[SIZE]) {
            return toString(buffer, SIZE);
        }

        /** @return TRUE if all elements are exactly 0.0f, which implies default initialized. 
         * To avoid FP errors, use almostZero() if you performed calculations */
        bool isZero()  const { return x == 0.0f && y == 0.0f; }
        bool notZero() const { return x != 0.0f || y != 0.0f; }

        /** @return TRUE if this vector is almost zero, with all components abs < 0.0001 */
        bool almostZero() const;

        /** @return TRUE if the vectors are almost equal, with a difference of < 0.0001 */
        bool almostEqual(const Vector2& b) const;

        /** @brief Set new XY values */
        void set(float x, float y);
    
        /** @return Length of the vector */
        float length() const;
    
        /** @return Squared length of the vector */
        float sqlength() const;
    
        /** @brief Normalize this vector */
        void normalize();

        /** @brief Normalize this vector to the given magnitude */
        void normalize(const float magnitude);
    
        /** @return A normalized copy of this vector */
        Vector2 normalized() const;
        Vector2 normalized(const float magnitude) const;

        /** @return Dot product of two vectors */
        float dot(const Vector2& v) const;

        /** @return Normalized direction of this vector */
        Vector2 direction() const { return normalized(); }

        /**
         * Treating this as point A, gives the RIGHT direction for vec AB
         */
        Vector2 right(const Vector2& b, float magnitude = 1.0f) const;

        /**
         * Treating this as point A, gives the LEFT direction for vec AB
         */
        Vector2 left(const Vector2& b, float magnitude = 1.0f) const;

        /**
         * Assuming this is already a direction vector, gives the perpendicular RIGHT direction vector
         */
        Vector2 right(float magnitude = 1.0f) const;

        /**
         * Assuming this is already a direction vector, gives the perpendicular LEFT direction vector
         */
        Vector2 left(float magnitude = 1.0f) const;
    
        Vector2& operator+=(const Vector2& b) { x+=b.x, y+=b.y; return *this; }
        Vector2& operator-=(const Vector2& b) { x-=b.x, y-=b.y; return *this; }
        Vector2& operator*=(const Vector2& b) { x*=b.x, y*=b.y; return *this; }
        Vector2& operator/=(const Vector2& b) { x/=b.x, y/=b.y; return *this; }
        Vector2  operator+ (const Vector2& b) const { return { x+b.x, y+b.y }; }
        Vector2  operator- (const Vector2& b) const { return { x-b.x, y-b.y }; }
        Vector2  operator* (const Vector2& b) const { return { x*b.x, y*b.y }; }
        Vector2  operator/ (const Vector2& b) const { return { x/b.x, y/b.y }; }
        Vector2  operator- () const { return {-x, -y}; }
    
        bool operator==(const Vector2& b) const { return x == b.x && y == b.y; }
        bool operator!=(const Vector2& b) const { return x != b.x || y != b.y; }

        Vector2& operator+=(float f) { x += f, y += f; return *this; }
        Vector2& operator-=(float f) { x -= f, y -= f; return *this; }
        Vector2& operator*=(float f) { x *= f, y *= f; return *this; }
        Vector2& operator/=(float f) { x /= f, y /= f; return *this; }
    };

    inline Vector2 operator+(const Vector2& a, float f) { return { a.x+f, a.y+f }; }
    inline Vector2 operator-(const Vector2& a, float f) { return { a.x-f, a.y-f }; }
    inline Vector2 operator*(const Vector2& a, float f) { return { a.x*f, a.y*f }; }
    inline Vector2 operator/(const Vector2& a, float f) { return { a.x/f, a.y/f }; }
    inline Vector2 operator+(float f, const Vector2& a) { return { f+a.x, f+a.y }; }
    inline Vector2 operator-(float f, const Vector2& a) { return { f-a.x, f-a.y }; }
    inline Vector2 operator*(float f, const Vector2& a) { return { f*a.x, f*a.y }; }
    inline Vector2 operator/(float f, const Vector2& a) { return { f/a.x, f/a.y }; }

    inline constexpr Vector2 clamp(const Vector2& value, const Vector2& min, const Vector2& max)
    {
        return { value.x < min.x ? min.x : (value.x < max.x ? value.x : max.x),
                 value.y < min.y ? min.y : (value.y < max.y ? value.y : max.y) };
    }

    inline constexpr Vector2 lerp(const Vector2& start, const Vector2& end, float position)
    {
        return { start.x + (end.x - start.x)*position,
                 start.y + (end.y - start.y)*position };
    }

    ////////////////////////////////////////////////////////////////////////////////

    /** @brief Proxy to allow constexpr initialization inside Point */
    struct int2
    {
        int x, y;
        constexpr int2(int x, int y) : x(x), y(y) {}
    };

    /**
     * @brief Utility for dealing with integer-only points. Pretty rare, but useful.
     */
    struct Point
    {
        int x, y;

        static const Point ZERO;

        Point() {}
        constexpr Point(int x, int y) : x(x), y(y) {}
        
        operator bool()  const { return  x || y;  }
        bool operator!() const { return !x && !y; }
        void set(int nx, int ny) { x = nx, y = ny; }

        bool isZero()  const { return !x && !y; }
        bool notZero() const { return  x || y;  }
    
        const char* toString() const;

        Point& operator+=(const Point& b) { x+=b.x, y+=b.y; return *this; }
        Point& operator-=(const Point& b) { x-=b.x, y-=b.y; return *this; }
        Point& operator*=(const Point& b) { x*=b.x, y*=b.y; return *this; }
        Point& operator/=(const Point& b) { x/=b.x, y/=b.y; return *this; }
        Point  operator+ (const Point& b) const { return { x+b.x, y+b.y }; }
        Point  operator- (const Point& b) const { return { x-b.x, y-b.y }; }
        Point  operator* (const Point& b) const { return { x*b.x, y*b.y }; }
        Point  operator/ (const Point& b) const { return { x/b.x, y/b.y }; }
        Point  operator- () const { return {-x, -y}; }

        bool operator==(const Point& b) const { return x == b.x && y == b.y; }
        bool operator!=(const Point& b) const { return x != b.x || y != b.y; }

        Point& operator+=(int i) { x += i, y += i; return *this; }
        Point& operator-=(int i) { x -= i, y -= i; return *this; }
        Point& operator*=(int i) { x *= i, y *= i; return *this; }
        Point& operator/=(int i) { x /= i, y /= i; return *this; }
    };

    inline Point operator+(const Point& a, int i) { return { a.x+i, a.y+i }; }
    inline Point operator-(const Point& a, int i) { return { a.x-i, a.y-i }; }
    inline Point operator*(const Point& a, int i) { return { a.x*i, a.y*i }; }
    inline Point operator/(const Point& a, int i) { return { a.x/i, a.y/i }; }
    inline Point operator+(int i, const Point& a) { return { i+a.x, i+a.y }; }
    inline Point operator-(int i, const Point& a) { return { i-a.x, i-a.y }; }
    inline Point operator*(int i, const Point& a) { return { i*a.x, i*a.y }; }
    inline Point operator/(int i, const Point& a) { return { i/a.x, i/a.y }; }

    ////////////////////////////////////////////////////////////////////////////////

    /** @brief Proxy to allow constexpr initialization inside Vector4 and Rect */
    struct float4
    {
        float x, y, z, w;
        constexpr float4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    };

    /** @brief Utility for dealing with 2D Rects */
    struct Rect
    {
        union {
            struct { float x, y, w, h; };
            struct { Vector2 pos; Vector2 size; };
            float4 xywh;
        };

        static constexpr float4 ZERO = { 0.0f, 0.0f, 0.0f, 0.0f };

        Rect() {}
        constexpr Rect(float x, float y, float w, float h)      : x(x), y(y), w(w), h(h) {}
        constexpr Rect(const Vector2& pos, const Vector2& size) : pos(pos),   size(size) {}
        constexpr Rect(const float4& xywh) : xywh(xywh) {}
        Rect(float x, float y, const Vector2& size)   : x(x), y(y), w(size.x), h(size.y) {}
        Rect(const Vector2& pos, float w, float h)    : x(pos.x), y(pos.y), w(w), h(h)   {}

        /** Print the Rect */
        void print() const;

        /** @return Temporary static string from this Rect in the form of "{pos %g;%g size %g;%g}" */
        const char* toString() const;

        char* toString(char* buffer) const;
        char* toString(char* buffer, int bufSize) const;
        template<int N> char* toString(char(&b)[N]) { return toString(b, N); }

        float area()   const { return w * h; }
        float left()   const { return x;     }
        float top()    const { return y;     }
        float right()  const { return x + w; }
        float bottom() const { return y + h; }
        const Vector2& topleft()  const { return pos; }
        Vector2        botright() const { return { x+w, y+h }; }

        /** @return TRUE if this Rect is equal to Rect::ZERO */
        bool isZero() const { return !x && !y && !w && !h; }
        /** @return TRUE if this Rect is NOT equal to Rect::ZERO */
        bool notZero() const { return w || h || x || y; }
    
        /** @return True if point is inside this Rect */
        bool hitTest(const Vector2& position) const;
        bool hitTest(const float xPos, const float yPos) const;
        /** @return TRUE if r is completely inside this Rect */
        bool hitTest(const Rect& r) const;

        /** @return TRUE if this Rect and r intersect */
        bool intersectsWith(const Rect& r) const;

        /** @brief Extrude the bounds of this rect by a positive or negative amount */
        void extrude(float extrude);
        void extrude(const Vector2& extrude);

        Rect extruded(float extrude) const { 
            Rect r = *this; 
            r.extrude(extrude); 
            return *this; 
        }

        Rect& operator+=(const Rect& b) { join(b); return *this; }

        // joins two rects, resulting in a rect that fits them both
        Rect joined(const Rect& b) const;

        // modifies this rect by joining rect b with this rect
        void join(const Rect& b);

        // clips this Rect with a potentially smaller frame
        // this Rect will then fit inside the frame Rect
        void clip(const Rect& frame);

        Rect operator+(const Rect& b) const { return joined(b); }
    
        bool operator==(const Rect& r) const { return x == r.x && y == r.y && w == r.w && h == r.h; }
        bool operator!=(const Rect& r) const { return x != r.x || y != r.y || w != r.w || h != r.h; }
    };

    inline Rect operator+(const Rect& a, float f) { return{ a.x+f, a.y+f, a.w, a.h }; }
    inline Rect operator-(const Rect& a, float f) { return{ a.x-f, a.y-f, a.w, a.h }; }
    inline Rect operator*(const Rect& a, float f) { return{ a.x, a.y, a.w*f, a.h*f }; }
    inline Rect operator/(const Rect& a, float f) { return{ a.x, a.y, a.w/f, a.h/f }; }
    inline Rect operator+(float f, const Rect& a) { return{ f+a.x, f+a.y, a.w, a.h }; }
    inline Rect operator-(float f, const Rect& a) { return{ f-a.x, f-a.y, a.w, a.h }; }
    inline Rect operator*(float f, const Rect& a) { return{ a.x, a.y, f*a.w, f*a.h }; }
    inline Rect operator/(float f, const Rect& a) { return{ a.x, a.y, f/a.w, f/a.h }; }

    ///////////////////////////////////////////////////////////////////////////////

    /** @brief Proxy to allow constexpr initialization inside Vector3 */
    struct float3
    { 
        float x, y, z;
        constexpr float3(float x, float y, float z) : x(x), y(y), z(z) {}
    };

    /** 
     * 3D Vector for matrix calculations
     * The coordinate system assumed in UP, FORWARD, RIGHT is OpenGL coordinate system:
     * +X is Right on the screen
     * +Y is Up on the screen
     * +Z is Forward INTO the screen
     */
    struct Vector3
    {
        union {
            struct { float x, y, z; };
            struct { Vector2 xy; };
            struct { float _x; Vector2 yz; };
            float3 xyz;
        };

        static constexpr float3 ZERO           = { 0.0f, 0.0f, 0.0f };      // 0 0 0
        static constexpr float3 ONE            = { 1.0f, 1.0f, 1.0f };      // 1 1 1

        static constexpr float3 LEFT           = { -1.0f, 0.0f, 0.0f };     // -X axis
        static constexpr float3 RIGHT          = { +1.0f, 0.0f, 0.0f };     // +X axis
        static constexpr float3 UP             = { 0.0f, +1.0f, 0.0f };     // +Y axis
        static constexpr float3 DOWN           = { 0.0f, -1.0f, 1.0f };     // -Y axis
        static constexpr float3 FORWARD        = { 0.0f, 0.0f, +1.0f };     // +Z axis
        static constexpr float3 BACKWARD       = { 0.0f, 0.0f, -1.0f };     // -Z axis
    
        static constexpr float3 RED            = { 1.0f, 0.0f, 0.0f };       // RGB 1 0 0
        static constexpr float3 GREEN          = { 0.0f, 1.0f, 0.0f };       // RGB 0 1 0
        static constexpr float3 BLUE           = { 0.0f, 0.0f, 1.0f };       // RGB 0 0 1

        static constexpr float3 YELLOW         = { 1.0f, 1.0f, 0.0f };       // 1 1 0
        static constexpr float3 ORANGE         = { 1.0f, 0.50196f, 0.0f };   // 1 0.502 0; 255 128 0
        static constexpr float3 MAGENTA        = { 1.0f, 0.0f, 1.0f };       // 1 0 1
        static constexpr float3 CYAN           = { 0.0f, 1.0f, 1.0f };       // 0 1 1
        static constexpr float3 SWEETGREEN     = { 0.337f, 0.737f, 0.223f }; // 86, 188, 57
        static constexpr float3 CORNFLOWERBLUE = { 0.33f, 0.66f, 1.0f };     // #55AAFF  85, 170, 255

        Vector3() {}
        constexpr Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
        constexpr Vector3(const float3& v) : xyz(v) {}
        Vector3(const Vector2& xy, float z) : xy(xy), z(z) {}
        Vector3(float x, const Vector2& yz) : x(x), yz(yz) {}

        /** @brief Set new XYZ values */
        void set(float x, float y, float z);
    
        /** @return Length of the vector */
        float length() const;
    
        /** @return Squared length of the vector */
        float sqlength() const;

        /** @return Absolute distance from this vec3 to target vec3 */
        float distanceTo(const Vector3& v) const;
    
        /** @brief Normalize this vector */
        void normalize();
        void normalize(const float magnitude);
    
        /** @return A normalized copy of this vector */
        Vector3 normalized() const;
        Vector3 normalized(const float magnitude) const;
    
        /** @return Cross product with another vector */
        Vector3 cross(const Vector3& b) const;
    
        /** @return Dot product with another vector */
        float dot(const Vector3& b) const;

        void print() const;
        const char* toString() const;
        char* toString(char* buffer) const;
        char* toString(char* buffer, int size) const;
        template<int SIZE> char* toString(char(&buffer)[SIZE]) const {
            return toString(buffer, SIZE);
        }

        /** @return TRUE if all elements are exactly 0.0f, which implies default initialized. 
         * To avoid FP errors, use almostZero() if you performed calculations */
        bool isZero()  const { return x == 0.0f && y == 0.0f && z == 0.0f; }
        bool notZero() const { return x != 0.0f || y != 0.0f || z != 0.0f; }

        /** @return TRUE if this vector is almost zero, with all components abs < 0.0001 */
        bool almostZero() const;

        /** @return TRUE if the vectors are almost equal, with a difference of < 0.0001 */
        bool almostEqual(const Vector3& b) const;

        Vector3& operator+=(const Vector3& b) { x+=b.x, y+=b.y, z+=b.z; return *this; }
        Vector3& operator-=(const Vector3& b) { x-=b.x, y-=b.y, z-=b.z; return *this; }
        Vector3& operator*=(const Vector3& b) { x*=b.x, y*=b.y, z*=b.z; return *this; }
        Vector3& operator/=(const Vector3& b) { x/=b.x, y/=b.y, z/=b.z; return *this; }
        Vector3  operator+ (const Vector3& b) const { return { x+b.x, y+b.y, z+b.z }; }
        Vector3  operator- (const Vector3& b) const { return { x-b.x, y-b.y, z-b.z }; }
        Vector3  operator* (const Vector3& b) const { return { x*b.x, y*b.y, z*b.z }; }
        Vector3  operator/ (const Vector3& b) const { return { x/b.x, y/b.y, z/b.z }; }
        Vector3  operator- () const { return {-x, -y, -z}; }
    
        bool operator==(const Vector3& b) const { return x == b.x && y == b.y && z == b.z; }
        bool operator!=(const Vector3& b) const { return x != b.x || y != b.y || z != b.z; }

        static const Vector3 smoothColor(const Vector3& src, const Vector3& dst, float ratio);

        /**
         * Some common 3D vector conversions
         * Some conversions are two way <->, but we still provide duplicate overloads for consistency
         *
         * This Vector library is based on OpenGL coordsys -- important when dealing with Matrix/Vector4(quat).
         * 
         * OpenGL coordsys: +X is Right on the screen, +Y is Up on the screen, +Z is Forward INTO the screen
         *
         */

        // OpenGL to OpenCV coordinate conversion. Works both ways: GL <-> CV
        // OpenCV coordsys: +X is Right on the screen, +Y is Down on the screen, +Z is INTO the screen
        Vector3 convertGL2CV() const noexcept { return { x, -y, z }; }
        Vector3 convertCV2GL() const noexcept { return { x, -y, z }; }

        // 3ds Max to OpenCV coordinate conversion
        // 3ds Max coordsys: +X is Right on the screen, +Y is INTO the screen, +Z is Up
        Vector3 convertMax2CV() const noexcept { return { x, -z, y }; }
        Vector3 convertCV2Max() const noexcept { return { x, z, -y }; }

        // OpenGL to 3ds Max coordinate conversion
        Vector3 convertMax2GL() const noexcept { return { x, z, y }; }
        Vector3 convertGL2Max() const noexcept { return { x, z, y }; }

        // OpenGL to iPhone coordinate conversion
        // iPhone coordsys: +X is Right on the screen, +Y is Up on the screen, +Z is OUT of the screen
        Vector3 convertGL2IOS() const noexcept { return { x, y, -z }; }
        Vector3 convertIOS2GL() const noexcept { return { x, y, -z }; }

        // Blender to OpenGL coordinate conversion
        // Blender coordsys: +X is OUT of the screen, +Y is Right on the screen, +Z is Up
        Vector3 convertBlender2GL() const noexcept { return { y, z, -x }; }
        Vector3 convertGL2Blender() const noexcept { return { -z, x, y }; }

        // DirectX to OpenGL coordinate conversion
        // DirectX default coordsys: +X is Right on the screen, +Y is Up, +Z is INTO the screen
        // -- D3D is identical to modern OpenGL coordsys --
        Vector3 convertDX2GL() const noexcept { return *this; }
        Vector3 convertGL2DX() const noexcept { return *this; }

        // Unreal Engine 4 to openGL coordinate conversion
        // UE4 coordsys: +X is INTO the screen, +Y is Right on the screen, +Z is Up
        Vector3 convertUE2GL() const noexcept { return { y, z, x }; }
        Vector3 convertGL2UE() const noexcept { return { z, x, y }; }

    };

    inline Vector3 operator+(const Vector3& a, float f) { return { a.x+f, a.y+f, a.z+f }; }
    inline Vector3 operator-(const Vector3& a, float f) { return { a.x-f, a.y-f, a.z-f }; }
    inline Vector3 operator*(const Vector3& a, float f) { return { a.x*f, a.y*f, a.z*f }; }
    inline Vector3 operator/(const Vector3& a, float f) { return { a.x/f, a.y/f, a.z/f }; }
    inline Vector3 operator+(float f, const Vector3& a) { return { f+a.x, f+a.y, f+a.z }; }
    inline Vector3 operator-(float f, const Vector3& a) { return { f-a.x, f-a.y, f-a.z }; }
    inline Vector3 operator*(float f, const Vector3& a) { return { f*a.x, f*a.y, f*a.z }; }
    inline Vector3 operator/(float f, const Vector3& a) { return { f/a.x, f/a.y, f/a.z }; }

    inline constexpr Vector3 clamp(const Vector3& value, const Vector3& min, const Vector3& max)
    {
        return { value.x < min.x ? min.x : (value.x < max.x ? value.x : max.x),
                 value.y < min.y ? min.y : (value.y < max.y ? value.y : max.y),
                 value.z < min.z ? min.z : (value.z < max.z ? value.z : max.z) };
    }

    inline constexpr Vector3 lerp(const Vector3& start, const Vector3& end, float position)
    {
        return { start.x + (end.x - start.x)*position,
                 start.y + (end.y - start.y)*position,
                 start.z + (end.z - start.z)*position };
    }

    ////////////////////////////////////////////////////////////////////////////////

    /** @brief 4D Vector for matrix calculations and quaternion rotations */
    struct Vector4
    {
        union {
            struct { float x, y, z, w; };
            struct { float r, g, b, a; };
            struct { Vector2 xy; Vector2 zw; };
            struct { Vector2 rg; Vector2 ba; };
            struct { Vector3 xyz; float _w; };
            struct { float3 rgb; float _a; };
            struct { float _x; Vector3 yzw; };
            struct { float _r; Vector3 gba; };
            float4 xyzw;
        };

        static constexpr float4 ZERO           = { 0.0f, 0.0f, 0.0f, 0.0f };   // XYZW 0 0 0 0
        static constexpr float4 WHITE          = { 1.0f, 1.0f, 1.0f, 1.0f };   // RGBA 1 1 1 1
        static constexpr float4 BLACK          = { 0.0f, 0.0f, 0.0f, 1.0f };   // RGBA 0 0 0 1
        static constexpr float4 RED            = { 1.0f, 0.0f, 0.0f, 1.0f };   // RGBA 1 0 0 1
        static constexpr float4 GREEN          = { 0.0f, 1.0f, 0.0f, 1.0f };   // RGBA 0 1 0 1
        static constexpr float4 BLUE           = { 0.0f, 0.0f, 1.0f, 1.0f };   // RGBA 0 0 1 1

        static constexpr float4 YELLOW         = { 1.0f, 1.0f, 0.0f, 1.0f };       // 1 1 0 1
        static constexpr float4 ORANGE         = { 1.0f, 0.50196f, 0.0f, 1.0f };   // 1 0.502 0 1; 255 128 0 255
        static constexpr float4 MAGENTA        = { 1.0f, 0.0f, 1.0f, 1.0f };       // 1 0 1 1
        static constexpr float4 CYAN           = { 0.0f, 1.0f, 1.0f, 1.0f };       // 0 1 1 1
        static constexpr float4 SWEETGREEN     = { 0.337f, 0.737f, 0.223f, 1.0f }; // 86, 188, 57
        static constexpr float4 CORNFLOWERBLUE = { 0.33f, 0.66f, 1.0f, 1.0f };     // #55AAFF  85, 170, 255
    
        Vector4() {}
        constexpr Vector4(float x, float y, float z,float w=1.f): x(x), y(y), z(z), w(w) {}
        constexpr Vector4(const float4& v) : xyzw(v) {}
        Vector4(const Vector2& xy, const Vector2& zw) : xy(xy), zw(zw) {}
        Vector4(const Vector2& xy, float z, float w)  : x(xy.x), y(xy.y), z(z), w(w) {}
        Vector4(float x, float y, const Vector2& zw)  : x(x), y(y), z(zw.x), w(zw.y) {}
        Vector4(const Vector3& xyz, float w)          : xyz(xyz), _w(w) {}
        Vector4(float x, const Vector3& yzw)          : _x(x), yzw(yzw) {}
        

        /** @return TRUE if all elements are exactly 0.0f, which implies default initialized.
        * To avoid FP errors, use almostZero() if you performed calculations */
        bool isZero()  const { return x == 0.0f && y == 0.0f && z == 0.0f; }
        bool notZero() const { return x != 0.0f || y != 0.0f || z != 0.0f; }
        
        /** @return TRUE if this vector is almost zero, with all components abs < 0.0001 */
        bool almostZero() const;

        /** @return TRUE if the vectors are almost equal, with a difference of < 0.0001 */
        bool almostEqual(const Vector4& b) const;

        void set(float x, float y, float z, float w);
    
        /** @return Dot product with another vector */
        float dot(const Vector4& b) const;

        /** @brief Creates a quaternion rotation from an Euler angle (degrees), rotation axis must be specified */
        static Vector4 fromAngleAxis(float angle, const Vector3& axis);
    
        /** @brief Creates a quaternion rotation from an Euler angle (degrees), rotation axis must be specified */
        static Vector4 fromAngleAxis(float angle, float x, float y, float z);
    
        /** @brief Creates a quaternion rotation from Euler XYZ (degrees) rotation */
        static Vector4 fromRotation(const Vector3& rotation);
        
        // don't use macros plz :)
        #undef RGB
        /** @return A 3-component float color from integer RGBA color */
        static constexpr Vector3 RGB(int r, int g, int b)
        {
            return Vector3{ r / 255.0f, g / 255.0f, b / 255.0f};
        }
        /** @return A 4-component float color from integer RGBA color */
        static constexpr Vector4 RGB(int r, int g, int b, int a)
        {
            return Vector4{ r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
        }
    
        /**
         * Parses a HEX color string, example: #rrggbb or #rrggbbaa
         * @warning The string must start with '#', otherwise WHITE is returned
         * @return Parsed color value
         */
        static const Vector4 HEX(const strview& s) noexcept;

        /**
         * Parses a color by name. Supported colors:
         * white, black, red, green, blue, yellow, orange
         */
        static const Vector4 NAME(const strview& s) noexcept;

        /**
         * Parses a color by number values:
         * -) RGBA integer values: 255 0 128 255
         * -) RGBA float values:   0.1 0.2 0.5 1
         * @note All HEX and NUMBER color fields are optional. 
         *       Color channel default is 0 and Alpha channel default is 255
         *       Example '#aa' would give 170 0 0 255  and '0.1' would give 25 0 0 255
         * @return Parsed color in normalized float value range [0.0 ... 1.0]
         */
        static const Vector4 NUMBER(strview s) noexcept;

        /**
         * Parses any type of color string. Supported:
         * -) RGBA HEX color strings: #rrggbb or #rrggbbaa
         * -) Named color strings: 'orange'
         * -) RGBA integer values: 255 0 128 255
         * -) RGBA float values:   0.1 0.2 0.5 1
         * @note All HEX and NUMBER color fields are optional. 
         *       Color channel default is 0 and Alpha channel default is 255
         *       Example '#aa' would give 170 0 0 255  and '0.1' would give 25 0 0 255
         * @return Default color WHITE or parsed color in normalized float value range [0.0 ... 1.0]
         */
        static const Vector4 parseColor(const strview& s) noexcept;

        /** 
         * @brief Rotates quaternion p with extra rotation q
         * q = additional rotation we wish to rotate with
         * p = original rotation
         */
        Vector4 rotate(const Vector4& q) const;
    
        Vector4& operator*=(const Vector4& q) { return (*this = rotate(q)); }
        Vector4  operator* (const Vector4& q) const { return rotate(q); }
    
        Vector4& operator+=(const Vector4& v) { x+=v.x, y+=v.y, z+=v.z, w+=v.w; return *this; }
        Vector4& operator-=(const Vector4& v) { x-=v.x, y-=v.y, z-=v.z, w-=v.w; return *this; }
        Vector4  operator+ (const Vector4& v) const { return { x+v.x, y+v.y, z+v.z, w+v.w }; }
        Vector4  operator- (const Vector4& v) const { return { x-v.x, y-v.y, z-v.z, w-v.w }; }
        Vector4  operator- () const { return {-x, -y, -z, -w }; }
    
        bool operator==(const Vector4& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
        bool operator!=(const Vector4& v) const { return x != v.x || y != v.y || z != v.z || w != v.w; }
    };

    inline Vector4 operator+(const Vector4& a, float f) { return { a.x+f, a.y+f, a.z+f, a.w+f }; }
    inline Vector4 operator-(const Vector4& a, float f) { return { a.x-f, a.y-f, a.z-f, a.w-f }; }
    inline Vector4 operator*(const Vector4& a, float f) { return { a.x*f, a.y*f, a.z*f, a.w*f }; }
    inline Vector4 operator/(const Vector4& a, float f) { return { a.x/f, a.y/f, a.z/f, a.w/f }; }
    inline Vector4 operator+(float f, const Vector4& a) { return { f+a.x, f+a.y, f+a.z, f+a.w }; }
    inline Vector4 operator-(float f, const Vector4& a) { return { f-a.x, f-a.y, f-a.z, f-a.w }; }
    inline Vector4 operator*(float f, const Vector4& a) { return { f*a.x, f*a.y, f*a.z, f*a.w }; }
    inline Vector4 operator/(float f, const Vector4& a) { return { f/a.x, f/a.y, f/a.z, f/a.w }; }

    inline constexpr Vector4 clamp(const Vector4& value, const Vector4& min, const Vector4& max)
    {
        return { value.x < min.x ? min.x : (value.x < max.x ? value.x : max.x),
                 value.y < min.y ? min.y : (value.y < max.y ? value.y : max.y),
                 value.z < min.z ? min.z : (value.z < max.z ? value.z : max.z),
                 value.w < min.w ? min.w : (value.w < max.w ? value.w : max.w) };
    }

    inline constexpr Vector4 lerp(const Vector4& start, const Vector4& end, float position)
    {
        return { start.x + (end.x - start.x)*position,
                 start.y + (end.y - start.y)*position,
                 start.z + (end.z - start.z)*position,
                 start.w + (end.w - start.w)*position };
    }

    ////////////////////////////////////////////////////////////////////////////////

    struct _Matrix4RowVis
    {
        float x, y, z, w;
    };

    struct Matrix4
    {
        union {
            struct {
                float m00, m01, m02, m03; // row0  0-3
                float m10, m11, m12, m13; // row1  0-3
                float m20, m21, m22, m23; // row2  0-3
                float m30, m31, m32, m33; // row3  0-3
            };
            struct {
                Vector4 r0, r1, r2, r3; // rows 0-3
            };
            struct {
                _Matrix4RowVis vis0, vis1, vis2, vis3;
            };
            float m[16]; // 4x4 float matrix

            Vector4 r[4]; // rows 0-3
        };
    
        /** @brief Global identity matrix for easy initialization */
        static const Matrix4 IDENTITY;
    
        /** @brief Loads identity matrix */
        Matrix4& loadIdentity();
    
        /** @brief Multiplies this matrix: this = this * mb */
        Matrix4& multiply(const Matrix4& mb);
    
        /** @brief Transforms 3D vector v with this matrix and return the resulting vec3 */
        Vector3 operator*(const Vector3& v) const;
    
        /** @brief Transforms 4D vector v with this matrix and returns the resulting vec4 */
        Vector4 operator*(const Vector4& v) const;
    
        /** @brief Translates object transformation matrix by given offset */
        Matrix4& translate(const Vector3& offset);
    
        /** @brief Rotates an object transformation matrix by given degree angle around a given axis */
        Matrix4& rotate(float angleDegs, const Vector3& axis);

        /** @brief Rotates an object transformation matrix by given degree angle around a given axis */
        Matrix4& rotate(float angleDegs, float x, float y, float z);
    
        /** @brief Scales an object transformation matrix by a given scale factor */
        Matrix4& scale(const Vector3& scale);
    
        /** @brief Loads an Ortographic projection matrix */
        Matrix4& setOrtho(float left, float right, float bottom, float top);
    
        /** @brief Loads a perspective projection matrix */
        Matrix4& setPerspective(float fov, float width, float height, float zNear, float zFar);
    
        /** @brief Loads a lookAt view/camera matrix */
        Matrix4& setLookAt(const Vector3& eye, const Vector3& center, const Vector3& up);
    
        /** @brief Creates a translated matrix from XYZ position */
        Matrix4& fromPosition(const Vector3& position);
    
        /** @brief Creates a rotated matrix from euler XYZ rotation */
        Matrix4& fromRotation(const Vector3& rotation);
    
        /** @brief Creates a scaled matrix from XYZ scale */
        Matrix4& fromScale(const Vector3& scale);

        /** @return Extracts position data from this affine matrix */
        Vector3 getPositionColumn() const;

        float getPosX() const { return m30; }
        float getPosY() const { return m31; }
        float getPosZ() const { return m32; }
        void setPosX(float x) { m30 = x; }
        void setPosY(float y) { m31 = y; }
        void setPosZ(float z) { m32 = z; }

        /** 
         * @brief Creates an affine matrix from 2D pos, zOrder, rotDegrees and 2D scale
         * @param pos 2D position on screen
         * @param zOrder Layer z-depth on screen; Higher values are on top.
         * @param rotDegrees Simple rotation in degrees. Rotates the object around its Z-Axis
         * @param scale Relative (0.0-1.0) 2D scale
         */
        Matrix4& setAffine2D(const Vector2& pos, float zOrder, float rotDegrees, const Vector2& scale);
    
        /** 
         * @brief Creates an affine matrix from 2D pos, zOrder, rotDegrees and 2D scale
         * @param pos 2D position on screen
         * @param zOrder Layer z-depth on screen; Higher values are on top.
         * @param rotDegrees Simple rotation in degrees. Rotates the object around its Z-Axis
         * @param rotAxis Point around which to rotate. By default this is {0,0}
         * @param scale Relative (0.0-1.0) 2D scale
         */
        Matrix4& setAffine2D(const Vector2& pos, float zOrder, float rotDegrees, 
                             const Vector2& rotAxis, const Vector2& scale);

        /** Print the matrix */
        void print() const;
    
        /** @return Temporary static string from this Matrix4 */
        const char* toString() const;
    
        char* toString(char* buffer) const;
        char* toString(char* buffer, int size) const;
        template<int SIZE> char* toString(char (&buffer)[SIZE]) const {
            return toString(buffer, SIZE);
        }
    };

    ////////////////////////////////////////////////////////////////////////////////

    /** @brief Simple 4-component RGBA float color */
    using Color = Vector4;

    /** @brief Simple 3-component RGB float color */
    using Color3 = Vector3;

    ////////////////////////////////////////////////////////////////////////////////

    // Vector3 with an associated Vertex ID
    struct IdVector3 : Vector3
    {
        int ID; // vertex id, -1 means invalid Vertex ID, [0] based indices

        IdVector3(int id, const Vector3& v) : Vector3{ v }, ID{ id } {}
        IdVector3(int id, float x, float y, float z) : Vector3{ x,y,z }, ID{ id } {}
    };

    ////////////////////////////////////////////////////////////////////////////////
    
    /** 3D bounding box */
    struct BoundingBox
    {
        Vector3 min;
        Vector3 max;
    
        BoundingBox() : min(0.0f,0.0f,0.0f), max(0.0f,0.0f,0.0f) {}
        BoundingBox(const Vector3& bbMinMax) : min(bbMinMax), max(bbMinMax) {}
        BoundingBox(const Vector3& bbMin, const Vector3& bbMax) : min(bbMin), max(bbMax) {}

        operator bool()  const { return min.notZero() && max.notZero(); }
        bool operator!() const { return min.isZero()  || max.isZero();  }

        bool isZero()  const { return min.isZero()  && max.isZero();  }
        bool notZero() const { return min.notZero() || max.notZero(); }

        float width()  const noexcept { return max.x - min.x; } // DX
        float height() const noexcept { return max.y - min.y; } // DY
        float depth()  const noexcept { return max.z - min.z; } // DZ

        // width*height*depth
        float volume() const noexcept;

        // get center of BoundingBox
        Vector3 center() const noexcept;

        Vector3 compare(const BoundingBox& bb) const noexcept;

        // joins a vector into this bounding box, possibly increasing the volume
        void join(const Vector3& v) noexcept;

        // joins with another bounding box, possibly increasing the volume
        void join(const BoundingBox& bbox) noexcept;

        // @return TRUE if vec3 point is inside this bounding box volume
        bool contains(const Vector3& v) const noexcept;

        // @return Distance to vec3 point from this boundingbox's corners
        // @todo Improve this to be more accurate
        float distanceTo(const Vector3& v) const noexcept;

        // grow the bounding box by the given value across all axes
        void grow(float growth) noexcept;

        // calculates the bounding box of the given point cloud
        static BoundingBox create(const vector<Vector3>& points) noexcept;

        // calculates the bounding box using ID-s from IdVector3-s, which index the given point cloud
        static BoundingBox create(const vector<Vector3>& points, const vector<IdVector3>& ids);

        // calculates the bounding box using vertex ID-s, which index the given point cloud
        static BoundingBox create(const vector<Vector3>& points, const vector<int>& ids);
    };

    ////////////////////////////////////////////////////////////////////////////////

} // namespace rpp

#pragma warning(pop)

#endif /* RPP_VECTORMATH_H */
