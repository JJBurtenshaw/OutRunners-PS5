#ifndef OR_PS5_INSTALL_HEAP_H
#define OR_PS5_INSTALL_HEAP_H
#include <stddef.h>
int or_install_heap_begin(void);
void *or_install_malloc(size_t n);
void *or_install_calloc(size_t n, size_t size);
void *or_install_realloc(void *p, size_t n);
void or_install_free(void *p);
#endif

#ifdef OR_INSTALL_HEAP_IMPLEMENTATION
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
extern int sceKernelAllocateMainDirectMemory(size_t,size_t,int,off_t *);
extern int sceKernelMapDirectMemory(void **,size_t,int,int,off_t,size_t);
extern int sceKernelReleaseDirectMemory(off_t,size_t);
#define HEAP_BYTES ((size_t)256 * 1024 * 1024)
#define HEAP_ALIGNMENT ((size_t)0x20000)
typedef union or_block or_block;
union or_block {
    max_align_t align;
    struct { size_t size; or_block *prev, *next; unsigned used; } b;
};
static void *or_heap_base;
static or_block *or_heap_head;
/* Installation is synchronous. Retain the arena until process exit because
 * upstream global asset pointers refer to its contents. */
int or_install_heap_begin(void) {
    if (or_heap_base) return 1;
    off_t physical = 0;
    void *base = NULL;
    int rc = sceKernelAllocateMainDirectMemory(HEAP_BYTES, HEAP_ALIGNMENT, 12, &physical);
    if (rc != 0) { printf("Installer heap allocation failed: %08X\n", (unsigned)rc); return 0; }
    rc = sceKernelMapDirectMemory(&base, HEAP_BYTES, 0x33, 0, physical, HEAP_ALIGNMENT);
    if (rc != 0 || !base) {
        sceKernelReleaseDirectMemory(physical, HEAP_BYTES);
        printf("Installer heap mapping failed: %08X\n", (unsigned)rc); return 0;
    }
    or_heap_base = base;
    or_heap_head = base;
    or_heap_head->b.size = HEAP_BYTES - sizeof(or_block);
    or_heap_head->b.prev = or_heap_head->b.next = NULL;
    or_heap_head->b.used = 0;
    printf("Installer private heap ready: 256 MiB\n");
    return 1;
}
static int or_owns(void *p) {
    uintptr_t a = (uintptr_t)p, base = (uintptr_t)or_heap_base;
    return or_heap_base && a >= base + sizeof(or_block) && a - base < HEAP_BYTES;
}
static void or_split(or_block *b, size_t n) {
    if (b->b.size - n < sizeof(or_block) + _Alignof(max_align_t)) return;
    or_block *tail = (or_block *)((unsigned char *)(b+1) + n);
    tail->b.size = b->b.size - n - sizeof(or_block);
    tail->b.used = 0; tail->b.prev = b; tail->b.next = b->b.next;
    if (tail->b.next) tail->b.next->b.prev = tail;
    b->b.next = tail; b->b.size = n;
}
static void or_merge_next(or_block *b) {
    or_block *n = b->b.next;
    if (!n || n->b.used) return;
    b->b.size += sizeof(or_block) + n->b.size;
    b->b.next = n->b.next;
    if (b->b.next) b->b.next->b.prev = b;
}
void *or_install_malloc(size_t n) {
    if (!or_heap_base) return malloc(n);
    const size_t a = _Alignof(max_align_t);
    if (!n) n = 1;
    if (n > SIZE_MAX - (a-1)) { errno = ENOMEM; return NULL; }
    n = (n + a-1) & ~(a-1);
    for (or_block *b = or_heap_head; b; b = b->b.next) {
        if (!b->b.used && b->b.size >= n) {
            or_split(b, n); b->b.used = 1; return b+1;
        }
    }
    printf("Installer private heap exhausted: %zu bytes\n", n);
    errno = ENOMEM; return NULL;
}
void or_install_free(void *p) {
    if (!p) return;
    if (!or_owns(p)) { free(p); return; }
    or_block *b = (or_block *)p - 1;
    b->b.used = 0;
    or_merge_next(b);
    if (b->b.prev && !b->b.prev->b.used) or_merge_next(b->b.prev);
}
void *or_install_calloc(size_t n, size_t size) {
    if (!or_heap_base) return calloc(n, size);
    if (size && n > SIZE_MAX / size) { errno = ENOMEM; return NULL; }
    size_t bytes = n * size;
    void *p = or_install_malloc(bytes);
    if (p) memset(p, 0, bytes);
    return p;
}
void *or_install_realloc(void *p, size_t n) {
    if (!p) return or_install_malloc(n);
    if (!or_owns(p)) return realloc(p, n);
    if (!n) { or_install_free(p); return NULL; }
    or_block *b = (or_block *)p - 1;
    if (n <= b->b.size) return p;
    void *q = or_install_malloc(n);
    if (!q) return NULL;
    memcpy(q, p, b->b.size);
    or_install_free(p);
    return q;
}
#endif
