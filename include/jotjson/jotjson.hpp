#ifndef JOTJSON_JOTJSON_HPP
#define JOTJSON_JOTJSON_HPP

#include <jotjson/version.hpp>
#include <jotjson/parse.hpp>
#include <jotjson/error.hpp>
#include <jotjson/value.hpp>
#include<jotjson/serialize.hpp>

namespace jotjson{
    // 返回静态存储区中的库名，调用方不得释放
    const char* library_name() noexcept;
}

#endif