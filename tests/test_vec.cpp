#include <rpp/tests.h>
#include <rpp/vec.h>
using namespace rpp;

TestImpl(test_vec)
{
    TestInit(test_vec)
    {
    }

    TestCase(initialization)
    {
#if _DEBUG // this is only possible in unoptimized builds, because /O2 will load Vector2 into xmm registers
        Vector2 point;

        // Force un-initialized variables
        AssertNotEqual(point.x, 0.0f);
        AssertNotEqual(point.y, 0.0f);
#endif
    }

    TestCase(bounding_box)
    {
        BoundingBox unitbox = { Vector3::Zero(), Vector3::One() };

        AssertThat(unitbox.center(), vec3(0.5f));
        AssertThat(unitbox.volume(), 1.0f);
        AssertThat(unitbox.radius(), vec3(0.5f).length());
        AssertThat(unitbox.distanceTo({ 2.0f,1.0f,1.0f }), 1.0f);
        AssertThat(unitbox.contains({ 0.5f,0.5f,0.5f }), true);

        AssertThat(unitbox.isZero(), false);
        AssertThat(unitbox.notZero(), true);
        AssertThat(unitbox.width(),  1.0f);
        AssertThat(unitbox.height(), 1.0f);
        AssertThat(unitbox.depth(),  1.0f);
    }

    TestCase(rect_clip_keeps_the_far_edges)
    {
        Recti r { 0, 0, 100, 100 };
        r.clip({ 10, 10, 200, 200 });
        AssertThat(r == Recti(10, 10, 90, 90), true);

        RectF f { 0.0f, 0.0f, 100.0f, 100.0f };
        f.clip({ 10.0f, 10.0f, 200.0f, 200.0f });
        AssertThat(f.right(), 100.0f);
        AssertThat(f.bottom(), 100.0f);
    }

    TestCase(vector_to_string_keeps_one_buffer_per_type)
    {
        const char* v2 = Vector2{1, 2}.toString();
        const char* v3 = Vector3{1, 2, 3}.toString();
        const char* v4 = Vector4{1, 2, 3, 4}.toString();
        AssertThat(std::string{v2}, "{1;2}");
        AssertThat(std::string{v3}, "{1;2;3}");
        AssertThat(std::string{v4}, "{1;2;3;4}");
    }

};
