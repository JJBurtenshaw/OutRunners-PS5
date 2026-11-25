#include <stdint.h>
#include <stddef.h>

#include "../orunners_scheduler/host/tasks.h"

extern void orunners_visual_checkpoint(int stage);

extern int orunners_task_allocation_error(void);
extern int orunners_task_identity_valid(void);

extern int orunners_task_stack_contains(
    host_task *task,
    const volatile void *address);

extern void orunners_tasks_release_all(void);

#define TEST_CYCLES 10000u

static host_task *main_task;
static host_task *task_a;
static host_task *task_b;

static unsigned count_a;
static unsigned count_b;

static int error_code;
static int result;

static unsigned read_sse_rounding(void)
{
    uint32_t value;

    __asm__ volatile(
        "stmxcsr %0"
        : "=m"(value)
    );

    return (value >> 13) & 3u;
}

static unsigned read_x87_rounding(void)
{
    uint16_t value;

    __asm__ volatile(
        "fnstcw %0"
        : "=m"(value)
    );

    return (value >> 10) & 3u;
}

static void set_rounding(unsigned sse, unsigned x87)
{
    uint32_t mxcsr;
    uint16_t control;

    __asm__ volatile(
        "stmxcsr %0"
        : "=m"(mxcsr)
    );

    __asm__ volatile(
        "fnstcw %0"
        : "=m"(control)
    );

    mxcsr =
        (mxcsr & ~(3u << 13)) |
        ((sse & 3u) << 13);

    control = (uint16_t)(
        (control & ~(3u << 10)) |
        ((x87 & 3u) << 10)
    );

    __asm__ volatile(
        "ldmxcsr %0"
        :
        : "m"(mxcsr)
        : "memory"
    );

    __asm__ volatile(
        "fldcw %0"
        :
        : "m"(control)
        : "memory"
    );
}

static void fail_test(int code)
{
    error_code = code;

    for (;;)
        host_task_switch(main_task);
}

static void run_a(void)
{
    volatile uint64_t guard =
        UINT64_C(0xA123456789ABCDEF);

    set_rounding(1, 1);

    for (;;)
    {
        if (host_task_current() != task_a ||
            !orunners_task_identity_valid())
            fail_test(95);

        if (guard != UINT64_C(0xA123456789ABCDEF) ||
            !orunners_task_stack_contains(
                task_a, &guard))
            fail_test(97);

        if (read_sse_rounding() != 1 ||
            read_x87_rounding() != 1)
            fail_test(98);

        ++count_a;

        host_task_switch(task_b);
    }
}

static void run_b(void)
{
    volatile uint64_t guard =
        UINT64_C(0xB987654321FEDCBA);

    set_rounding(2, 2);

    for (;;)
    {
        if (host_task_current() != task_b ||
            !orunners_task_identity_valid())
            fail_test(95);

        if (guard != UINT64_C(0xB987654321FEDCBA) ||
            !orunners_task_stack_contains(
                task_b, &guard))
            fail_test(97);

        if (read_sse_rounding() != 2 ||
            read_x87_rounding() != 2)
            fail_test(98);

        ++count_b;

        host_task_switch(main_task);
    }
}

int orunners_task_probe(void)
{
    if (result)
        return result;

    orunners_visual_checkpoint(1);

    main_task = host_task_main();

    if (!main_task)
    {
        orunners_visual_checkpoint(91);
        return result = -1;
    }

    const unsigned main_sse = read_sse_rounding();
    const unsigned main_x87 = read_x87_rounding();

    orunners_visual_checkpoint(2);

    task_a = host_task_create(run_a);

    if (!task_a)
    {
        orunners_visual_checkpoint(
            orunners_task_allocation_error());

        return result = -1;
    }

    orunners_visual_checkpoint(3);

    task_b = host_task_create(run_b);

    if (!task_b)
    {
        int error = orunners_task_allocation_error();

        orunners_tasks_release_all();

        orunners_visual_checkpoint(error);

        return result = -1;
    }

    orunners_visual_checkpoint(4);

    if (task_a == task_b)
    {
        orunners_tasks_release_all();

        orunners_visual_checkpoint(94);

        return result = -1;
    }

    orunners_visual_checkpoint(6);

    for (unsigned i = 0; i < TEST_CYCLES; ++i)
    {
        volatile uint64_t guard =
            UINT64_C(0x1234FEDC5678ABCD);

        host_task_switch(task_a);

        if (error_code)
            break;

        if (host_task_current() != main_task ||
            !orunners_task_identity_valid())
        {
            error_code = 95;
            break;
        }

        if (guard != UINT64_C(0x1234FEDC5678ABCD))
        {
            error_code = 97;
            break;
        }

        if (read_sse_rounding() != main_sse ||
            read_x87_rounding() != main_x87)
        {
            error_code = 98;
            break;
        }

        if (count_a != i + 1 ||
            count_b != i + 1)
        {
            error_code = 99;
            break;
        }

        if (i == 0)
            orunners_visual_checkpoint(7);
    }

    if (!error_code &&
        (count_a != TEST_CYCLES ||
         count_b != TEST_CYCLES))
    {
        error_code = 99;
    }

    orunners_tasks_release_all();

    task_a = NULL;
    task_b = NULL;

    if (error_code)
    {
        orunners_visual_checkpoint(error_code);

        return result = -1;
    }

    orunners_visual_checkpoint(8);
    orunners_visual_checkpoint(9);

    return result = 1;
}
