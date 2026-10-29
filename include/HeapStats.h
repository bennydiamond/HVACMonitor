#pragma once

#include <esp_heap_caps.h>
#include <stdint.h>

// Memory figures reported everywhere (screen, Home Assistant, web, logs). They cover
// the default heap, the RAM ordinary allocations (malloc/new) come from.
// ESP.getFreeHeap() would also count the IRAM heap, which only allows 32-bit
// accesses and which those allocations can't use; freeIram() reports it separately.
namespace HeapStats {

inline uint32_t freeHeap() { return heap_caps_get_free_size(MALLOC_CAP_DEFAULT); }

inline uint32_t minFreeHeap() { return heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT); }

inline uint32_t largestFreeBlock() { return heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT); }

// Free internal RAM reachable only with 32-bit accesses (IRAM heap)
inline uint32_t freeIram() {
    size_t internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t byteAddressable = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    return internal > byteAddressable ? internal - byteAddressable : 0;
}

} // namespace HeapStats
