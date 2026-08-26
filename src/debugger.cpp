#include "debugger.hpp"

#include <linenoise.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>
#include <iomanip>

#include "registers.hpp"
#include "utils.hpp"

namespace minidbg {
    void debugger::run() {
        int wait_status;
        auto options = 0;
        waitpid(m_pid, &wait_status, options);

        char *line = nullptr;
        while ((line = linenoise("minidbg> ")) != nullptr) {
            handle_command(line);
            linenoiseHistoryAdd(line);
            linenoiseFree(line);
        }
    }

    void debugger::handle_command(const std::string &line) {
        auto args = split(line, ' ');
        auto command = args[0];

        if (is_prefix(command, "continue")) {
            continue_execution();
        } else if (is_prefix(command, "break")) {
            if (args.size() > 1) {
                set_breakpoint_at_address(std::stol(args[1], nullptr, 0));
            } else {
                std::cerr << "usage: break <address>\n";
            }
        } else if (is_prefix(command, "register")) {
            if (args.size() < 2) {
                std::cerr << "usage: register dump | read <name> | write <name> <value>\n";
            } else if (is_prefix(args[1], "dump")) {
                dump_registers();
            } else if (is_prefix(args[1], "read")) {
                if (args.size() < 3) {
                    std::cerr << "usage: register read <name>\n";
                } else {
                    std::cout << std::hex
                              << get_register_value(m_pid, get_register_from_name(args[2]))
                              << std::endl;
                }
            } else if (is_prefix(args[1], "write")) {
                if (args.size() < 4) {
                    std::cerr << "usage: register write <name> <value>\n";
                } else {
                    set_register_value(m_pid, get_register_from_name(args[2]),
                                       std::stol(args[3], nullptr, 0));
                }
            } else {
                std::cerr << "Unknown register subcommand: " << args[1] << "\n";
            }
        } else if (is_prefix(command, "memory")) {
            if (args.size() < 3) {
                std::cerr << "usage: memory read <address> | memory write <address> <value>\n";
            } else if (is_prefix(args[1], "read")) {
                std::cout << std::hex << read_memory(std::stol(args[2], nullptr, 0)) << std::endl;
            } else if (is_prefix(args[1], "write")) {
                if (args.size() < 4) {
                    std::cerr << "usage: memory write <address> <value>\n";
                } else {
                    write_memory(std::stol(args[2], nullptr, 0), std::stol(args[3], nullptr, 0));
                }
            } else {
                std::cerr << "Unknown memory subcommand: " << args[1] << "\n";
            }
        } else {
            std::cerr << "Unknown command\n";
        }
    }

    void debugger::continue_execution() {
        step_over_breakpoint();
        ptrace(PTRACE_CONT, m_pid, nullptr, nullptr);
        wait_for_signal();
    }

    void debugger::set_breakpoint_at_address(std::intptr_t addr) {
        std::cout << "Set breakpoint at address 0x" << std::hex << addr << std::endl;
        breakpoint bp{m_pid, addr};
        bp.enable();
        m_breakpoints[addr] = bp;
    }

    void debugger::dump_registers() {
        for (const auto &rd : g_register_descriptors) {
            std::cout << rd.name << "0x" << std::setfill('0') << std::setw(16) << std::hex
                      << get_register_value(m_pid, rd.r) << std::endl;
        }
    }

    uint64_t debugger::read_memory(uint64_t address) {
        return ptrace(PTRACE_PEEKDATA, m_pid, address, nullptr);
    }

    void debugger::write_memory(uint64_t address, uint64_t value) {
        ptrace(PTRACE_POKEDATA, m_pid, address, value);
    }

    uint64_t debugger::get_pc() {
        return get_register_value(m_pid, reg::rip);
    }

    void debugger::set_pc(uint64_t pc) {
        set_register_value(m_pid, reg::rip, pc);
    }

    void debugger::step_over_breakpoint() {
        auto possible_breakpoint_location = get_pc() - 1;

        if (m_breakpoints.count(possible_breakpoint_location)) {
            auto &bp = m_breakpoints[possible_breakpoint_location];

            if (bp.is_enabled()) {
                auto previous_instruction_address = possible_breakpoint_location;
                set_pc(previous_instruction_address);
                bp.disable();
                ptrace(PTRACE_SINGLESTEP, m_pid, nullptr, nullptr);
                wait_for_signal();
                bp.enable();
            }
        }
    }

    void debugger::wait_for_signal() {
        int wait_status;
        auto options = 0;
        waitpid(m_pid, &wait_status, options);
    }

} // namespace minidbg