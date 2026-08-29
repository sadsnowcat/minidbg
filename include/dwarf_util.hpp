// 从 DWARF 表达式字节缓冲区中解码 有符号/无符号 变长整数
// 返回消耗的字节数

#ifndef MINIDBG_DWARF_UTIL_HPP
#define MINIDBG_DWARF_UTIL_HPP

#include <cstddef>
#include <cstdint>

namespace minidbg {
    size_t read_uleb128(const uint8_t *p, unsigned *out);
    size_t read_sleb128(const uint8_t *p, int64_t *out);
} // namespace minidbg

#endif
