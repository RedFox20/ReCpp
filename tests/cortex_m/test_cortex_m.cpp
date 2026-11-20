// A lock case contends with a second FreeRTOS task, because newlib on arm-none-eabi has no std::thread.
// The helper task releases each wait, and it never asserts or logs, because every task shares one TLS block
#include <rpp/mutex.h>
#include <rpp/semaphore.h>
#include <rpp/tests.h>
#include <rpp/timepoint.h> // rpp::TimePoint
#include <FreeRTOS.h>
#include <task.h>
using namespace rpp;

TestImpl(test_cortex_m)
{
    TestInit(test_cortex_m)
    {
    }

    static constexpr int ROUNDS = 2000;

    struct shared
    {
        rpp::futex_mutex mutex;
        rpp::semaphore items { 0 };
        rpp::semaphore_flag done;
        int counter = 0;
    };

    // one priority for both tasks, so a waiter which yields lets the other task run
    static void start_task(TaskFunction_t body, shared& s)
    {
        BaseType_t created = xTaskCreate(body, "helper", 1024, &s, uxTaskPriorityGet(nullptr), nullptr);
        AssertThat(created, pdPASS);
    }

    static void add_rounds(shared& s)
    {
        for (int i = 0; i < ROUNDS; ++i)
        {
            rpp::lock_guard lock { s.mutex };
            int value = s.counter;
            taskYIELD(); // the other task runs while this one holds the lock
            s.counter = value + 1;
        }
    }

    TestCase(futex_mutex_serializes_two_tasks)
    {
        shared s;
        start_task([](void* arg)
        {
            shared& data = *static_cast<shared*>(arg);
            add_rounds(data);
            data.done.notify();
            vTaskDelete(nullptr);
        }, s);
        add_rounds(s);
        AssertEqual(rpp::semaphore::notified, s.done.wait(rpp::seconds(1)));
        AssertThat(s.counter, 2 * ROUNDS);
    }

    TestCase(semaphore_wakes_a_waiting_task)
    {
        shared s;
        start_task([](void* arg)
        {
            shared& data = *static_cast<shared*>(arg);
            for (int i = 0; i < ROUNDS; ++i)
                data.items.notify();
            data.done.notify();
            vTaskDelete(nullptr);
        }, s);
        for (int i = 0; i < ROUNDS; ++i)
            AssertEqual(rpp::semaphore::notified, s.items.wait(rpp::seconds(1)));
        AssertEqual(rpp::semaphore::notified, s.done.wait(rpp::seconds(1)));
        AssertThat(s.items.try_wait(), false);
    }

    // QEMU reads the DWT cycle counter as 0, so the spin takes the loop. A stopped counter or a wrapped count hangs
    TestCase(spin_sleep_ends_without_a_cycle_counter)
    {
        const rpp::TimePoint start = rpp::TimePoint::monotonic_now();
        spin_sleep_for_us(0);
        spin_sleep_for_us(1000);
        AssertLess((rpp::TimePoint::monotonic_now() - start).nsec, rpp::seconds(1).nsec);
    }
};
