#ifndef MINIDBG_DEBUGGER_HPP
#define MINIDBG_DEBUGGER_HPP

#include <iostream>
#include <linenoise.h>
#include <sstream>
#include <string>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unordered_map>
#include <utility>

#include "breakpoint.hpp"
#include "dwarf/dwarf++.hh"
#include "elf/elf++.hh"
#include "registers.hpp"

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

        void step_over_breakpoint();

        // Registers access
        void dump_registers();
        
        // Memory access
        uint64_t read_memory(uint64_t address);
        void write_memory(uint64_t address,uint64_t value);

        // PC 
        uint64_t get_pc();
        void set_pc(uint64_t pc);

        void wait_for_signal();


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
        } else if (is_prefix(command, "register")) {
            if (is_prefix(args[1], "dump")) {
                dump_registers();
            } else if (is_prefix(args[1], "read")) {
                std::cout << get_register_value(m_pid, get_register_from_name(args[2]))
                          << std::endl;
            } else if (is_prefix(args[1], "write")) {
                std::string val{args[3], 2};
                set_register_value(m_pid, get_register_from_name(args[2]), std::stol(val, 0, 16));
            }
        } else if (is_prefix(command,"memory")){
            std::string addr {args[2],2};

            if(is_prefix(args[1],"read")){
                std::cout<<std::hex<<read_memory(std::stol(addr,0,16))<<std::endl;
            }
            if(is_prefix(args[1],"write")){
                std::string val {args[3],2};
                write_memory(std::stol(addr,0,16),std::stol(val,0,16));
            }
        } 
        else {
            std::cerr << "Unknown command\n";
        }
    }

    void debugger::continue_execution() {
        step_over_breakpoint();
        ptrace(PTRACE_CONT,m_pid,nullptr,nullptr);
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

    uint64_t debugger::read_memory(uint64_t address){
        return ptrace(PTRACE_PEEKDATA,m_pid,address,nullptr);
    }

    void debugger::write_memory(uint64_t address,uint64_t value){
        ptrace(PTRACE_POKEDATA,m_pid,address,value);
    }

    uint64_t debugger::get_pc(){
        return get_register_value(m_pid,reg::rip);
    }

    void debugger::set_pc(uint64_t pc){
        set_register_value(m_pid,reg::rip,pc);
    }

    void debugger::step_over_breakpoint(){
        auto possible_breakpoint_location = get_pc() -1;

        if(m_breakpoints.count(possible_breakpoint_location)){
            auto& bp = m_breakpoints[possible_breakpoint_location];

            if(bp.is_enabled()){
                auto previous_instruction_address = possible_breakpoint_location;
                set_pc(previous_instruction_address);
                bp.disable();
                ptrace(PTRACE_SINGLESTEP,m_pid,nullptr,nullptr);
                wait_for_signal();
                bp.enable();
            }
        }
    }

    void debugger::wait_for_signal(){
        int wait_status;
        auto options =0;
        waitpid(m_pid,&wait_status,options);
    }


} // namespace minidbg

#endif