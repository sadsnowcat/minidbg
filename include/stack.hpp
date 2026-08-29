#ifndef MINIDBG_STACK_HPP
#define MINIDBG_STACK_HPP

#include <cstdint>

namespace minidbg {
    class debugger;

    void print_stack(debugger &dbg);

    void print_telescope(debugger &dbg, uint64_t address, int count);

    void print_frames(debugger &dbg);
} // namespace minidbg

#endif
