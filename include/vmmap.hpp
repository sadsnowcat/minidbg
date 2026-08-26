#ifndef MINIDBG_VMMAP_HPP
#define MINIDBG_VMMAP_HPP
#include <cstdint>
#include <sys/types.h>

namespace minidbg {
    void print_vmmap(pid_t pid, uint64_t pc);

} // namespace minidbg

#endif
