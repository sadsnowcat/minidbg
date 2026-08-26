#ifndef MINIDBG_REGISTERS_HPP
#define MINIDBG_REGISTERS_HPP

#include <array>
#include <sys/types.h>
#include <cstddef>
#include <string>
#include <cstdint>

namespace minidbg {
    enum class reg {
        rax,
        rbx,
        rcx,
        rdx,
        rdi,
        rsi,
        rbp,
        rsp,
        r8,
        r9,
        r10,
        r11,
        r12,
        r13,
        r14,
        r15,
        rip,
        rflags,
        cs,
        orig_rax,
        fs_base,
        gs_base,
        fs,
        gs,
        ss,
        ds,
        es
    };

    constexpr std::size_t n_registers = 27;

    struct reg_descriptor {
        reg r;
        int dwarf_r;
        std::string name;
    };

    extern const std::array<reg_descriptor, n_registers> g_register_descriptors;

    uint64_t get_register_value(pid_t pid, reg r);
    void set_register_value(pid_t pid, reg r, uint64_t value);
    uint64_t get_register_value_from_dwarf_register(pid_t pid, unsigned regnum);
    std::string get_register_name(reg r);
    reg get_register_from_name(const std::string &name);
} // namespace minidbg

#endif