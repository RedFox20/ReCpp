/**
 * Basic Vector Math, Copyright (c) 2015 - Jorma Rebane
 */
#include "vec.h"
#include <stdio.h>
#include <algorithm> // min,max

namespace rpp
{
    const Vector2 Vector2::ZERO  = { 0.0f, 0.0f };
    const Vector2 Vector2::ONE   = { 1.0f, 1.0f };
    const Vector2 Vector2::RIGHT = { 1.0f, 0.0f };
    const Vector2 Vector2::UP    = { 0.0f, 1.0f };

    const Rect Rect::ZERO = { 0.0f, 0.0f, 0.0f, 0.0f };

    const Vector3 Vector3::ZERO    = { 0.0f, 0.0f, 0.0f };
    const Vector3 Vector3::ONE     = { 1.0f, 1.0f, 1.0f };
    const Vector3 Vector3::RIGHT   = { 1.0f, 0.0f, 0.0f };
    const Vector3 Vector3::FORWARD = { 0.0f, 1.0f, 0.0f };
    const Vector3 Vector3::UP      = { 0.0f, 0.0f, 1.0f };

    const Vector4 Vector4::ZERO    = { 0.0f, 0.0f, 0.0f, 0.0f };
    const Vector4 Vector4::WHITE   = { 1.0f, 1.0f, 1.0f, 1.0f };
    const Vector4 Vector4::BLACK   = { 0.0f, 0.0f, 0.0f, 1.0f };
    const Vector4 Vector4::RED     = { 1.0f, 0.0f, 0.0f, 1.0f };
    const Vector4 Vector4::GREEN   = { 0.0f, 1.0f, 0.0f, 1.0f };
    const Vector4 Vector4::SWEETGREEN = Color::RGB(86, 188, 57);
    const Vector4 Vector4::BLUE    = { 0.0f, 0.0f, 1.0f, 1.0f };
    const Vector4 Vector4::YELLOW  = { 1.0f, 1.0f, 0.0f, 1.0f };
    const Vector4 Vector4::ORANGE  = { 1.0f, 0.5f, 0.0f, 1.0f };

    ////////////////////////////////////////////////////////////////////////////////

    float radf(float degrees)
    {
        return (degrees * (float)M_PI) / 180.0f; // rads=(degs*PI)/180
    }

    ///////////////////////////////////////////////////////////////////////////////

    void Vector2::print() const
    {
        char buffer[256];
        puts(toString(buffer));
    }
    
    const char* Vector2::toString() const
    {
        static char buffer[256];
        return toString(buffer, sizeof(buffer));
    }
    
    char* Vector2::toString(char* buffer) const
    {
        return toString(buffer, 256);
    }
    
    char* Vector2::toString(char* buffer, int size) const
    {
        snprintf(buffer, size, "{%.3g;%.3g}", x, y);
        return buffer;
    }
    
    void Vector2::set(float newX, float newY)
    {
        x=newX, y=newY;
    }

    float Vector2::length() const
    {
        return sqrtf(x*x + y*y);
    }

    float Vector2::sqlength() const
    {
        return x*x + y*y;
    }

    void Vector2::normalize()
    {
        float inv = 1.0f / sqrtf(x*x + y*y);
        x *= inv, y *= inv;
    }

    void Vector2::normalize(const float magnitude)
    {
        float inv = magnitude / sqrtf(x*x + y*y);
        x *= inv, y *= inv;
    }

    Vector2 Vector2::normalized() const
    {
        float inv = 1.0f / sqrtf(x*x + y*y);
        return{ x*inv, y*inv };
    }

    Vector2 Vector2::normalized(const float magnitude) const
    {
        float inv = magnitude / sqrtf(x*x + y*y);
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

    ////////////////////////////////////////////////////////////////////////////////

    const char* Point::toString() const
    {
        static char buf[64]; snprintf(buf, sizeof(buf), "{%d;%d}", x, y);
        return buf;
    }


    ////////////////////////////////////////////////////////////////////////////////

    void Rect::print() const
    {
        char buffer[256];
        puts(toString(buffer));
    }

    const char* Rect::toString() const
    {
        static char buffer[256];
        return toString(buffer, sizeof(buffer));
    }

    char* Rect::toString(char* buffer) const
    {
        return toString(buffer, 256);
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

    ////////////////////////////////////////////////////////////////////////////////

    void Vector3::set(float newX, float newY, float newZ)
    {
        x=newX, y=newY, z=newZ;
    }

    float Vector3::length() const
    {
        return sqrtf(x*x + y*y + z*z);
    }

    float Vector3::sqlength() const
    {
        return x*x + y*y + z*z;
    }

    void Vector3::normalize()
    {
        float inv = 1.0f / sqrtf(x*x + y*y + z*z);
        x *= inv, y *= inv, z *= inv;
    }

    void Vector3::normalize(const float magnitude)
    {
        float inv = magnitude / sqrtf(x*x + y*y + z*z);
        x *= inv, y *= inv, z *= inv;
    }

    Vector3 Vector3::normalized() const
    {
        float inv = 1.0f / sqrtf(x*x + y*y + z*z);
        return{ x*inv, y*inv, z*inv };
    }
    Vector3 Vector3::normalized(const float magnitude) const
    {
        float inv = magnitude / sqrtf(x*x + y*y + z*z);
        return { x*inv, y*inv, z*inv };
    }

    Vector3 Vector3::cross(const Vector3& b) const
    {
        return { y*b.z - b.y*z, z*b.x - b.z*x, x*b.y - b.x*y };
    }

    float Vector3::dot(const Vector3& b) const
    {
        return x*b.x + y*b.y + z*b.z;
    }

	void Vector3::print() const
	{
		char buffer[256];
		puts(toString(buffer));
	}

	const char* Vector3::toString() const
	{
		static char buffer[256];
		return toString(buffer, sizeof(buffer));
	}

	char* Vector3::toString(char* buffer) const
	{
		return toString(buffer, 256);
	}

	char* Vector3::toString(char* buffer, int size) const
	{
		snprintf(buffer, size, "{%.3g;%.3g;%.3g}", x, y, z);
		return buffer;
	}

	///////////////////////////////////////////////////////////////////////////////

    void Vector4::set(float newX, float newY, float newZ, float newW)
    {
        x = newX, y = newY, z = newZ, w = newW;
    }

    float Vector4::dot(const Vector4& v) const
    {
        return x*v.x + y*v.y + z*v.z + w*v.w;
    }

    Vector4 Vector4::fromAngleAxis(float angle, const Vector3& axis)
    {
        return fromAngleAxis(angle, axis.x, axis.y, axis.z);
    }

    Vector4 Vector4::fromAngleAxis(float angle, float x, float y, float z)
    {
        const float r = radf(angle) * 0.5f;
        const float s = sinf(r);
        return Vector4(x*s, y*s, z*s, cosf(r));
    }

    Vector4 Vector4::fromRotation(const Vector3& rotation)
    {
        Vector4 q = fromAngleAxis(rotation.x, 1.0f, 0.0f, 0.0f);
                q = fromAngleAxis(rotation.y, 0.0f, 1.0f, 0.0f) * q;
        return      fromAngleAxis(rotation.z, 0.0f, 0.0f, 1.0f) * q;
    }

    Vector4 Vector4::rotate(const Vector4& q) const
    {
        return Vector4(
                q.w*w - q.x*x - q.y*y - q.z*z,
                q.w*x + q.x*w + q.y*z - q.z*y,
                q.w*y + q.y*w + q.z*x - q.x*z,
                q.w*z + q.z*w + q.x*y - q.y*x
        );
    }
    
    const Vector4 Vector4::HEX(const strview& s) noexcept
    {
        Color c = Color::WHITE;
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

    const Vector4 Vector4::NAME(const strview& s) noexcept
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

    const Vector4 Vector4::NUMBER(strview s) noexcept
    {
        Color c;
        strview r = s.next(' ');
        strview g = s.next(' ');
        strview b = s.next(' ');
        strview a = s.next(' ');
        c.r = r.to_float();
        c.g = g.to_float();
        c.b = b.to_float();
        c.a = a ? a.to_float() : 1.0f;
        if (c.r > 1.0f) c.r /= 255.0f;
        if (c.g > 1.0f) c.g /= 255.0f;
        if (c.b > 1.0f) c.b /= 255.0f;
        if (c.a > 1.0f) c.a /= 255.0f;
        return c;
    }

    const Vector4 Vector4::parseColor(const strview& s) noexcept
    {
        if (!s)
            return WHITE;

        if (s[0] == '#')
            return HEX(s);

        if (isalpha(s[0]))
            return NAME(s);

        return NUMBER(s);
    }

    ///////////////////////////////////////////////////////////////////////////////

    const Matrix4 Matrix4::IDENTITY = {{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    }};

    Matrix4& Matrix4::loadIdentity()
    {
        return (*this = IDENTITY);
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

    Vector4 Matrix4::operator*(const Vector3& v) const
    {
        return {
                (m00*v.x) + (m10*v.y) + (m20*v.z) + m30,
                (m01*v.x) + (m11*v.y) + (m21*v.z) + m31,
                (m02*v.x) + (m12*v.y) + (m22*v.z) + m32,
                (m03*v.x) + (m13*v.y) + (m23*v.z) + m33
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
        r3 = this->operator*(offset);
        return *this;
    }

    Matrix4& Matrix4::rotate(float angleDegs, const Vector3& rotationAxis)
    {
        float rad = radf(angleDegs);
        float c = cosf(rad);
        const Vector3 axis  = rotationAxis.normalized();
        const Vector3 temp  = axis * (1.0f - c);
        const Vector3 sAxis = axis * sinf(rad);
        const Vector3 rot0 = {  c + temp.x*axis.x,
                                temp.x*axis.y + sAxis.z,
                                temp.x*axis.z - sAxis.y, };
        const Vector3 rot1 = {  temp.y*axis.x - sAxis.z,
                                c + temp.y*axis.y,
                                temp.y*axis.z + sAxis.x, };
        const Vector3 rot2 = {  temp.z*axis.x + sAxis.y,
                                temp.z*axis.y - sAxis.x,
                                c + temp.z*axis.z };

        const Vector4 new0 = (r0 * rot0.x) + (r1 * rot0.y) + (r2 * rot0.z);
        const Vector4 new1 = (r0 * rot1.x) + (r1 * rot1.y) + (r2 * rot1.z);
        const Vector4 new2 = (r0 * rot2.x) + (r1 * rot2.y) + (r2 * rot2.z);
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
        m00 = 2.0f / rl, m01 = 0.0f, m02 = 0.0f,  m03 = 0.0f;
        m10 = 0.0f, m11 = 2.0f / tb, m12 = 0.0f,  m13 = 0.0f;
        m20 = 0.0f, m21 = 0.0f,      m22 = -2.0f/dt, m23 = 0.0f;
        m30 = -(right+left)/rl, m31 = -(top+bottom)/tb, m32 = -(far+near)/dt, m33 = 1.0f;
        return *this;
    }

    Matrix4& Matrix4::setPerspective(float fov, float width, float height, float zNear, float zFar)
    {
        const float rad2 = radf(fov) * 0.5f;
        const float h = cosf(rad2) / sinf(rad2);
        const float w = (h * height) / width;
        const float range = zFar - zNear;
        m00 = w, m01 = 0, m02 = 0, m03 = 0;
        m10 = 0, m11 = h, m12 = 0, m13 = 0;
        m20 = 0, m21 = 0, m22 = -(zFar + zNear) / range, m23 = -1;
        m30 = 0, m31 = 0, m32 = (-2.0f * zFar * zNear) / range, m33 = 1;
        return *this;
    }

    Matrix4& Matrix4::setLookAt(const Vector3 &eye, const Vector3 &center, const Vector3 &up)
    {
        const Vector3 f = (center - eye).normalized();
        const Vector3 s = f.cross(up.normalized()).normalized();
        const Vector3 u = s.cross(f);
        m00 = s.x, m01 = u.x, m02 = -f.x, m03 = 0.0f;
        m10 = s.y, m11 = u.y, m12 = -f.y, m13 = 0.0f;
        m20 = s.z, m21 = u.z, m22 = -f.z, m23 = 0.0f;
        m30 = -s.dot(eye), m31 = -u.dot(eye), m32 = f.dot(eye), m33 = 1.0f;
        return *this;
    }

    Matrix4& Matrix4::fromPosition(const Vector3& position)
    {
        *this = IDENTITY;
        return translate(position);
    }

    Matrix4& Matrix4::fromRotation(const Vector3& rotation)
    {
        Vector4 q = Vector4::fromRotation(rotation);
        m00 = 1 - 2 * q.y * q.y - 2 * q.z * q.z;
        m01 = 2 * q.x * q.y + 2 * q.w * q.z;
        m02 = 2 * q.x * q.z - 2 * q.w * q.y;
        m03 = 0.0f;
        m10 = 2 * q.x * q.y - 2 * q.w * q.z;
        m11 = 1 - 2 * q.x * q.x - 2 * q.z * q.z;
        m12 = 2 * q.y * q.z + 2 * q.w * q.x;
        m13 = 0.0f;
        m20 = 2 * q.x * q.z + 2 * q.w * q.y;
        m21 = 2 * q.y * q.z - 2 * q.w * q.x;
        m22 = 1 - 2 * q.x * q.x - 2 * q.y * q.y;
        m23 = 0.0f;
        m30 = 0.0f;
        m31 = 0.0f;
        m32 = 0.0f;
        m33 = 1.0f;
        return *this;
    }

    Matrix4& Matrix4::fromScale(const Vector3& sc)
    {
        m00 = sc.x, m01 = 0.0f, m02 = 0.0f, m03 = 0.0f;
        m10 = 0.0f, m11 = sc.y, m12 = 0.0f, m13 = 0.0f;
        m20 = 0.0f, m21 = 0.0f, m22 = sc.z, m23 = 0.0f;
        m30 = 0.0f, m31 = 0.0f, m32 = 0.0f, m33 = 1.0f;
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
        int len = snprintf(buffer, size, "{\n");
        for (int i = 0; i < 4; ++i)
            len += snprintf(buffer+len, size-len, " %8.3f,%8.3f,%8.3f,%8.3f\n",
                                                  r[i].x,r[i].y,r[i].z,r[i].w);
        len += snprintf(buffer+len, size-len, "}");
        return buffer;
    }

	float BoundingBox::volume() const
	{
		return width() * height() * depth();
	}

	Vector3 BoundingBox::compare(const BoundingBox& bb) const
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

	////////////////////////////////////////////////////////////////////////////////

} // namespace rpp

