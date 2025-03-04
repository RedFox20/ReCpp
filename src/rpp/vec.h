#pragma once
/**
 * Basic Vector Math, Copyright (c) 2015 - Jorma Rebane
 * Distributed under MIT Software License
 */
#include "strview.h"
#include "math.h" // min, max, abs, radf, degf, clamp, lerp, lerpInverse
#include <vector> // for vector<Vector3> and vector<int>

#if _MSC_VER
#pragma warning(push)
#pragma warning(disable:4201) // nameless struct/union warning
#endif

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////

    /** @brief 2D Vector for UI calculations */
    struct RPPAPI Vector2
    {
        float x, y;

        static constexpr Vector2 Zero()  { return { 0.0f, 0.0f }; } // 0 0
        static constexpr Vector2 One()   { return { 1.0f, 1.0f }; } // 1 1
        static constexpr Vector2 Right() { return { 1.0f, 0.0f }; } // 1, 0 (X-axis)
        static constexpr Vector2 Up()    { return { 0.0f, 1.0f }; } // 0, 1 (Y-axis=OpenGL UP)
    
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
        bool hasNaN() const { return isnan(x) || isnan(y); }

        /** @return TRUE if this vector is almost zero, with all components abs < 0.0001 */
        bool almostZero() const;

        /** @return TRUE if the vectors are almost equal, with a difference of < 0.0001 */
        bool almostEqual(const Vector2& b) const;

        /** @brief Set new XY values */
        void set(float newX, float newY);
    
        /** @return Length of the vector */
        float length() const;
    
        /** @return Squared length of the vector */
        float sqlength() const;
    
        /** @brief Normalize this vector */
        void normalize();

        /** @brief Normalize this vector to the given magnitude */
        void normalize(float magnitude);
    
        /** @return A normalized copy of this vector */
        Vector2 normalized() const;
        Vector2 normalized(float magnitude) const;

        /** @return Dot product of two vectors */
        float dot(const Vector2& v) const;

        /** @return Normalized direction of this vector */
        Vector2 direction() const { return normalized(); }

        /**
         * Treating this as point A, gives the RIGHT direction for vec AB
         * @note THIS ASSUMES OPENGL COORDINATE SYSTEM
         */
        Vector2 right(const Vector2& b, float magnitude = 1.0f) const;

        /**
         * Treating this as point A, gives the LEFT direction for vec AB
         * @note THIS ASSUMES OPENGL COORDINATE SYSTEM
         */
        Vector2 left(const Vector2& b, float magnitude = 1.0f) const;

        /**
         * Assuming this is already a direction vector, gives the perpendicular RIGHT direction vector
         * @note THIS ASSUMES OPENGL COORDINATE SYSTEM
         */
        Vector2 right(float magnitude = 1.0f) const;

        /**
         * Assuming this is already a direction vector, gives the perpendicular LEFT direction vector
         * @note THIS ASSUMES OPENGL COORDINATE SYSTEM
         */
        Vector2 left(float magnitude = 1.0f) const;
    
        Vector2& operator+=(float f) { x+=f; y+=f; return *this; }
        Vector2& operator-=(float f) { x-=f; y-=f; return *this; }
        Vector2& operator*=(float f) { x*=f; y*=f; return *this; }
        Vector2& operator/=(float f) { x/=f; y/=f; return *this; }
        Vector2& operator+=(const Vector2& b) { x+=b.x; y+=b.y; return *this; }
        Vector2& operator-=(const Vector2& b) { x-=b.x; y-=b.y; return *this; }
        Vector2& operator*=(const Vector2& b) { x*=b.x; y*=b.y; return *this; }
        Vector2& operator/=(const Vector2& b) { x/=b.x; y/=b.y; return *this; }
        Vector2  operator+ (const Vector2& b) const { return { x+b.x, y+b.y }; }
        Vector2  operator- (const Vector2& b) const { return { x-b.x, y-b.y }; }
        Vector2  operator* (const Vector2& b) const { return { x*b.x, y*b.y }; }
        Vector2  operator/ (const Vector2& b) const { return { x/b.x, y/b.y }; }
        Vector2  operator- () const { return {-x, -y}; }
    
        bool operator==(const Vector2& b) const { return x == b.x && y == b.y; }
        bool operator!=(const Vector2& b) const { return x != b.x || y != b.y; }
    };

    constexpr Vector2 vec2(float xy) { return { xy, xy }; }

    inline Vector2 operator+(const Vector2& a, float f) { return { a.x+f, a.y+f }; }
    inline Vector2 operator-(const Vector2& a, float f) { return { a.x-f, a.y-f }; }
    inline Vector2 operator*(const Vector2& a, float f) { return { a.x*f, a.y*f }; }
    inline Vector2 operator/(const Vector2& a, float f) { return { a.x/f, a.y/f }; }
    inline Vector2 operator+(float f, const Vector2& a) { return { f+a.x, f+a.y }; }
    inline Vector2 operator-(float f, const Vector2& a) { return { f-a.x, f-a.y }; }
    inline Vector2 operator*(float f, const Vector2& a) { return { f*a.x, f*a.y }; }
    inline Vector2 operator/(float f, const Vector2& a) { return { f/a.x, f/a.y }; }

    constexpr Vector2 clamp(const Vector2& value, const Vector2& min, const Vector2& max)
    {
        return { value.x < min.x ? min.x : (value.x < max.x ? value.x : max.x),
                 value.y < min.y ? min.y : (value.y < max.y ? value.y : max.y) };
    }

    constexpr Vector2 lerp(float position, const Vector2& start, const Vector2& end)
    {
        return { start.x + (end.x - start.x)*position,
                 start.y + (end.y - start.y)*position };
    }
    
    ///////////////////////////////////////////////////////////////////////////////

    /** @brief 2D Vector for UI calculations */
    struct RPPAPI Vector2d
    {
        double x, y;

        static constexpr Vector2d Zero()  { return { 0.0, 0.0 }; } // 0 0
        static constexpr Vector2d One()   { return { 1.0, 1.0 }; } // 1 1
        static constexpr Vector2d Right() { return { 1.0, 0.0 }; } // 1, 0 (X-axis)
        static constexpr Vector2d Up()    { return { 0.0, 1.0 }; } // 0, 1 (Y-axis=OpenGL UP)

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
        bool isZero()  const { return x == 0.0 && y == 0.0; }
        bool notZero() const { return x != 0.0 || y != 0.0; }
        bool hasNaN() const { return isnan(x) || isnan(y); }

        /** @return TRUE if this vector is almost zero, with all components abs < 0.0001 */
        bool almostZero() const;

        /** @return TRUE if the vectors are almost equal, with a difference of < 0.0001 */
        bool almostEqual(const Vector2d& b) const;

        /** @brief Set new XY values */
        void set(double newX, double newY);
    
        /** @return Length of the vector */
        double length() const;
    
        /** @return Squared length of the vector */
        double sqlength() const;
    
        /** @brief Normalize this vector */
        void normalize();

        /** @brief Normalize this vector to the given magnitude */
        void normalize(double magnitude);
    
        /** @return A normalized copy of this vector */
        Vector2d normalized() const;
        Vector2d normalized(double magnitude) const;

        /** @return Dot product of two vectors */
        double dot(const Vector2d& v) const;

        /** @return Normalized direction of this vector */
        Vector2d direction() const { return normalized(); }

        /**
         * Treating this as point A, gives the RIGHT direction for vec AB
         * @note THIS ASSUMES OPENGL COORDINATE SYSTEM
         */
        Vector2d right(const Vector2d& b, double magnitude = 1.0) const;

        /**
         * Treating this as point A, gives the LEFT direction for vec AB
         * @note THIS ASSUMES OPENGL COORDINATE SYSTEM
         */
        Vector2d left(const Vector2d& b, double magnitude = 1.0) const;

        /**
         * Assuming this is already a direction vector, gives the perpendicular RIGHT direction vector
         * @note THIS ASSUMES OPENGL COORDINATE SYSTEM
         */
        Vector2d right(double magnitude = 1.0) const;

        /**
         * Assuming this is already a direction vector, gives the perpendicular LEFT direction vector
         * @note THIS ASSUMES OPENGL COORDINATE SYSTEM
         */
        Vector2d left(double magnitude = 1.0) const;
        
        Vector2d& operator+=(double f) { x+=f; y+=f; return *this; }
        Vector2d& operator-=(double f) { x-=f; y-=f; return *this; }
        Vector2d& operator*=(double f) { x*=f; y*=f; return *this; }
        Vector2d& operator/=(double f) { x/=f; y/=f; return *this; }
        Vector2d& operator+=(const Vector2d& b) { x+=b.x; y+=b.y; return *this; }
        Vector2d& operator-=(const Vector2d& b) { x-=b.x; y-=b.y; return *this; }
        Vector2d& operator*=(const Vector2d& b) { x*=b.x; y*=b.y; return *this; }
        Vector2d& operator/=(const Vector2d& b) { x/=b.x; y/=b.y; return *this; }
        Vector2d  operator+ (const Vector2d& b) const { return { x+b.x, y+b.y }; }
        Vector2d  operator- (const Vector2d& b) const { return { x-b.x, y-b.y }; }
        Vector2d  operator* (const Vector2d& b) const { return { x*b.x, y*b.y }; }
        Vector2d  operator/ (const Vector2d& b) const { return { x/b.x, y/b.y }; }
        Vector2d  operator- () const { return {-x, -y}; }
    
        bool operator==(const Vector2d& b) const { return x == b.x && y == b.y; }
        bool operator!=(const Vector2d& b) const { return x != b.x || y != b.y; }
    };

    constexpr Vector2d vec2d(double xy) { return { xy, xy }; }

    inline Vector2d operator+(const Vector2d& a, double f) { return { a.x+f, a.y+f }; }
    inline Vector2d operator-(const Vector2d& a, double f) { return { a.x-f, a.y-f }; }
    inline Vector2d operator*(const Vector2d& a, double f) { return { a.x*f, a.y*f }; }
    inline Vector2d operator/(const Vector2d& a, double f) { return { a.x/f, a.y/f }; }
    inline Vector2d operator+(double f, const Vector2d& a) { return { f+a.x, f+a.y }; }
    inline Vector2d operator-(double f, const Vector2d& a) { return { f-a.x, f-a.y }; }
    inline Vector2d operator*(double f, const Vector2d& a) { return { f*a.x, f*a.y }; }
    inline Vector2d operator/(double f, const Vector2d& a) { return { f/a.x, f/a.y }; }

    constexpr Vector2d clamp(const Vector2d& value, const Vector2d& min, const Vector2d& max)
    {
        return { value.x < min.x ? min.x : (value.x < max.x ? value.x : max.x),
                 value.y < min.y ? min.y : (value.y < max.y ? value.y : max.y) };
    }

    constexpr Vector2d lerp(double position, const Vector2d& start, const Vector2d& end)
    {
        return { start.x + (end.x - start.x)*position,
                 start.y + (end.y - start.y)*position };
    }
    
    ////////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Utility for dealing with integer-only points. Pretty rare, but useful.
     */
    struct RPPAPI Point
    {
        int x, y;

        static constexpr Point Zero() { return { 0, 0 }; } // 0 0

        explicit operator bool()  const { return  x || y;  }
        bool operator!() const { return !x && !y; }
        void set(int nx, int ny) { x = nx; y = ny; }

        bool isZero()  const { return !x && !y; }
        bool notZero() const { return  x || y;  }
    
        const char* toString() const;
        char* toString(char* buffer) const;
        char* toString(char* buffer, int size) const;
        template<int SIZE> char* toString(char(&buffer)[SIZE]) {
            return toString(buffer, SIZE);
        }

        Point& operator+=(int i) { x+=i; y+=i; return *this; }
        Point& operator-=(int i) { x-=i; y-=i; return *this; }
        Point& operator*=(int i) { x*=i; y*=i; return *this; }
        Point& operator/=(int i) { x/=i; y/=i; return *this; }
        Point& operator*=(float f) { x = int(x*f); y = int(y*f); return *this; }
        Point& operator/=(float f) { x = int(x/f); y = int(y/f); return *this; }
        Point& operator+=(const Point& b) { x+=b.x; y+=b.y; return *this; }
        Point& operator-=(const Point& b) { x-=b.x; y-=b.y; return *this; }
        Point& operator*=(const Point& b) { x*=b.x; y*=b.y; return *this; }
        Point& operator/=(const Point& b) { x/=b.x; y/=b.y; return *this; }
        Point  operator+ (const Point& b) const { return { x+b.x, y+b.y }; }
        Point  operator- (const Point& b) const { return { x-b.x, y-b.y }; }
        Point  operator* (const Point& b) const { return { x*b.x, y*b.y }; }
        Point  operator/ (const Point& b) const { return { x/b.x, y/b.y }; }
        Point  operator- () const { return {-x, -y}; }

        bool operator==(const Point& b) const { return x == b.x && y == b.y; }
        bool operator!=(const Point& b) const { return x != b.x || y != b.y; }
    };

    constexpr RPPAPI Point point2(int xy) { return { xy, xy }; }

    inline Point operator+(const Point& a, int i) { return { a.x+i, a.y+i }; }
    inline Point operator-(const Point& a, int i) { return { a.x-i, a.y-i }; }
    inline Point operator*(const Point& a, int i) { return { a.x*i, a.y*i }; }
    inline Point operator/(const Point& a, int i) { return { a.x/i, a.y/i }; }
    inline Point operator+(int i, const Point& a) { return { i+a.x, i+a.y }; }
    inline Point operator-(int i, const Point& a) { return { i-a.x, i-a.y }; }
    inline Point operator*(int i, const Point& a) { return { i*a.x, i*a.y }; }
    inline Point operator/(int i, const Point& a) { return { i/a.x, i/a.y }; }

    ////////////////////////////////////////////////////////////////////////////////


    /** @brief Utility for dealing with 2D Rects */
    struct RPPAPI RectF
    {
        union {
            struct { float x, y, w, h; };
            struct { Vector2 pos; Vector2 size; };
        };

        static constexpr RectF Zero() { return { 0, 0, 0, 0 }; } // 0 0 0 0

        constexpr RectF() : x{ 0 }, y{ 0 }, w{ 0 }, h{ 0 } {}
        constexpr RectF(float x, float y, float w, float h) : x{x}, y{y}, w{w}, h{h} {}
        constexpr RectF(Vector2 pos, Vector2 size) : pos{pos}, size{size} {}
        constexpr RectF(Vector2 pos, float w, float h) : pos{ pos }, size{ w, h } {}
        constexpr RectF(float x, float y, Vector2 size) : pos{ x, y }, size{ size } {}

        /** Print the RectF */
        void print() const;

        /** @return Temporary static string from this RectF in the form of "{pos %g;%g size %g;%g}" */
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

        Vector2 center() const { return { x + w/2, y + h/2 }; }
        float center_x() const { return x + w/2; }
        float center_y() const { return y + h/2; }

        /** @return TRUE if this RectF is equal to RectF::Zero */
        bool isZero() const { return !x && !y && !w && !h; }
        /** @return TRUE if this RectF is NOT equal to RectF::Zero */
        bool notZero() const { return w || h || x || y; }

        /** @return True if point is inside this RectF */
        bool hitTest(const Vector2& position) const;
        bool hitTest(float xPos, float yPos) const;
        /** @return TRUE if r is completely inside this RectF */
        bool hitTest(const RectF& r) const;

        /** @return TRUE if this RectF and r intersect */
        bool intersectsWith(const RectF& r) const;

        /** @brief Extrude the bounds of this RectF by a positive or negative amount */
        void extrude(float extrude);
        void extrude(const Vector2& extrude);

        RectF extruded(float extrude) const {
            RectF r = *this;
            r.extrude(extrude);
            return *this;
        }

        RectF& operator+=(const RectF& b) { join(b); return *this; }

        // joins two rects, resulting in a RectF that fits them both
        RectF joined(const RectF& b) const;

        // modifies this RectF by joining RectF b with this RectF
        void join(const RectF& b);

        // clips this RectF with a potentially smaller frame
        // this RectF will then fit inside the frame RectF
        void clip(const RectF& frame);

        RectF operator+(const RectF& b) const { return joined(b); }

        bool operator==(const RectF& r) const { return x == r.x && y == r.y && w == r.w && h == r.h; }
        bool operator!=(const RectF& r) const { return x != r.x || y != r.y || w != r.w || h != r.h; }
    };

    inline RectF operator+(const RectF& a, float f) { return{ a.x+f, a.y+f, a.w, a.h }; }
    inline RectF operator-(const RectF& a, float f) { return{ a.x-f, a.y-f, a.w, a.h }; }
    inline RectF operator*(const RectF& a, float f) { return{ a.x, a.y, a.w*f, a.h*f }; }
    inline RectF operator/(const RectF& a, float f) { return{ a.x, a.y, a.w/f, a.h/f }; }
    inline RectF operator+(float f, const RectF& a) { return{ f+a.x, f+a.y, a.w, a.h }; }
    inline RectF operator-(float f, const RectF& a) { return{ f-a.x, f-a.y, a.w, a.h }; }
    inline RectF operator*(float f, const RectF& a) { return{ a.x, a.y, f*a.w, f*a.h }; }
    inline RectF operator/(float f, const RectF& a) { return{ a.x, a.y, f/a.w, f/a.h }; }

    using Rect = RectF;

    ///////////////////////////////////////////////////////////////////////////////

    struct RPPAPI Recti
    {
        union {
            struct { int x, y, w, h; };
            struct { Point pos; Point size; };
        };

        static constexpr Recti Zero() { return { 0, 0, 0, 0 }; } // 0 0 0 0

        constexpr Recti() : x{ 0 }, y{ 0 }, w{ 0 }, h{ 0 } {}
        constexpr Recti(int x, int y, int w, int h) : x{x}, y{y}, w{w}, h{h} {}
        constexpr Recti(Point pos, Point size) : x{ pos.x }, y{ pos.y }, w{ size.x }, h{ size.y } {}
        constexpr Recti(Point pos, int w, int h) : x{ pos.x }, y{ pos.y }, w{ w }, h{ h } {}
        constexpr Recti(int x, int y, Point size) : x{ x }, y{ y }, w{ size.x }, h{ size.y } {}

        /** Print the Recti */
        void print() const;

        /** @return Temporary static string from this Recti in the form of "{pos %d;%d size %d;%d}" */
        const char* toString() const;

        char* toString(char* buffer) const;
        char* toString(char* buffer, int bufSize) const;
        template<int N> char* toString(char(&b)[N]) { return toString(b, N); }

        int area()   const { return w * h; }
        int left()   const { return x;     }
        int top()    const { return y;     }
        int right()  const { return x + w; }
        int bottom() const { return y + h; }
        const Point& topleft()  const { return pos; }
        Point        botright() const { return { x+w, y+h }; }

        Point center() const { return { x + w/2, y + h/2 }; }
        int center_x() const { return x + w/2; }
        int center_y() const { return y + h/2; }

        /** @return TRUE if this Recti is equal to Recti::Zero */
        bool isZero() const { return !x && !y && !w && !h; }
        /** @return TRUE if this Recti is NOT equal to Recti::Zero */
        bool notZero() const { return w || h || x || y; }

        /** @return TRUE if this Recti has a valid W and H */
        explicit operator bool() const { return  w > 0 && h > 0; }

        /** @return True if point is inside this Recti */
        bool hitTest(const Point& position) const;
        bool hitTest(int xPos, int yPos) const;
        /** @return TRUE if r is completely inside this Recti */
        bool hitTest(const Recti& r) const;

        /** @return TRUE if this Recti and r intersect */
        bool intersectsWith(const Recti& r) const;

        /** @brief Extrude the bounds of this rect by a positive or negative amount */
        void extrude(int extrude);
        void extrude(const Point& extrude);

        Recti extruded(int extrude) const {
            Recti r = *this;
            r.extrude(extrude);
            return *this;
        }

        Recti& operator+=(const Recti& b) { join(b); return *this; }

        // joins two rects, resulting in a Recti that fits them both
        Recti joined(const Recti& b) const;

        // modifies this Recti by joining Recti b with this Recti
        void join(const Recti& b);

        // clips this Recti with a potentially smaller frame
        // this Recti will then fit inside the frame Recti
        void clip(const Recti& frame);

        Recti operator+(const Recti& b) const { return joined(b); }

        bool operator==(const Recti& r) const { return x == r.x && y == r.y && w == r.w && h == r.h; }
        bool operator!=(const Recti& r) const { return x != r.x || y != r.y || w != r.w || h != r.h; }
    };

    inline Recti operator+(const Recti& a, int i) { return{ a.x+i, a.y+i, a.w, a.h }; }
    inline Recti operator-(const Recti& a, int i) { return{ a.x-i, a.y-i, a.w, a.h }; }
    inline Recti operator*(const Recti& a, int i) { return{ a.x, a.y, a.w*i, a.h*i }; }
    inline Recti operator/(const Recti& a, int i) { return{ a.x, a.y, a.w/i, a.h/i }; }
    inline Recti operator+(int i, const Recti& a) { return{ i+a.x, i+a.y, a.w, a.h }; }
    inline Recti operator-(int i, const Recti& a) { return{ i-a.x, i-a.y, a.w, a.h }; }
    inline Recti operator*(int i, const Recti& a) { return{ a.x, a.y, i*a.w, i*a.h }; }
    inline Recti operator/(int i, const Recti& a) { return{ a.x, a.y, i/a.w, i/a.h }; }

    ///////////////////////////////////////////////////////////////////////////////

    struct Vector3d;

    /** 
     * 3D Vector for matrix calculations
     * The coordinate system assumed in UP, FORWARD, RIGHT is OpenGL coordinate system:
     * +X is Right on the screen
     * +Y is Up on the screen
     * +Z is Forward INTO the screen
     */
    struct RPPAPI Vector3
    {
        union {
            struct { float x, y, z; };
            struct { float r, g, b; };
        };

        static constexpr Vector3 Zero() { return { 0.0f, 0.0f, 0.0f }; }      // 0 0 0
        static constexpr Vector3 One() { return { 1.0f, 1.0f, 1.0f }; }      // 1 1 1
        static constexpr Vector3 Left() { return { -1.0f,  0.0f,  0.0f }; }   // -X axis
        static constexpr Vector3 Right() { return { +1.0f,  0.0f,  0.0f }; }   // +X axis
        static constexpr Vector3 Up() { return {  0.0f, +1.0f,  0.0f }; }   // +Y axis
        static constexpr Vector3 Down() { return {  0.0f, -1.0f,  0.0f }; }   // -Y axis
        static constexpr Vector3 Forward() { return {  0.0f,  0.0f, +1.0f }; }   // +Z axis
        static constexpr Vector3 Backward() { return {  0.0f,  0.0f, -1.0f }; }   // -Z axis
        static constexpr Vector3 XAxis() { return { +1.0f,  0.0f,  0.0f }; }   // +X axis
        static constexpr Vector3 YAxis() { return {  0.0f, +1.0f,  0.0f }; }   // +Y axis
        static constexpr Vector3 ZAxis() { return {  0.0f,  0.0f, +1.0f }; }   // +Z axis
        static constexpr Vector3 White() { return { 1.0f, 1.0f, 1.0f }; }       // RGB 1 1 1
        static constexpr Vector3 Black() { return { 0.0f, 0.0f, 0.0f }; }       // RGB 0 0 0
        static constexpr Vector3 Red() { return { 1.0f, 0.0f, 0.0f }; }       // RGB 1 0 0
        static constexpr Vector3 Green() { return { 0.0f, 1.0f, 0.0f }; }       // RGB 0 1 0
        static constexpr Vector3 Blue() { return { 0.0f, 0.0f, 1.0f }; }       // RGB 0 0 1
        static constexpr Vector3 Yellow() { return { 1.0f, 1.0f, 0.0f }; }       // 1 1 0
        static constexpr Vector3 Orange() { return { 1.0f, 0.50196f, 0.0f }; }   // 1 0.502 0; 255 128 0
        static constexpr Vector3 Magenta() { return { 1.0f, 0.0f, 1.0f }; }       // 1 0 1
        static constexpr Vector3 Cyan() { return  { 0.0f, 1.0f, 1.0f }; }       // 0 1 1
        static constexpr Vector3 SweetGreen() { return { 0.337f, 0.737f, 0.223f }; } // 86, 188, 57
        static constexpr Vector3 CornflowerBlue() { return { 0.33f, 0.66f, 1.0f }; }     // #55AAFF  85, 170, 255

    #if __clang__ || _MSC_VER
        Vector3() = default;
        constexpr Vector3(float x, float y, float z) : x{x}, y{y}, z{z} {}
    #endif

        explicit operator Vector3d() const;

        /** @brief Set new XYZ values */
        void set(float newX, float newY, float newZ);
    
        /** @return Length of the vector */
        float length() const;
    
        /** @return Squared length of the vector */
        float sqlength() const;

        /** @return Absolute distance from this vec3 to target vec3 */
        float distanceTo(Vector3 v) const;

        /** @return Squared distance from this vec3 to target vec3 */
        float sqDistanceTo(Vector3 v) const;
    
        /** @brief Normalize this vector */
        void normalize();
        void normalize(float magnitude);
    
        /** @return A normalized copy of this vector */
        Vector3 normalized() const;
        Vector3 normalized(float magnitude) const;
    
        /** @return Cross product with another vector */
        Vector3 cross(Vector3 v) const;
    
        /** @return Dot product with another vector */
        float dot(Vector3 v) const;

        /**
         * Creates a mask vector for each component
         * x = almostZero(x) ? 1.0f : 0.0f;
         */
        Vector3 mask() const;

        /**
         * @return Assuming this is a direction vector, gives XYZ Euler rotation in RADIANS
         * X: Roll
         * Y: Pitch
         * Z: Yaw
         */
        Vector3 toEulerAngles() const;

        /**
         * Transforms this Vector3 with a transformation functor
         * @param componentTransform   `float transform(float v)`
         */
        template<class Transform> void transform(const Transform& componentTransform)
        {
            x = componentTransform(x);
            y = componentTransform(y);
            z = componentTransform(z);
        }

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
        bool hasNaN() const { return isnan(x) || isnan(y) || isnan(z); }

        /** @return TRUE if this vector is almost zero, with all components abs < 0.0001 */
        bool almostZero() const;

        /** @return TRUE if the vectors are almost equal, with a difference of < 0.0001 */
        bool almostEqual(Vector3 v) const;

        Vector3& operator+=(float f) { x+=f; y+=f; z+=f; return *this; }
        Vector3& operator-=(float f) { x-=f; y-=f; z-=f; return *this; }
        Vector3& operator*=(float f) { x*=f; y*=f; z*=f; return *this; }
        Vector3& operator/=(float f) { x/=f; y/=f; z/=f; return *this; }
        Vector3& operator+=(const Vector3& v) { x+=v.x; y+=v.y; z+=v.z; return *this; }
        Vector3& operator-=(const Vector3& v) { x-=v.x; y-=v.y; z-=v.z; return *this; }
        Vector3& operator*=(const Vector3& v) { x*=v.x; y*=v.y; z*=v.z; return *this; }
        Vector3& operator/=(const Vector3& v) { x/=v.x; y/=v.y; z/=v.z; return *this; }
        Vector3  operator+ (const Vector3& v) const { return { x+v.x, y+v.y, z+v.z }; }
        Vector3  operator- (const Vector3& v) const { return { x-v.x, y-v.y, z-v.z }; }
        Vector3  operator* (const Vector3& v) const { return { x*v.x, y*v.y, z*v.z }; }
        Vector3  operator/ (const Vector3& v) const { return { x/v.x, y/v.y, z/v.z }; }
        Vector3  operator- () const { return {-x, -y, -z}; }
    
        bool operator==(const Vector3& v) const { return x == v.x && y == v.y && z == v.z; }
        bool operator!=(const Vector3& v) const { return x != v.x || y != v.y || z != v.z; }

        static Vector3 smoothColor(Vector3 src, Vector3 dst, float ratio);

        // don't use macros plz :)
        #undef RGB
        /** @return A 3-component float color from integer RGBA color */
        static constexpr Vector3 RGB(int r, int g, int b)
        {
            return { r / 255.0f, g / 255.0f, b / 255.0f };
        }

        /**
         * Parses any type of color string. Supported:
         * -) RGB HEX color strings: #rrggbb
         * -) Named color strings: 'orange'
         * -) RGB integer values: 255 0 128
         * -) RGB float values:   0.1 0.2 0.5
         * @note All HEX and NUMBER color fields are optional. 
         *       Color channel default is 0
         *       Example '#aa' would give 170 0 0  and '0.1' would give 25 0 0
         * @return Default color White or parsed color in normalized float value range [0.0 ... 1.0]
         */
        static Vector3 parseColor(const strview& s) noexcept;

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
        // OpenGL coordsys:  +X is Right on the screen, +Y is Up on the screen,   +Z is Forward INTO the screen
        // OpenCV coordsys:  +X is Right on the screen, +Y is Down on the screen, +Z is INTO the screen
        Vector3 convertGL2CV() const noexcept { return { x, -y, z }; }
        Vector3 convertCV2GL() const noexcept { return { x, -y, z }; }

        // 3ds Max to OpenCV coordinate conversion
        // 3ds Max coordsys: +X is Right on the screen, +Y is INTO the screen,    +Z is Up
        // OpenCV  coordsys: +X is Right on the screen, +Y is Down on the screen, +Z is INTO the screen
        Vector3 convertMax2CV() const noexcept { return { x, -z, y }; }
        Vector3 convertCV2Max() const noexcept { return { x, z, -y }; }

        // OpenGL to 3ds Max coordinate conversion
        // 3ds Max coordsys: +X is Right on the screen, +Y is INTO the screen,  +Z is Up
        // OpenGL coordsys:  +X is Right on the screen, +Y is Up on the screen, +Z is Forward INTO the screen
        Vector3 convertMax2GL() const noexcept { return { x, z, y }; }
        Vector3 convertGL2Max() const noexcept { return { x, z, y }; }

        // OpenGL to iPhone coordinate conversion
        // OpenGL coordsys:  +X is Right on the screen, +Y is Up on the screen, +Z is Forward INTO the screen
        // iPhone coordsys:  +X is Right on the screen, +Y is Up on the screen, +Z is OUT of the screen
        Vector3 convertGL2IOS() const noexcept { return { x, y, -z }; }
        Vector3 convertIOS2GL() const noexcept { return { x, y, -z }; }

        // Blender to OpenGL coordinate conversion
        // Blender coordsys: +X is Right on the screen, +Y is INTO the screen,  +Z is Up on the screen
        // OpenGL  coordsys: +X is Right on the screen, +Y is Up on the screen, +Z is Forward INTO the screen
        Vector3 convertBlender2GL() const noexcept  { return { x, z, y }; }
        Vector3 convertGL2Blender() const noexcept  { return { x, z, y }; }
        
        // Blender to iPhone coordinate conversion
        // Blender coordsys: +X is Right on the screen, +Y is INTO the screen,  +Z is Up on the screen
        // iPhone  coordsys: +X is Right on the screen, +Y is Up on the screen, +Z is OUT of the screen
        Vector3 convertBlender2IOS() const noexcept { return { x,  z, -y }; }
        Vector3 convertIOS2Blender() const noexcept { return { x, -z,  y }; }

        
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

    constexpr Vector3 vec3(Vector2 xy, float z) { return { xy.x, xy.y, z }; }
    constexpr Vector3 vec3(float x, Vector2 yz) { return { x, yz.x, yz.y }; }
    constexpr Vector3 vec3(float xyz) { return { xyz, xyz, xyz }; }

    inline Vector3 operator+(const Vector3& a, float f) { return { a.x+f, a.y+f, a.z+f }; }
    inline Vector3 operator-(const Vector3& a, float f) { return { a.x-f, a.y-f, a.z-f }; }
    inline Vector3 operator*(const Vector3& a, float f) { return { a.x*f, a.y*f, a.z*f }; }
    inline Vector3 operator/(const Vector3& a, float f) { return { a.x/f, a.y/f, a.z/f }; }
    inline Vector3 operator+(float f, const Vector3& a) { return { f+a.x, f+a.y, f+a.z }; }
    inline Vector3 operator-(float f, const Vector3& a) { return { f-a.x, f-a.y, f-a.z }; }
    inline Vector3 operator*(float f, const Vector3& a) { return { f*a.x, f*a.y, f*a.z }; }
    inline Vector3 operator/(float f, const Vector3& a) { return { f/a.x, f/a.y, f/a.z }; }

    constexpr Vector3 clamp(const Vector3& value, const Vector3& min, const Vector3& max)
    {
        return { value.x < min.x ? min.x : (value.x < max.x ? value.x : max.x),
                 value.y < min.y ? min.y : (value.y < max.y ? value.y : max.y),
                 value.z < min.z ? min.z : (value.z < max.z ? value.z : max.z) };
    }

    constexpr Vector3 lerp(float position, const Vector3& start, const Vector3& end)
    {
        return { start.x + (end.x - start.x)*position,
                 start.y + (end.y - start.y)*position,
                 start.z + (end.z - start.z)*position };
    }
    
    ////////////////////////////////////////////////////////////////////////////////


    struct RPPAPI Vector3d
    {
        double x, y, z;

        static constexpr Vector3d Zero() { return { 0.0f, 0.0f, 0.0f }; }
        explicit operator Vector3() const { return {float(x), float(y), float(z)}; }
        
        /** @brief Set new XYZ values */
        void set(double newX, double newY, double newZ);
    
        /** @return Length of the vector */
        double length() const;
    
        /** @return Squared length of the vector */
        double sqlength() const;

        /** @return Absolute distance from this vec3 to target vec3 */
        double distanceTo(const Vector3d& v) const;
    
        /** @brief Normalize this vector */
        void normalize();
        void normalize(const double magnitude);
    
        /** @return A normalized copy of this vector */
        Vector3d normalized() const;
        Vector3d normalized(const double magnitude) const;
    
        /** @return Cross product with another vector */
        Vector3d cross(const Vector3d& b) const;
    
        /** @return Dot product with another vector */
        double dot(const Vector3d& b) const;
        
        void print() const;
        const char* toString() const;
        char* toString(char* buffer) const;
        char* toString(char* buffer, int size) const;
        template<int SIZE> char* toString(char(&buffer)[SIZE]) const {
            return toString(buffer, SIZE);
        }

        /** @return TRUE if all elements are exactly 0.0f, which implies default initialized. 
         * To avoid FP errors, use almostZero() if you performed calculations */
        bool isZero()  const { return x == 0.0 && y == 0.0 && z == 0.0; }
        bool notZero() const { return x != 0.0 || y != 0.0 || z != 0.0; }
        bool hasNaN() const { return isnan(x) || isnan(y) || isnan(z); }

        /** @return TRUE if this vector is almost zero, with all components abs < 0.0001 */
        bool almostZero() const;

        /** @return TRUE if the vectors are almost equal, with a difference of < 0.0001 */
        bool almostEqual(const Vector3d& v) const;

        Vector3d& operator+=(double f) { x+=f; y+=f; z+=f; return *this; }
        Vector3d& operator-=(double f) { x-=f; y-=f; z-=f; return *this; }
        Vector3d& operator*=(double f) { x*=f; y*=f; z*=f; return *this; }
        Vector3d& operator/=(double f) { x/=f; y/=f; z/=f; return *this; }
        Vector3d& operator+=(const Vector3d& b) { x+=b.x; y+=b.y; z+=b.z; return *this; }
        Vector3d& operator-=(const Vector3d& b) { x-=b.x; y-=b.y; z-=b.z; return *this; }
        Vector3d& operator*=(const Vector3d& b) { x*=b.x; y*=b.y; z*=b.z; return *this; }
        Vector3d& operator/=(const Vector3d& b) { x/=b.x; y/=b.y; z/=b.z; return *this; }
        Vector3d  operator+ (const Vector3d& b) const { return { x+b.x, y+b.y, z+b.z }; }
        Vector3d  operator- (const Vector3d& b) const { return { x-b.x, y-b.y, z-b.z }; }
        Vector3d  operator* (const Vector3d& b) const { return { x*b.x, y*b.y, z*b.z }; }
        Vector3d  operator/ (const Vector3d& b) const { return { x/b.x, y/b.y, z/b.z }; }
        Vector3d  operator- () const { return {-x, -y, -z}; }
        
        bool operator==(const Vector3d& v) const { return x == v.x && y == v.y && z == v.z; }
        bool operator!=(const Vector3d& v) const { return x != v.x || y != v.y || z != v.z; }
        
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
        Vector3d convertGL2CV() const noexcept { return { x, -y, z }; }
        Vector3d convertCV2GL() const noexcept { return { x, -y, z }; }

    };

    constexpr Vector3d vec3d(double xyz) { return { xyz, xyz, xyz }; }

    inline Vector3d operator+(const Vector3d& a, double f) { return { a.x+f, a.y+f, a.z+f }; }
    inline Vector3d operator-(const Vector3d& a, double f) { return { a.x-f, a.y-f, a.z-f }; }
    inline Vector3d operator*(const Vector3d& a, double f) { return { a.x*f, a.y*f, a.z*f }; }
    inline Vector3d operator/(const Vector3d& a, double f) { return { a.x/f, a.y/f, a.z/f }; }
    inline Vector3d operator+(double f, const Vector3d& a) { return { f+a.x, f+a.y, f+a.z }; }
    inline Vector3d operator-(double f, const Vector3d& a) { return { f-a.x, f-a.y, f-a.z }; }
    inline Vector3d operator*(double f, const Vector3d& a) { return { f*a.x, f*a.y, f*a.z }; }
    inline Vector3d operator/(double f, const Vector3d& a) { return { f/a.x, f/a.y, f/a.z }; }

    constexpr Vector3d clamp(const Vector3d& value, const Vector3d& min, const Vector3d& max)
    {
        return { value.x < min.x ? min.x : (value.x < max.x ? value.x : max.x),
                 value.y < min.y ? min.y : (value.y < max.y ? value.y : max.y),
                 value.z < min.z ? min.z : (value.z < max.z ? value.z : max.z) };
    }

    constexpr Vector3d lerp(double position, const Vector3d& start, const Vector3d& end)
    {
        return { start.x + (end.x - start.x)*position,
                 start.y + (end.y - start.y)*position,
                 start.z + (end.z - start.z)*position };
    }
    
    inline Vector3::operator Vector3d() const { return { double(x), double(y), double(z) }; }


    ////////////////////////////////////////////////////////////////////////////////


    /**
     * @note This is meant for use along with Matrix4
     *       It has no intrinsic value as stand-alone
     */
    struct RPPAPI AngleAxis
    {
        Vector3 axis = Vector3::Zero(); // rotation axis
        float angle = 0.0f;  // rotation angle in DEGREES

        /**
         * @return The rotation axis and angle between two vectors
         */
        static AngleAxis fromVectors(Vector3 a, Vector3 b);
    };


    ////////////////////////////////////////////////////////////////////////////////

    struct RPPAPI Matrix3;
    struct RPPAPI Matrix4;

    /** @brief 4D Vector for matrix calculations and quaternion rotations */
    struct RPPAPI Vector4
    {
        union {
            struct { float x, y, z, w; };
            struct { float r, g, b, a; };
            struct { Vector2 xy; Vector2 zw; };
            struct { Vector3 xyz; };
            struct { Vector3 rgb; };
        };

        static constexpr Vector4 Zero() { return { 0.0f, 0.0f, 0.0f, 0.0f }; }   // XYZW 0 0 0 0
        static constexpr Vector4 One() { return { 1.0f, 1.0f, 1.0f, 1.0f }; }   // XYZW 1 1 1 1
        static constexpr Vector4 White() { return { 1.0f, 1.0f, 1.0f, 1.0f }; }   // RGBA 1 1 1 1
        static constexpr Vector4 Black() { return { 0.0f, 0.0f, 0.0f, 1.0f }; }   // RGBA 0 0 0 1
        static constexpr Vector4 Red() { return { 1.0f, 0.0f, 0.0f, 1.0f }; }   // RGBA 1 0 0 1
        static constexpr Vector4 Green() { return { 0.0f, 1.0f, 0.0f, 1.0f }; }   // RGBA 0 1 0 1
        static constexpr Vector4 Blue() { return { 0.0f, 0.0f, 1.0f, 1.0f }; }   // RGBA 0 0 1 1
        static constexpr Vector4 Yellow() { return { 1.0f, 1.0f, 0.0f, 1.0f }; }       // 1 1 0 1
        static constexpr Vector4 Orange() { return { 1.0f, 0.50196f, 0.0f, 1.0f }; }   // 1 0.502 0 1; 255 128 0 255
        static constexpr Vector4 Magenta() { return { 1.0f, 0.0f, 1.0f, 1.0f }; }       // 1 0 1 1
        static constexpr Vector4 Cyan() { return { 0.0f, 1.0f, 1.0f, 1.0f }; }       // 0 1 1 1
        static constexpr Vector4 SweetGreen() { return { 0.337f, 0.737f, 0.223f, 1.0f }; } // 86, 188, 57
        static constexpr Vector4 CornflowerBlue() { return { 0.33f, 0.66f, 1.0f, 1.0f }; }     // #55AAFF  85, 170, 255

    #if __clang__ || _MSC_VER
        Vector4() = default;
        constexpr Vector4(Vector2 xy, Vector2 zw) : xy{xy}, zw{zw} {}
        constexpr Vector4(float x, float y, float z, float w) : x{x}, y{y}, z{z}, w{w} {}
    #endif

        /** @return TRUE if all elements are exactly 0.0f, which implies default initialized.
         * To avoid FP errors, use almostZero() if you performed calculations */
        bool isZero()  const { return x == 0.0f && y == 0.0f && z == 0.0f && w == 0.0f; }
        bool notZero() const { return x != 0.0f || y != 0.0f || z != 0.0f || w != 0.0f; }
        bool hasNaN() const { return isnan(x) || isnan(y) || isnan(z) || isnan(w); }
        
        /** @return TRUE if this vector is almost zero, with all components abs < 0.0001 */
        bool almostZero() const;

        /** @return TRUE if the vectors are almost equal, with a difference of < 0.0001 */
        bool almostEqual(const Vector4& v) const;

        void set(float newX, float newY, float newZ, float newW);
    
        /** @return Dot product with another vector */
        float dot(const Vector4& v) const;

        /** Print the matrix */
        void print() const;

        /** @return Temporary static string from this Matrix4 */
        const char* toString() const;

        char* toString(char* buffer) const;
        char* toString(char* buffer, int size) const;
        template<int SIZE> char* toString(char (&buffer)[SIZE]) const {
            return toString(buffer, SIZE);
        }

        /** @brief Assuming this is a quaternion, gives the Euler XYZ angles (degrees) */
        Vector3 quatToEulerAngles() const;
        Vector3 quatToEulerRadians() const;

        /** @brief Creates a quaternion rotation from an Euler angle (degrees), rotation axis must be specified */
        static Vector4 fromAngleAxis(float degrees, const Vector3& axis)
        { return fromAngleAxis(degrees, axis.x, axis.y, axis.z); }
        static Vector4 fromRadianAxis(float radians, const Vector3& axis)
        { return fromRadianAxis(radians, axis.x, axis.y, axis.z); }


        /** @brief Creates a quaternion rotation from an Euler angle (degrees), rotation axis must be specified */
        static Vector4 fromAngleAxis(float degrees, float x, float y, float z);
        static Vector4 fromRadianAxis(float radians, float x, float y, float z);
    
        /** @brief Creates a quaternion rotation from Euler XYZ (degrees) rotation */
        static Vector4 fromRotationAngles(const Vector3& rotationDegrees);
        static Vector4 fromRotationRadians(const Vector3& rotationRadians);

        /** @brief Creates a quaternion rotation from a 4x4 affine matrix */
        static Vector4 fromRotationMatrix(const Matrix4& rotation);

        /** @brief Creates a quaternion rotation from a 3x3 rotation matrix */
        static Vector4 fromRotationMatrix(const Matrix3& rotation);

        /** @return A 4-component float color from integer RGB color, with A being 1.0f */
        static constexpr Vector4 RGB(int r, int g, int b)
        {
            return { r / 255.0f, g / 255.0f, b / 255.0f, 1.0f };
        }
        /** @return A 4-component float color from integer RGBA color */
        static constexpr Vector4 RGB(int r, int g, int b, int a)
        {
            return { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
        }
        /** @return A 4-component float color with Alpha override */
        static constexpr Vector4 RGB(Vector4 color, float newAlpha)
        {
            return { color.r, color.g, color.b, newAlpha };
        }

        static Vector4 smoothColor(Vector4 src, Vector4 dst, float ratio);

        /**
         * Parses a HEX color string, example: #rrggbb or #rrggbbaa
         * @warning The string must start with '#', otherwise White is returned
         * @return Parsed color value
         */
        static Vector4 HEX(const strview& s) noexcept;

        /**
         * Parses a color by name. Supported colors:
         * white, black, red, green, blue, yellow, orange
         */
        static Vector4 NAME(const strview& s) noexcept;

        /**
         * Parses a color by number values:
         * -) RGBA integer values: 255 0 128 255
         * -) RGBA float values:   0.1 0.2 0.5 1
         * @note All HEX and NUMBER color fields are optional. 
         *       Color channel default is 0 and Alpha channel default is 255
         *       Example '#aa' would give 170 0 0 255  and '0.1' would give 25 0 0 255
         * @return Parsed color in normalized float value range [0.0 ... 1.0]
         */
        static Vector4 NUMBER(strview s) noexcept;

        /**
         * Parses any type of color string. Supported:
         * -) RGBA HEX color strings: #rrggbb or #rrggbbaa
         * -) Named color strings: 'orange'
         * -) RGBA integer values: 255 0 128 255
         * -) RGBA float values:   0.1 0.2 0.5 1
         * @note All HEX and NUMBER color fields are optional. 
         *       Color channel default is 0 and Alpha channel default is 255
         *       Example '#aa' would give 170 0 0 255  and '0.1' would give 25 0 0 255
         * @return Default color White or parsed color in normalized float value range [0.0 ... 1.0]
         */
        static Vector4 parseColor(const strview& s) noexcept;

        /** 
         * @brief Rotates quaternion p with extra rotation q
         * q = additional rotation we wish to rotate with
         * p = original rotation
         */
        Vector4 rotate(const Vector4& q) const;
    
        Vector4& operator*=(const Vector4& q) { return (*this = rotate(q)); }
        Vector4  operator* (const Vector4& q) const { return rotate(q); }
    
        Vector4& operator+=(float f) { x+=f; y+=f; z+=f; w+=f; return *this; }
        Vector4& operator-=(float f) { x-=f; y-=f; z-=f; w-=f; return *this; }
        Vector4& operator*=(float f) { x*=f; y*=f; z*=f; w*=f; return *this; }
        Vector4& operator/=(float f) { x/=f; y/=f; z/=f; w/=f; return *this; }
        Vector4& operator+=(const Vector4& v) { x+=v.x; y+=v.y; z+=v.z; w+=v.w; return *this; }
        Vector4& operator-=(const Vector4& v) { x-=v.x; y-=v.y; z-=v.z; w-=v.w; return *this; }

        Vector4  operator+ (const Vector4& v) const { return { x+v.x, y+v.y, z+v.z, w+v.w }; }
        Vector4  operator- (const Vector4& v) const { return { x-v.x, y-v.y, z-v.z, w-v.w }; }
        Vector4  operator- () const { return {-x, -y, -z, -w }; }
    
        bool operator==(const Vector4& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
        bool operator!=(const Vector4& v) const { return x != v.x || y != v.y || z != v.z || w != v.w; }
    };

    constexpr Vector4 vec4(float x, float y, float z, float w = 1.0f)  { return { x, y, z, w }; }
    constexpr Vector4 vec4(Vector2 xy, Vector2 zw)           { return { xy.x, xy.y, zw.x, zw.y }; }
    constexpr Vector4 vec4(Vector2 xy, float z, float w)     { return { xy.x, xy.y, z,    w    }; }
    constexpr Vector4 vec4(float x, float y, Vector2 zw)     { return { x,    y,    zw.x, zw.y }; }
    constexpr Vector4 vec4(Vector3 xyz, float w)             { return { xyz.x, xyz.y, xyz.z, w }; }
    constexpr Vector4 vec4(float x, Vector3 yzw)             { return { x, yzw.x, yzw.y, yzw.z }; }

    constexpr RectF rect(Vector4 xywh) { return { xywh.x, xywh.y, xywh.x, xywh.y }; }

    inline Vector4 operator+(const Vector4& a, float f) { return { a.x+f, a.y+f, a.z+f, a.w+f }; }
    inline Vector4 operator-(const Vector4& a, float f) { return { a.x-f, a.y-f, a.z-f, a.w-f }; }
    inline Vector4 operator*(const Vector4& a, float f) { return { a.x*f, a.y*f, a.z*f, a.w*f }; }
    inline Vector4 operator/(const Vector4& a, float f) { return { a.x/f, a.y/f, a.z/f, a.w/f }; }
    inline Vector4 operator+(float f, const Vector4& a) { return { f+a.x, f+a.y, f+a.z, f+a.w }; }
    inline Vector4 operator-(float f, const Vector4& a) { return { f-a.x, f-a.y, f-a.z, f-a.w }; }
    inline Vector4 operator*(float f, const Vector4& a) { return { f*a.x, f*a.y, f*a.z, f*a.w }; }
    inline Vector4 operator/(float f, const Vector4& a) { return { f/a.x, f/a.y, f/a.z, f/a.w }; }

    constexpr Vector4 clamp(const Vector4& value, const Vector4& min, const Vector4& max)
    {
        return { value.x < min.x ? min.x : (value.x < max.x ? value.x : max.x),
                 value.y < min.y ? min.y : (value.y < max.y ? value.y : max.y),
                 value.z < min.z ? min.z : (value.z < max.z ? value.z : max.z),
                 value.w < min.w ? min.w : (value.w < max.w ? value.w : max.w) };
    }

    constexpr Vector4 lerp(float position, const Vector4& start, const Vector4& end)
    {
        return { start.x + (end.x - start.x)*position,
                 start.y + (end.y - start.y)*position,
                 start.z + (end.z - start.z)*position,
                 start.w + (end.w - start.w)*position };
    }

    ////////////////////////////////////////////////////////////////////////////////


    struct RPPAPI _Matrix3RowVis
    {
        float x, y, z;
    };


    /**
     * 3x3 Rotation Matrix for OpenGL in row-major order, which is best suitable for MODERN OPENGL development
     */
    struct RPPAPI Matrix3
    {
        union {
            struct {
                float m00, m01, m02; // row0  0-2
                float m10, m11, m12; // row1  0-2
                float m20, m21, m22; // row2  0-2
            };
            struct {
                Vector3 r0, r1, r2; // rows 0-3
            };
            struct {
                _Matrix3RowVis vis0, vis1, vis2;
            };
            float m[12]; // 3x3 float matrix

            Vector3 r[3]; // rows 0-2
        };

        Matrix3() noexcept = default; // NOLINT

        constexpr Matrix3( // NOLINT
            float m00, float m01, float m02,
            float m10, float m11, float m12,
            float m20, float m21, float m22) noexcept :
            m00{m00}, m01{m01}, m02{m02},
            m10{m10}, m11{m11}, m12{m12},
            m20{m20}, m21{m21}, m22{m22} { }

        constexpr Matrix3(Vector3 r0, Vector3 r1, Vector3 r2) noexcept // NOLINT
            : r0{r0}, r1{r1}, r2{r2} { }

        /** @brief Global identity matrix for easy initialization */
        static const Matrix3& Identity();

        /** @brief Loads identity matrix */
        Matrix3& loadIdentity();
    
        /** @brief Multiplies this matrix: this = this * mb */
        Matrix3& multiply(const Matrix3& mb);

        /** @brief Multiplies this matrix ma with matrix mb and returns the result as mc */
        Matrix3 operator*(const Matrix3& mb) const;

        /** @brief Transforms 3D vector v with this matrix and return the resulting vec3 */
        Vector3 operator*(const Vector3& v) const;

        /** @brief Creates ROTATION MATRIX from angle DEGREES around AXIS */
        Matrix3& fromAngleAxis(float angle, Vector3 axis)
        {
            return fromAngleAxis(angle, axis.x, axis.y, axis.z);
        }
        Matrix3& fromAngleAxis(float angle, float x, float y, float z);
        static Matrix3 createAngleAxis(float angle, Vector3 axis)
        {
            Matrix3 rot;
            rot.fromAngleAxis(angle, axis.x, axis.y, axis.z);
            return rot;
        }

        /** @brief Rotates this Matrix3 by angle DEGREES around AXIS */
        Matrix3& rotate(float angle, Vector3 axis)
        {
            return multiply(createAngleAxis(angle, axis));
        }

        // Transposes THIS Matrix4
        Matrix3& transpose();

        // Returns a transposed copy of this matrix
        Matrix3 transposed() const;

        float norm() const;
        float norm(const Matrix3& b) const;

        /** @return TRUE if this matrix looks like a rotation matrix */
        bool isRotationMatrix() const;

        /** Assuming this is a rotation matrix, returns Euler XYZ angles in DEGREES */
        Vector3 toEulerAngles() const;
        Vector3 toEulerRadians() const;

        /**
         * Initializes this Matrix3 with euler angles
         * @param eulerAngles Euler XYZ rotation in degrees
         */
        Matrix3& fromRotationAngles(const Vector3& eulerAngles)
        {
            return fromRotationRadians({radf(eulerAngles.x), radf(eulerAngles.y), radf(eulerAngles.z)});
        }
        Matrix3& fromRotationRadians(const Vector3& eulerRadians);

        static Matrix3 createRotationFromAngles(const Vector3& eulerAngles)
        {
            return createRotationFromRadians({radf(eulerAngles.x), radf(eulerAngles.y), radf(eulerAngles.z)});
        }
        static Matrix3 createRotationFromRadians(const Vector3& eulerRadians);

        /** Print the matrix */
        void print() const;
    
        /** @return Temporary static string from this */
        const char* toString() const;
    
        char* toString(char* buffer) const;
        char* toString(char* buffer, int size) const;
        template<int SIZE> char* toString(char (&buffer)[SIZE]) const {
            return toString(buffer, SIZE);
        }
    };

    
    struct RPPAPI _Matrix4RowVis
    {
        float x, y, z, w;
    };


    /**
     * 4x4 Affine Matrix for OpenGL in row-major order, which is best suitable for MODERN OPENGL development
     */
    struct RPPAPI Matrix4
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
        static const Matrix4& Identity();

        Matrix4() noexcept = default;

        constexpr Matrix4( // NOLINT
            float m00, float m01, float m02, float m03,
            float m10, float m11, float m12, float m13,
            float m20, float m21, float m22, float m23,
            float m30, float m31, float m32, float m33) noexcept :
            m00{m00}, m01{m01}, m02{m02}, m03{m03},
            m10{m10}, m11{m11}, m12{m12}, m13{m13},
            m20{m20}, m21{m21}, m22{m22}, m23{m23},
            m30{m30}, m31{m31}, m32{m32}, m33{m33} { }

        constexpr Matrix4(Vector4 r0, Vector4 r1, Vector4 r2, Vector4 r3) noexcept // NOLINT
            : r0(r0), r1(r1), r2(r2), r3(r3) { }

        /** @brief Loads identity matrix */
        Matrix4& loadIdentity();

        /** @return The translation component of this 4x4 matrix */
        Vector3 getTranslation() const;

        /** @return The scale component of this 4x4 matrix */
        Vector3 getScale() const;

        /** @return The 3x3 normalized Rotation Matrix of this 4x4 affine matrix */
        Matrix3 getRotationMatrix() const;

        /** @return The Euler Angle DEGREES of this 4x4 matrix */
        Vector3 getRotationAngles() const;

        /** @return The Euler Angle RADIANS of this 4x4 matrix */
        Vector3 getRotationRadians() const;

        /** @brief Multiplies this matrix: this = this * mb */
        Matrix4& multiply(const Matrix4& mb);

        /** @brief Multiplies this matrix ma with matrix mb and returns the result as mc */
        Matrix4 operator*(const Matrix4& mb) const;

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
    
        /** @brief Loads an Orthographic projection matrix */
        Matrix4& setOrtho(float left, float right, float bottom, float top);
        static Matrix4 createOrtho(float left, float right, float bottom, float top)
        {
            Matrix4 view = {};
            view.setOrtho(left, right, bottom, top);
            return view;
        }
        // create a classical GUI friendly ortho: 0,0 is topleft
        // left [0, width]  right
        // top  [0, height] bottom
        static Matrix4 createOrtho(int width, int height)
        {
            return createOrtho(0.0f, float(width), float(height), 0.0f);
        }
    
        /** @brief Loads a perspective projection matrix */
        Matrix4& setPerspective(float fov, float width, float height, float zNear, float zFar);
        static Matrix4 createPerspective(float fov, float width, float height, float zNear, float zFar)
        {
            Matrix4 view = {};
            view.setPerspective(fov, width, height, zNear, zFar);
            return view;
        }
        static Matrix4 createPerspective(float fov, int width, int height, float zNear, float zFar)
        {
            Matrix4 view = {};
            view.setPerspective(fov, float(width), float(height), zNear, zFar);
            return view;
        }

        /** @brief Loads a lookAt view/camera matrix */
        Matrix4& setLookAt(const Vector3& eye, const Vector3& center, const Vector3& up);
        static Matrix4 createLookAt(const Vector3& eye, const Vector3& center, const Vector3& up)
        {
            Matrix4 view = {};
            view.setLookAt(eye, center, up);
            return view;
        }
    
        /** @brief Creates a translated matrix from XYZ position */
        Matrix4& fromPosition(const Vector3& position);
        static Matrix4 createTranslation(const Vector3& position)
        {
            Matrix4 mat = Identity();
            mat.translate(position);
            return mat;
        }
    
        /** @brief Creates a rotated matrix from euler XYZ rotation */
        Matrix4& fromRotation(const Vector3& rotationDegrees);
        static Matrix4 createRotation(const Vector3& rotationDegrees)
        {
            Matrix4 mat = {};
            mat.fromRotation(rotationDegrees);
            return mat;
        }
    
        /** @brief Creates a scaled matrix from XYZ scale */
        Matrix4& fromScale(const Vector3& sc);
        static Matrix4 createScale(const Vector3& scale)
        {
            Matrix4 mat = {};
            mat.fromScale(scale);
            return mat;
        }

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

        /**
         * @brief Creates an affine 3D transformation matrix. Rotation is in Euler XYZ degrees.
         */
        Matrix4& setAffine3D(const Vector3& pos, const Vector3& scale, const Vector3& rotationDegrees);
        static Matrix4 createAffine3D(const Vector3& pos, const Vector3& scale, const Vector3& rotationDegrees)
        {
            Matrix4 affine = Identity();
            affine.setAffine3D(pos, scale, rotationDegrees);
            return affine;
        }

        // Transposes THIS Matrix4
        Matrix4& transpose();

        // Returns a transposed copy of this matrix
        Matrix4 transposed() const;

        // Matrix x InverseMatrix = Identity, useful for unprojecting
        Matrix4 inverse() const;

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

    /**
     * Viewport utility for creating a projection matrix and managing projection from
     * 2D to 3D space and vice-versa
     */
    struct RPPAPI PerspectiveViewport
    {
        float fov, width, height, zNear, zFar;
        Matrix4 projection;

        /**
         * Creates a new perspective viewport with a specific field of view.
         * The screen width and height parameters are used for setting the aspect ratio
         * and later for projections between screen space and world space
         */
        PerspectiveViewport(float fov, float width, float height, float zNear = 0.001f, float zFar = 10000.0f)
            : fov(fov), width(width), height(height), zNear(zNear), zFar(zFar)
        {
            projection.setPerspective(fov, width, height, zNear, zFar);
        }

        /**
         * Project from world space to screen space. Screen space is defined by PerspectiveViewport params
         * @param worldPos 3D position in the world
         * @param cameraView View matrix such as Matrix4::createLookAt
         * @return Position on screen. The resulting point can also be outside of the viewport,
         *         which naturally means worldPos is not visible on screen
         */
        Vector2 ProjectToScreen(Vector3 worldPos, const Matrix4& cameraView) const
        {
            Matrix4 viewProjection = cameraView; viewProjection.multiply(projection);
            return ViewProjectToScreen(worldPos, viewProjection);
            
        }
        // Same as ProjectToScreen, but using a premultiplied View-Projection matrix
        Vector2 ViewProjectToScreen(Vector3 worldPos, const Matrix4& viewProjection) const;

        /**
         * Project from screen space to world space. Screen space is defined by PerspectiveViewport params
         * @param screenPos 2D position on the screen
         * @param depth The Z depth from camera position, usually either 0.0 or 1.0
         * @param cameraView View matrix such as Matrix4::createLookAt
         * @return Position in the world. The resulting point depends greatly on depth value
         */
        Vector3 ProjectToWorld(Vector2 screenPos, float depth, const Matrix4& cameraView) const
        {
            Matrix4 viewProjection = cameraView; viewProjection.multiply(projection);
            return InverseViewProjectToWorld(screenPos, depth, viewProjection.inverse());
        }

        // Same as ProjectToWorld, but using a premultiplied View-Projection matrix
        Vector3 ViewProjectToWorld(Vector2 screenPos, float depth, const Matrix4& viewProjection) const
        {
            return InverseViewProjectToWorld(screenPos, depth, viewProjection.inverse());
        }

        // Same as ProjectToWorld, but using an inverse of a premultiplied View-Projection matrix
        // This is the fastest ProjectToWorld
        Vector3 InverseViewProjectToWorld(Vector2 screenPos, float depth, const Matrix4& inverseViewProjection) const;
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

        constexpr IdVector3(int id, const Vector3& v) noexcept
            : Vector3{ v }, ID{ id } {}
        IdVector3(int id, float x, float y, float z) noexcept
        {
            this->x = x;
            this->y = y;
            this->z = z;
            this->ID = id;
        }
    };

    ////////////////////////////////////////////////////////////////////////////////
    
    /** 3D bounding box */
    struct RPPAPI BoundingBox
    {
        Vector3 min;
        Vector3 max;
    
        BoundingBox() : min{0.0f,0.0f,0.0f}, max{0.0f,0.0f,0.0f} {}
        explicit BoundingBox(const Vector3& bbMinMax) : min(bbMinMax), max(bbMinMax) {}
        BoundingBox(const Vector3& bbMin, const Vector3& bbMax) : min(bbMin), max(bbMax) {}

        explicit operator bool()  const { return min.notZero() && max.notZero(); }
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

        // get the bounding radius: (max-min).length() / 2
        float radius() const noexcept;

        // get the bounding diagonal length (enclosing diameter): (max-min).length()
        float diagonal() const noexcept;

        Vector3 compare(const BoundingBox& bb) const noexcept;

        // joins a vector into this bounding box, possibly increasing the volume
        void join(const Vector3& v) noexcept;

        // joins with another bounding box, possibly increasing the volume
        void join(const BoundingBox& bbox) noexcept;

        // @return TRUE if vec3 point is inside this bounding box volume
        bool contains(const Vector3& v) const noexcept;

        // @return Distance to vec3 point from this boundingbox's corners
        float distanceTo(const Vector3& v) const noexcept;

        // grow the bounding box by the given value across all axes
        void grow(float growth) noexcept;

        // creates a new bounding box with a specific radius
        static BoundingBox create(float radius) noexcept;

        // calculates the bounding box of the given point cloud
        static BoundingBox create(const std::vector<Vector3>& points) noexcept;

        // calculates the bounding box using ID-s from IdVector3-s, which index the given point cloud
        static BoundingBox create(const std::vector<Vector3>& points, const std::vector<IdVector3>& ids) noexcept;

        // calculates the bounding box using vertex ID-s, which index the given point cloud
        static BoundingBox create(const std::vector<Vector3>& points, const std::vector<int>& ids) noexcept;

        /**
         * Calculates the bounding box from an arbitrary vertex array
         * @note Position data must be the first Vector3 element!
         * @param vertexData Array of vertices
         * @param vertexCount Number of vertices in vertexData
         * @param stride The step in bytes(!!) to the next element
         * 
         * Example:
         * @code
         *     vector<Vertex3UVNorm> vertices;
         *     auto bb = BoundingBox::create((Vector3*)vertices.data(), vertices.size(), sizeof(Vertex3UVNorm));
         * @endcode
         */
        static BoundingBox create(const Vector3* vertexData, int vertexCount, int stride) noexcept;

        /**
         * Calculates the bounding box from an arbitrary vertex array
         * @note Position data must be the first Vector3 element!
         * 
         * Example:
         * @code
         *     vector<Vertex3UVNorm> vertices;
         *     auto bb = BoundingBox::create(vertices);
         * @endcode
         */
        template<class Vertex> static BoundingBox create(const std::vector<Vertex>& vertices)
        {
            return create((Vector3*)vertices.data(), (int)vertices.size(), sizeof(Vertex));
        }
        template<class Vertex> static BoundingBox create(const Vertex* vertices, int vertexCount)
        {
            return create((Vector3*)vertices, vertexCount, sizeof(Vertex));
        }
    };

    ////////////////////////////////////////////////////////////////////////////////

    struct RPPAPI BoundingSphere
    {
        Vector3 center;
        float radius;
        BoundingSphere() : center{0.0f, 0.0f, 0.0f}, radius{0.0f} {}
        BoundingSphere(const Vector3& center, float radius) : center{center}, radius{radius} {}

        /**
         * Calculates the bounding sphere from an arbitrary vertex array
         * @note Position data must be the first Vector3 element!
         * @param vertexData Array of vertices
         * @param vertexCount Number of vertices in vertexData
         * @param stride The step in bytes(!!) to the next element
         * 
         * Example:
         * @code
         *     vector<Vertex3UVNorm> vertices;
         *     auto s = BoundingSphere::create((Vector3*)vertices.data(), vertices.size(), sizeof(Vertex3UVNorm));
         * @endcode
         */
        static BoundingSphere create(const Vector3* vertexData, int vertexCount, int stride) noexcept;

        // calculates the bounding sphere from basic bounding box of the point cloud
        static BoundingSphere create(const std::vector<Vector3>& points) noexcept
        {
            return create(points.data(), (int)points.size(), sizeof(Vector3));
        }
        template<class Vertex> static BoundingSphere create(const std::vector<Vertex>& vertices)
        {
            return create((Vector3*)vertices.data(), (int)vertices.size(), sizeof(Vertex));
        }
        template<class Vertex> static BoundingSphere create(const Vertex* vertices, int vertexCount)
        {
            return create((Vector3*)vertices, vertexCount, sizeof(Vertex));
        }
    };

    ////////////////////////////////////////////////////////////////////////////////

    struct RPPAPI Ray
    {
        Vector3 origin;
        Vector3 direction;

        // Ray-Sphere intersection 
        // @return Distance from rayStart to intersection OR 0.0f if no solutions
        float intersectSphere(Vector3 sphereCenter, float sphereRadius) const noexcept;

        // Exactly the same as Ray-Sphere intersection. Only difference is usage
        // semantics, where the intent is to Raycast using a cylindrical ray.
        // @return Distance from rayStart to intersection OR 0.0f if no solutions
        float intersectPoint(Vector3 point, float rayRadius) const noexcept
        {
            return intersectSphere(point, rayRadius);
        }

        // Ray-Triangle intersection
        // @return Distance from rayStart to intersection OR 0.0f if no solutions 
        float intersectTriangle(Vector3 v0, Vector3 v1, Vector3 v2) const noexcept;
    };

    ////////////////////////////////////////////////////////////////////////////////

    inline std::string to_string(const Vector2& v)  { char buf[32];  return { v.toString(buf, sizeof(buf)) }; }
    inline std::string to_string(const Point& v)    { char buf[48];  return { v.toString(buf, sizeof(buf)) }; }
    inline std::string to_string(const Vector3& v)  { char buf[48];  return { v.toString(buf, sizeof(buf)) }; }
    inline std::string to_string(const Vector3d& v) { char buf[48];  return { v.toString(buf, sizeof(buf)) }; }
    inline std::string to_string(const Vector4& v)  { char buf[64];  return { v.toString(buf, sizeof(buf)) }; }
    inline std::string to_string(const Matrix4& v)  { char buf[256]; return { v.toString(buf, sizeof(buf)) }; }

    ////////////////////////////////////////////////////////////////////////////////
} // namespace rpp

#if _MSC_VER
#pragma warning(pop) // nameless struct/union warning
#endif
