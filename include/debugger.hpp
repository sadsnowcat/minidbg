#ifndef MINIDBG_DEBUGGER_HPP
#define MINIDBG_DEBUGGER_HPP

#include <string>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <sys/types.h>

#include "breakpoint.hpp"

namespace minidbg {
    class debugger {
    public:
        debugger(std::string prog_name, pid_t pid)
            : m_prog_name{std::move(prog_name)}, m_pid{pid} {}

        void run();
        void handle_command(const std::string &line);
        void continue_execution();
        void set_breakpoint_at_address(std::intptr_t addr);

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

    private:
        std::string m_prog_name;
        pid_t m_pid;

        std::unordered_map<std::intptr_t, breakpoint> m_breakpoints;
    };
} // namespace minidbg

#endif