#ifndef MINIDBG_DEBUGGER_HPP
#define MINIDBG_DEBUGGER_HPP

#include <iostream>
#include <sstream>
#include <string>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unordered_map>
#include <utility>
#include <linenoise.h>

#include "breakpoint.hpp"
#include "dwarf/dwarf++.hh"
#include "elf/elf++.hh"

namespace minidbg {
    std::vector<std::string> split(const std::string &s, char delimiter);
    bool is_prefix(const std::string &s, const std::string &of);

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

    class debugger {
    public:
        debugger(std::string prog_name, pid_t pid)
            : m_prog_name{std::move(prog_name)}, m_pid{pid} {}

        void run();
        void handle_command(const std::string &line);
        void continue_execution();
        void set_breakpoint_at_address(std::intptr_t addr);

    private:
        std::string m_prog_name;
        pid_t m_pid;

        std::unordered_map<std::intptr_t, breakpoint> m_breakpoints;
    };

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
            std::string addr{args[1], 2};
            set_breakpoint_at_address(std::stol(addr, 0, 16));
        } else {
            std::cerr << "Unknown command\n";
        }
    }

    void debugger::continue_execution() {
        ptrace(PTRACE_CONT, m_pid, nullptr, nullptr);

        int wait_status;
        auto options = 0;
        waitpid(m_pid, &wait_status, options);
    }

    void debugger::set_breakpoint_at_address(std::intptr_t addr) {
        std::cout << "Set breakpoint at address 0x" << std::hex << addr << std::endl;
        breakpoint bp{m_pid, addr};
        bp.enable();
        m_breakpoints[addr] = bp;
    }

} // namespace minidbg

#endif