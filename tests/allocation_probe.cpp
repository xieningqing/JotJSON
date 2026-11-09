#include "allocation_probe.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

#if defined(__GNUC__) && \
    (defined(__MINGW32__) || defined(__MINGW64__))
#include <malloc.h>
#endif

std::atomic<std::size_t> allocation_count(0);
std::atomic<std::size_t> deallocation_count(0);
std::atomic<std::size_t> allocated_byte_count(0);

#if defined(__GNUC__) && \
    (defined(__MINGW32__) || defined(__MINGW64__))

extern "C" void* __real_malloc(std::size_t size);
extern "C" void __real_free(void* pointer);
extern "C" void* __real_calloc(
    std::size_t count,
    std::size_t size);
extern "C" void* __real_realloc(
    void* pointer,
    std::size_t size);
extern "C" void* __real__aligned_malloc(
    std::size_t size,
    std::size_t alignment);
extern "C" void __real__aligned_free(void* pointer);

extern "C" void* __wrap_malloc(std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    allocated_byte_count.fetch_add(size, std::memory_order_relaxed);
    return __real_malloc(size);
}

extern "C" void __wrap_free(void* pointer) {
    if (pointer != nullptr) {
        deallocation_count.fetch_add(1, std::memory_order_relaxed);
    }

    __real_free(pointer);
}

extern "C" void* __wrap_calloc(
    std::size_t count,
    std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    allocated_byte_count.fetch_add(
        count * size,
        std::memory_order_relaxed);

    return __real_calloc(count, size);
}

extern "C" void* __wrap_realloc(
    void* pointer,
    std::size_t size) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    allocated_byte_count.fetch_add(size, std::memory_order_relaxed);
    return __real_realloc(pointer, size);
}

extern "C" void* __wrap__aligned_malloc(
    std::size_t size,
    std::size_t alignment) {
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    allocated_byte_count.fetch_add(size, std::memory_order_relaxed);
    return __real__aligned_malloc(size, alignment);
}

extern "C" void __wrap__aligned_free(void* pointer) {
    if (pointer != nullptr) {
        deallocation_count.fetch_add(1, std::memory_order_relaxed);
    }

    __real__aligned_free(pointer);
}

#endif

namespace jotjson_test {

void reset_allocation_probe() {
    allocation_count.store(0, std::memory_order_relaxed);
    deallocation_count.store(0, std::memory_order_relaxed);
    allocated_byte_count.store(0, std::memory_order_relaxed);
}

bool allocation_probe_available() {
#if defined(__GNUC__) && \
    (defined(__MINGW32__) || defined(__MINGW64__))
    // MinGW 的 new/delete 可能绕过链接器可包装的 malloc 符号。
    // 只有实际记录到事件时才宣称可用。
    return allocation_count.load(std::memory_order_relaxed) != 0 ||
        deallocation_count.load(std::memory_order_relaxed) != 0;
#else
    return false;
#endif
}

bool probe_allocation_path() {
    reset_allocation_probe();

    // 解析器的节点由 operator new 分配，
    // 因此探针必须能观察 new 路径才有意义。
    // 这里只做一次自检，不改变后续语料测量。
    void* pointer = ::operator new(16);
    ::operator delete(pointer);

    return allocation_probe_available();
}

allocation_snapshot allocation_probe_snapshot() {
    allocation_snapshot result;

    result.allocations =
        allocation_count.load(std::memory_order_relaxed);
    result.deallocations =
        deallocation_count.load(std::memory_order_relaxed);
    result.allocated_bytes =
        allocated_byte_count.load(std::memory_order_relaxed);

    // 链接器包装只能观察 malloc 家族，无法安全跟踪 new/delete。
    // 因此这些字段保持为 0，调用方必须结合 available() 判断。
    result.live_allocations = 0;
    result.peak_live_allocations = 0;
    result.live_bytes = 0;
    result.peak_live_bytes = 0;

    return result;
}

} // namespace jotjson_test
