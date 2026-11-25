/*
 * OutRunners PS5 cooperative task adapter.
 *
 * Context switching:
 * Hardware-verified libco System V AMD64 backend.
 *
 * Native stack allocation:
 * Cached direct-memory parameters from the
 * PS5 Native SDL2 Starter framebuffer allocator.
 *
 * Original OutRunners host_task API retained.
 */

#include "host/tasks.h"
#include "../orunners_libco/libco.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef OR_HOST_TEST

#include <sys/types.h>
#include <sys/mman.h>

extern int sceKernelAllocateMainDirectMemory(
    size_t length,
    size_t alignment,
    int memory_type,
    off_t *physical);

extern int sceKernelMapDirectMemory(
    void **address,
    size_t length,
    int protection,
    int flags,
    off_t physical,
    size_t alignment);

extern int sceKernelReleaseDirectMemory(
    off_t physical,
    size_t length);

#endif

#define TASK_STACK_BYTES (16u * 1024u * 1024u)
#define TASK_ALIGNMENT   0x20000u

struct host_task
{
    cothread_t context;

    void *memory;

#ifndef OR_HOST_TEST
    off_t physical;
#endif

    void (*start)(void);

    uint32_t mxcsr;
    uint16_t x87_control;

    struct host_task *next;
};

static host_task main_task;
static host_task *current_task;
static host_task *allocated_tasks;

static int allocation_error;

static void save_fp(host_task *task)
{
    __asm__ volatile(
        "stmxcsr %0"
        : "=m"(task->mxcsr)
        :
        : "memory"
    );

    __asm__ volatile(
        "fnstcw %0"
        : "=m"(task->x87_control)
        :
        : "memory"
    );
}

static void restore_fp(const host_task *task)
{
    __asm__ volatile(
        "ldmxcsr %0"
        :
        : "m"(task->mxcsr)
        : "memory"
    );

    __asm__ volatile(
        "fldcw %0"
        :
        : "m"(task->x87_control)
        : "memory"
    );
}

static int allocate_stack(host_task *task)
{
#ifdef OR_HOST_TEST

    task->memory = aligned_alloc(
        TASK_ALIGNMENT,
        TASK_STACK_BYTES
    );

    if (!task->memory)
    {
        allocation_error = 92;
        return 0;
    }

#else

    if (sceKernelAllocateMainDirectMemory(
            TASK_STACK_BYTES,
            TASK_ALIGNMENT,
            12,
            &task->physical) != 0)
    {
        allocation_error = 92;
        return 0;
    }

    if (sceKernelMapDirectMemory(
            &task->memory,
            TASK_STACK_BYTES,
            0x33,
            0,
            task->physical,
            TASK_ALIGNMENT) != 0 ||
        !task->memory)
    {
        sceKernelReleaseDirectMemory(
            task->physical,
            TASK_STACK_BYTES);

        task->memory = NULL;

        allocation_error = 93;
        return 0;
    }

#endif

    /*
     * Direct memory is not guaranteed to be zeroed.
     * Initialize the complete allocation before use.
     */

    memset(task->memory, 0, TASK_STACK_BYTES);

    return 1;
}

static void release_stack(host_task *task)
{
    if (!task->memory)
        return;

#ifdef OR_HOST_TEST

    free(task->memory);

#else

    munmap(task->memory, TASK_STACK_BYTES);

    sceKernelReleaseDirectMemory(
        task->physical,
        TASK_STACK_BYTES);

#endif

    task->memory = NULL;
}

static void task_entry(void)
{
    host_task *task = current_task;

    if (!task || !task->start)
        abort();

    task->start();

    /* OutRunners task entrypoints must not return. */
    abort();
}

host_task *host_task_main(void)
{
    if (!current_task)
    {
        main_task.context = co_active();
        current_task = &main_task;

        save_fp(&main_task);
    }

    return &main_task;
}

host_task *host_task_current(void)
{
    if (!current_task)
        host_task_main();

    return current_task;
}

host_task *host_task_create(void (*start)(void))
{
    host_task_main();

    allocation_error = 0;

    if (!start)
    {
        allocation_error = 91;
        return NULL;
    }

    host_task *task = calloc(1, sizeof(*task));

    if (!task)
    {
        allocation_error = 91;
        return NULL;
    }

    task->start = start;

    if (!allocate_stack(task))
    {
        free(task);
        return NULL;
    }

    save_fp(task);

    task->context = co_derive(
        task->memory,
        TASK_STACK_BYTES,
        task_entry
    );

    if (!task->context)
    {
        release_stack(task);
        free(task);

        allocation_error = 94;
        return NULL;
    }

    task->next = allocated_tasks;
    allocated_tasks = task;

    return task;
}

void host_task_switch(host_task *to)
{
    host_task *from = host_task_current();

    if (!to || !to->context)
        abort();

    if (to == from)
        return;

    save_fp(from);

    current_task = to;

    restore_fp(to);

    co_switch(to->context);

    /*
     * Execution resumes here when another task
     * switches back to this task.
     */

    restore_fp(from);

    if (current_task != from ||
        co_active() != from->context)
    {
        abort();
    }
}

/*
 * Diagnostic extensions.
 * Not part of the upstream OutRunners API.
 */

int orunners_task_allocation_error(void)
{
    return allocation_error;
}

int orunners_task_identity_valid(void)
{
    return current_task &&
        co_active() == current_task->context;
}

int orunners_task_stack_contains(
    host_task *task,
    const volatile void *address)
{
    uintptr_t base = (uintptr_t)task->memory;
    uintptr_t value = (uintptr_t)address;

    return task->memory &&
        value >= base &&
        value - base < TASK_STACK_BYTES;
}

void orunners_tasks_release_all(void)
{
    if (current_task != &main_task)
        abort();

    /*
     * Only call after all child tasks are permanently
     * suspended and will never be resumed.
     *
     * Do not call co_delete() on direct-memory stacks.
     */

    while (allocated_tasks)
    {
        host_task *task = allocated_tasks;

        allocated_tasks = task->next;

        release_stack(task);
        free(task);
    }
}
