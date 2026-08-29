#ifndef MINIDBG_VARIABLES_HPP
#define MINIDBG_VARIABLES_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace dwarf {
class die;
}

namespace minidbg {
    class debugger;

    struct variable_info {
        std::string name;
        int64_t cfa_offset;
        uint64_t address;
    };


    std::vector<variable_info> collect_local_variables(const dwarf::die &func,uint64_t frame_base);

    void print_variables(debugger &dbg);
} // namespace minidbg

#endif
