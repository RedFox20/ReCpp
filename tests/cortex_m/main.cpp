// Runs the rpp tests in one FreeRTOS task. exit() reports the result to QEMU through semihosting
#include <rpp/tests.h>
#include <FreeRTOS.h>
#include <task.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h> // _exit

extern "C" void rpp_cm_assert_failed(const char* file, int line)
{
    printf("configASSERT failed at %s:%d\n", file, line);
    _exit(98);
}

extern "C" void vApplicationStackOverflowHook(TaskHandle_t, char* task_name)
{
    printf("stack overflow in task %s\n", task_name);
    _exit(97);
}

// newlib malloc is not thread safe, and the idle task frees the stack of a deleted task
extern "C" void __malloc_lock(struct _reent*) { vTaskSuspendAll(); }
extern "C" void __malloc_unlock(struct _reent*) { (void)xTaskResumeAll(); }

extern "C" void vApplicationMallocFailedHook()
{
    printf("FreeRTOS heap is full\n");
    _exit(96);
}

static int test_argc;
static char** test_argv;

static void run_all_tests(void*)
{
    int result = rpp::test::run_tests(test_argc, test_argv);
    rpp::test::cleanup_all_tests();
    exit(result);
}

// the semihosting command line, which qemu-system-arm -append sets, selects the tests
int main(int argc, char** argv)
{
    test_argc = argc;
    test_argv = argv;
    // the test cases put long strings on the stack
    xTaskCreate(run_all_tests, "rpp_tests", 16 * 1024, nullptr, tskIDLE_PRIORITY + 1, nullptr);
    vTaskStartScheduler();
    printf("vTaskStartScheduler() returned\n");
    return 95;
}
