#ifndef ORUNNERS_PS5_EXTRACT_MEMORY_H
#define ORUNNERS_PS5_EXTRACT_MEMORY_H

/*
 * PS5 direct-memory allocator for locally owned
 * temporary ROM extraction buffers.
 *
 * ORUNNERS_PS5_EXTRACT_MEMORY_V1
 *
 * Not a replacement for the global application heap.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

#define OR_DM_ALIGN ((size_t)0x20000u)

typedef union
{
    struct
    {
        off_t physical;
        size_t allocation;
        uint64_t magic;
    } info;

    max_align_t alignment;

} or_dm_header;

#define OR_DM_MAGIC UINT64_C(0x4F52444D454D3031)

static void *or_extract_malloc(size_t requested)
{
    if (requested == 0)
        requested = 1;

    if (requested >
        SIZE_MAX - sizeof(or_dm_header) -
        (OR_DM_ALIGN - 1))
        return NULL;

    size_t length =
        (requested + sizeof(or_dm_header) +
         OR_DM_ALIGN - 1) &
        ~(OR_DM_ALIGN - 1);

    off_t physical = 0;
    void *address = NULL;

    int status =
        sceKernelAllocateMainDirectMemory(
            length,
            OR_DM_ALIGN,
            12,
            &physical);

    if (status != 0)
    {
        printf(
            "ROM DM allocate failed: "
            "%zu bytes rc=%08X\n",
            length,
            (unsigned)status);

        return NULL;
    }

    status = sceKernelMapDirectMemory(
        &address,
        length,
        0x33,
        0,
        physical,
        OR_DM_ALIGN);

    if (status != 0 || !address)
    {
        printf(
            "ROM DM map failed: "
            "%zu bytes rc=%08X\n",
            length,
            (unsigned)status);

        sceKernelReleaseDirectMemory(
            physical, length);

        return NULL;
    }

    or_dm_header *header = address;

    header->info.physical = physical;
    header->info.allocation = length;
    header->info.magic = OR_DM_MAGIC;

    printf(
        "ROM DM allocated: %zu bytes "
        "(mapped %zu)\n",
        requested, length);

    return (void *)(header + 1);
}

static void or_extract_free(void *pointer)
{
    if (!pointer)
        return;

    or_dm_header *header =
        ((or_dm_header *)pointer) - 1;

    if (header->info.magic != OR_DM_MAGIC)
    {
        printf("ROM DM invalid allocation header\n");
        return;
    }

    /*
     * Copy allocation metadata before unmapping.
     */

    const off_t physical =
        header->info.physical;

    const size_t length =
        header->info.allocation;

    if (munmap(header, length) != 0)
    {
        printf(
            "ROM DM unmap failed: %zu bytes\n",
            length);

        return;
    }

    int status =
        sceKernelReleaseDirectMemory(
            physical, length);

    if (status != 0)
    {
        printf(
            "ROM DM release failed: "
            "%zu bytes rc=%08X\n",
            length, (unsigned)status);
    }
    else
    {
        printf(
            "ROM DM released: %zu bytes\n",
            length);
    }
}

#endif
