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
        BoundingBox unitbox = { Vector3::ZERO, Vector3::ONE };

        AssertThat(unitbox.center(), Vector3(0.5f));
        AssertThat(unitbox.volume(), 1.0f);
        AssertThat(unitbox.radius(), Vector3(0.5f).length());
        AssertThat(unitbox.distanceTo({ 2.0f,1.0f,1.0f }), 1.0f);
        AssertThat(unitbox.contains({ 0.5f,0.5f,0.5f }), true);

        AssertThat(unitbox.isZero(), false);
        AssertThat(unitbox.notZero(), true);
        AssertThat(unitbox.width(),  1.0f);
        AssertThat(unitbox.height(), 1.0f);
        AssertThat(unitbox.depth(),  1.0f);
    }

} Impl;
