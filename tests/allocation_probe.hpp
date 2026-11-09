#ifndef JOTJSON_TEST_ALLOCATION_PROBE_HPP
#define JOTJSON_TEST_ALLOCATION_PROBE_HPP

#include <cstddef>

namespace jotjson_test {

struct allocation_snapshot {
    std::size_t allocations;
    std::size_t deallocations;
    // MinGW 链接器包装只能安全统计 malloc 家族，
    // 因此以下生命周期字段保留为 0。
    std::size_t live_allocations;
    std::size_t peak_live_allocations;
    std::size_t allocated_bytes;
    std::size_t live_bytes;
    std::size_t peak_live_bytes;
};

// 重新开始统计。调用之后产生的分配才会被记录。
void reset_allocation_probe();

// 当前构建是否支持分配计数。
bool allocation_probe_available();

// 用一次受观察的分配做自检。
// 返回 true 表示后续快照中的 allocations 有实际意义。
bool probe_allocation_path();

allocation_snapshot allocation_probe_snapshot();

} // namespace jotjson_test

#endif
