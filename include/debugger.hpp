#ifndef MINIDBG_DEBUGGER_HPP
#define MINIDBG_DEBUGGER_HPP

#include "breakpoint.hpp"
#include "symbol.hpp"

#include <dwarf++.hh>
#include <elf++.hh>

#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <sys/types.h>

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

            try {
                m_dwarf = dwarf::dwarf{dwarf::elf::create_loader(m_elf)};
                m_dwarf_ok = true;
            } catch (const std::exception &e) {
                std::cerr
                    << "warning: Failed to parse DWARF debug information. (" << e.what()
                    << "), Source view is disabled; address-level debugging still works normally.\n"
                    << "          Hint: Recompile the debugged program with -gdwarf-4 to restore "
                       "source view.\n";
                m_dwarf_ok = false;
            }
        }

        void run();
        void handle_command(const std::string &line);
        void continue_execution();
        void set_breakpoint_at_address(std::uintptr_t addr, bool silent = false);
 
        void set_breakpoint_at_function(const std::string& name);
        void set_breakpoint_at_source_line(const std::string& file,unsigned line);

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

        void print_source(const std::string &file_name, unsigned line,
                          unsigned n_lines_context = 2);

        siginfo_t get_signal_info();

        void handle_sigtrap(siginfo_t info);

        void single_step_instruction();
        void single_step_instruction_with_breakpoint_check();
        void step_out();
        void step_in();
        void step_over();

        void remove_breakpoint(std::uintptr_t addr);

        uint64_t get_offset_pc();

        uint64_t offset_dwarf_address(uint64_t addr);

        std::vector<symbol> lookup_symbol(const std::string& name);

    private:
        std::string m_prog_name;
        pid_t m_pid;

        std::unordered_map<std::uintptr_t, breakpoint> m_breakpoints;

        dwarf::dwarf m_dwarf;
        elf::elf m_elf;
        bool m_dwarf_ok{false}; // Whether DWARF was parsed successfully.

        uint64_t m_load_address;
    };
} // namespace minidbg

#endif