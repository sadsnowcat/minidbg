#ifndef MINIDBG_VMMAP_HPP
#define MINIDBG_VMMAP_HPP
#include <sys/types.h>
#include <cstdint>

namespace minidbg {
    void print_vmmap(pid_t pid, uint64_t pc);

} // namespace minidbg

#endif
