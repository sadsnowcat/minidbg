#include "utils.hpp"

#include <sstream>

namespace minidbg {
    std::vector<std::string> split(const std::string &s, char delimiter) {
        std::vector<std::string> out{};
        std::stringstream ss{s};
        std::string item;

        while (std::getline(ss, item, delimiter)) {
            out.push_back(item);
        }

        return out;
    }

    bool is_prefix(const std::string &s, const std::string &of) {
        if (s.size() > of.size())
            return false;
        return std::equal(s.begin(), s.end(), of.begin());
    }
    
    bool is_suffix(const std::string& s, const std::string& of) {
        if (s.size() > of.size()) return false;
        auto diff = of.size() - s.size();
        return std::equal(s.begin(), s.end(), of.begin() + diff);
    }
} // namespace minidbg
