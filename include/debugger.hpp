#ifndef MINIDBG_DEBUGGER_HPP
#define MINIDBG_DEBUGGER_HPP

#include "breakpoint.hpp"        // 项目内部头

#include <elf++.hh>
#include <dwarf++.hh>

#include <fcntl.h>
#include <sys/types.h>
#include <csignal>
#include <iostream> 

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

namespace minidbg {
    class debugger {
    public:
        debugger(std::string prog_name, pid_t pid) : m_prog_name{std::move(prog_name)}, m_pid{pid} {
            auto fd = open(m_prog_name.c_str(), O_RDONLY);

            m_elf = elf::elf{elf::create_mmap_loader(fd)};
            // DWARF 解析可能因调试信息版本过新(libelfin 仅支持 DWARF 4 及以下)而失败,
            // 用容错包裹: 失败仅禁用源码视图, 不影响寄存器/内存/断点等地址级调试。
            try {
                m_dwarf = dwarf::dwarf{dwarf::elf::create_loader(m_elf)};
                m_dwarf_ok = true;
            } catch (const std::exception &e) {
                std::cerr << "warning: Failed to parse DWARF debug information. (" << e.what()
                          << "), Source view is disabled; address-level debugging still works normally.\n"
                          << "          Hint: Recompile the debugged program with -gdwarf-4 to restore source view.\n";
                m_dwarf_ok = false;
            }
        }

        void run();
        void handle_command(const std::string &line);
        void continue_execution();
        void set_breakpoint_at_address(std::uintptr_t addr);

        void step_over_breakpoint();

        // Registers access
        void dump_registers();

        // Memory access
        uint64_t read_memory(uint64_t address);
        void write_memory(uint64_t address, uint64_t value);

        // PC
        uint64_t get_pc();
        void set_pc(uint64_t pc);

        void wait_for_signal();

        // 
        dwarf::die get_function_from_pc(uint64_t pc);

        dwarf::line_table::iterator get_line_entry_from_pc(uint64_t pc);

        void initialise_load_address();

        uint64_t offset_load_address(uint64_t addr);

        void print_source(const std::string& file_name,unsigned line,unsigned n_lines_context=2);

        siginfo_t get_signal_info();

        void handle_sigtrap(siginfo_t info);

    private:
        std::string m_prog_name;
        pid_t m_pid;

        std::unordered_map<std::uintptr_t, breakpoint> m_breakpoints;

        dwarf::dwarf m_dwarf;
        elf::elf m_elf;
        bool m_dwarf_ok{false};  // Whether DWARF was parsed successfully.

        uint64_t m_load_address;
    };
} // namespace minidbg

#endif