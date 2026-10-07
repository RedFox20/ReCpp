// Qt defines these keywords as macros, and a Qt consumer includes ReCpp after them
#define slots
#define signals public
#define emit
#include <rpp/delegate.h>
#undef slots
#undef signals
#undef emit
#include <rpp/tests.h>
using namespace rpp;


TestImpl(test_qt_keywords)
{
    TestInit(test_qt_keywords)
    {
    }

    TestCase(multicast_delegate_compiles_after_qt_keywords)
    {
        int sum = 0;
        multicast_delegate<int> evt;
        evt += [&sum](int x) { sum += x; };
        evt(2);
        AssertThat(sum, 2);
    }
};
