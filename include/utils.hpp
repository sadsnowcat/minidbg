#ifndef MINIDBG_UTILS_HPP
#define MINIDBG_UTILS_HPP

#include <string>
#include <vector>

namespace minidbg {
    // 分割字符串
    std::vector<std::string> split(const std::string &s, char delimiter);
    bool is_prefix(const std::string &s, const std::string &of);
} // namespace minidbg

#endif