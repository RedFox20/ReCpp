#include <rpp/concurrent_queue.h>
#include <rpp/future.h>
#include <rpp/tests.h>
using namespace rpp;

TestImpl(test_concurrent_queue)
{
    TestInit(test_concurrent_queue)
    {
    }

    TestCase(basic_enqueue_dequeue)
    {
        concurrent_queue<string> queue;
        queue.push("item1");
        queue.push("item2");
        queue.push("item3");
        AssertThat(queue.size(), 3);
        AssertThat(queue.empty(), false);

        AssertThat(queue.pop(), "item1");
        AssertThat(queue.pop(), "item2");
        AssertThat(queue.pop(), "item3");
        AssertThat(queue.size(), 0);
        AssertThat(queue.empty(), true);
    }

    TestCase(basic_producer_consumer)
    {
        concurrent_queue<string> queue;

        cfuture<void> producer = rpp::async_task([&] {
            queue.push("item1");
            queue.push("item2");
            queue.push("item3");
        });

        cfuture<void> consumer = rpp::async_task([&] {
            AssertThat(queue.wait_pop(), "item1");
            AssertThat(queue.wait_pop(), "item2");
            AssertThat(queue.wait_pop(), "item3");
        });

        producer.get();
        consumer.get();
    }
};
