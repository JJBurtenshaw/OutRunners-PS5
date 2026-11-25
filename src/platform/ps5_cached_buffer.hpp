#pragma once

#ifdef PS5_NATIVE_CACHED_BUFFER

#include <cstddef>
#include <sys/types.h>
#include <cstdio>

extern "C" {
    int sceKernelAllocateMainDirectMemory(
        size_t length, size_t alignment,
        int memory_type, off_t* physical);

    int sceKernelMapDirectMemory(
        void** address, size_t length,
        int protection, int flags,
        off_t physical, size_t alignment);

    int sceKernelReleaseDirectMemory(
        off_t physical, size_t length);
}

namespace ps5_cached {

inline void* cached_buffer(size_t bytes)
{
    const size_t alignment = 0x20000;

    const size_t length =
        (bytes + alignment - 1) &
        ~(alignment - 1);

    off_t physical = 0;
    void* address = nullptr;

    int result = sceKernelAllocateMainDirectMemory(
        length, alignment, 12, &physical);

    if (result != 0)
        return nullptr;

    result = sceKernelMapDirectMemory(
        &address, length, 0x33, 0,
        physical, alignment);

    if (result != 0 || !address)
    {
        sceKernelReleaseDirectMemory(physical, length);
        return nullptr;
    }

    return address;
}

} // namespace ps5_cached

#endif
