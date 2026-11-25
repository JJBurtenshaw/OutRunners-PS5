#define LIBCO_C
#include "libco.h"
#include "settings.h"

#ifdef __cplusplus
extern "C" {
#endif

static thread_local long long co_active_buffer[64];
static thread_local cothread_t co_active_handle = 0;
static void (*co_swap)(cothread_t, cothread_t) = 0;


/* LIBCO_PS5_STATIC_TEXT_V1
 *
 * Original System V AMD64 instructions.
 * Only executable code placement is adapted.
 */

extern void co_swap_function(cothread_t, cothread_t);

__asm__(
    ".pushsection .text.libco_swap,\"ax\",@progbits\n"
    ".p2align 4\n"
    ".globl co_swap_function\n"
    ".hidden co_swap_function\n"
    ".type co_swap_function,@function\n"
    "co_swap_function:\n"
    ".byte 0x48, 0x89, 0x26, 0x48, 0x8b, 0x27, 0x58, 0x48\n"
    ".byte 0x89, 0x6e, 0x08, 0x48, 0x89, 0x5e, 0x10, 0x4c\n"
    ".byte 0x89, 0x66, 0x18, 0x4c, 0x89, 0x6e, 0x20, 0x4c\n"
    ".byte 0x89, 0x76, 0x28, 0x4c, 0x89, 0x7e, 0x30, 0x48\n"
    ".byte 0x8b, 0x6f, 0x08, 0x48, 0x8b, 0x5f, 0x10, 0x4c\n"
    ".byte 0x8b, 0x67, 0x18, 0x4c, 0x8b, 0x6f, 0x20, 0x4c\n"
    ".byte 0x8b, 0x77, 0x28, 0x4c, 0x8b, 0x7f, 0x30, 0xff\n"
    ".byte 0xe0\n"
    ".size co_swap_function, .-co_swap_function\n"
    ".popsection\n"
);

#include "valgrind.h"

static void co_init(void)
{
    /* Code is already executable. */
}


static void crash(void) {
  LIBCO_ASSERT(0);  /* called only if cothread_t entrypoint returns */
}

cothread_t co_active(void) {
  if(!co_active_handle) co_active_handle = &co_active_buffer;
  return co_active_handle;
}

cothread_t co_derive(void* memory, unsigned int size, void (*entrypoint)(void)) {
  cothread_t handle;
  if(!co_swap) {
    co_init();
    co_swap = co_swap_function;
  }
  if(!co_active_handle) co_active_handle = &co_active_buffer;

  #if defined(__VALGRIND_MAJOR__)
    VALGRIND_STACK_REGISTER(memory, memory + size);
  #endif

  if((handle = (cothread_t)memory)) {
    unsigned long long stack_top = (unsigned long long)handle + size;
    long long *p;
    stack_top -= 32;
    stack_top &= ~((unsigned long long) 15);
    p = (long long*)(stack_top);             /* seek to top of stack */
    *--p = (long long)crash;                 /* crash if entrypoint returns */
    *--p = (long long)entrypoint;            /* start of function */
    *(long long*)handle = (long long)p;      /* stack pointer */
  }

  return handle;
}

cothread_t co_create(unsigned int size, void (*entrypoint)(void)) {
  void* memory = LIBCO_MALLOC(size);
  if(!memory) return (cothread_t)0;
  return co_derive(memory, size, entrypoint);
}

void co_delete(cothread_t handle) {
  LIBCO_FREE(handle);
}

void co_switch(cothread_t handle) {
  register cothread_t co_previous_handle = co_active_handle;
  co_swap(co_active_handle = handle, co_previous_handle);
}

int co_serializable(void) {
  return 1;
}

#ifdef __cplusplus
}
#endif
