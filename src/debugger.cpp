#include "debugger.hpp"

#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <linenoise.h>
#include <stdexcept>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "registers.hpp"
#include "utils.hpp"
#include "vmmap.hpp"

namespace minidbg {
    void debugger::run() {
        wait_for_signal();
        initialise_load_address();

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
            if (args.size() < 2) {
                std::cerr << "usage: break <address> | <file>:<line> | <function>\n";
            } else if (args[1].rfind("0x", 0) == 0) {              // 0x 开头 → 地址断点
                set_breakpoint_at_address(std::stol(args[1], nullptr, 0));
            } else if (args[1].find(':') != std::string::npos) {   // 有冒号 → 行号断点
                auto file_and_line = split(args[1], ':');
                set_breakpoint_at_source_line(file_and_line[0], std::stoi(file_and_line[1]));
            } else {                                                // 其余 → 函数名断点
                set_breakpoint_at_function(args[1]);
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
        } else if (is_prefix(command, "vmmap")) {
            print_vmmap(m_pid, get_pc());
        } else if (is_prefix(command, "step")) {
            step_in();
        } else if (is_prefix(command, "stepi")) {
            single_step_instruction_with_breakpoint_check();
            try {
                auto line_entry = get_line_entry_from_pc(get_offset_pc());
                print_source(line_entry->file->path, line_entry->line);
            } catch (const std::exception &) {
                // no debug info at this address; nothing to print
            }
        } else if (is_prefix(command, "next")) {
            step_over();
        } else if (is_prefix(command, "finish")) {
            step_out();
        }else if(is_prefix(command, "symbol")) {
            auto syms = lookup_symbol(args[1]);
            for (auto&& s : syms) {
                std::cout << s.name << ' ' << to_string(s.type) << " 0x" << std::hex << s.addr << std::endl;
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

    void debugger::set_breakpoint_at_address(std::uintptr_t addr, bool silent) {
        if (!silent) {
            std::cout << "Set breakpoint at address 0x" << std::hex << addr << std::endl;
        }
        breakpoint bp{m_pid, addr};
        bp.enable();
        m_breakpoints[addr] = bp;
    }

    void debugger::set_breakpoint_at_function(const std::string& name){
        for (const auto& cu : m_dwarf.compilation_units()){
            for (const auto& die : cu.root()){
                if(die.has(dwarf::DW_AT::name)&&at_name(die) == name){
                    auto low_pc = at_low_pc(die);
                    auto entry = get_line_entry_from_pc(low_pc);
                    ++entry;
                    set_breakpoint_at_address(offset_dwarf_address(entry->address));
                }
            }
        }
    }

    void debugger::set_breakpoint_at_source_line(const std::string& file,unsigned line){
        for (const auto&cu:m_dwarf.compilation_units()){
            if (is_suffix(file, at_name(cu.root()))) {
                const auto& lt = cu.get_line_table();

                for(const auto& entry:lt){
                    if(entry.is_stmt && entry.line == line){
                        set_breakpoint_at_address(offset_dwarf_address(entry.address));
                    }
                }
            }
        }
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
        // After a breakpoint hit, handle_sigtrap has already rewound the PC to the
        // breakpoint address, so look it up directly (no -1 adjustment).
        auto possible_breakpoint_location = get_pc();

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

        auto siginfo = get_signal_info();

        switch (siginfo.si_signo) {
        case SIGTRAP:
            handle_sigtrap(siginfo);
            break;
        case SIGSEGV:
            std::cout << "Yay, segfault. Reason: " << siginfo.si_code << std::endl;
            break;
        default:
            std::cout << "Got signal " << strsignal(siginfo.si_signo) << std::endl;
            break;
        }
    }

    dwarf::die debugger::get_function_from_pc(uint64_t pc) {
        for (auto &cu : m_dwarf.compilation_units()) {
            if (die_pc_range(cu.root()).contains(pc)) {
                for (const auto &die : cu.root()) {
                    if (die.tag == dwarf::DW_TAG::subprogram) {
                        if (die_pc_range(die).contains(pc)) {
                            return die;
                        }
                    }
                }
            }
        }

        throw std::out_of_range{"Cannot find function"};
    }

    dwarf::line_table::iterator debugger::get_line_entry_from_pc(uint64_t pc) {
        if (!m_dwarf_ok) {
            throw std::runtime_error{"DWARF debug info is unavailable"};
        }
        for (auto &cu : m_dwarf.compilation_units()) {
            if (die_pc_range(cu.root()).contains(pc)) {
                auto &lt = cu.get_line_table();
                auto it = lt.find_address(pc);
                if (it == lt.end()) {
                    throw std::out_of_range{"Cannot find line entry"};
                } else {
                    return it;
                }
            }
        }

        throw std::out_of_range{"Cannot find line entry"};
    }

    void debugger::initialise_load_address() {
        if (m_elf.get_hdr().type == elf::et::dyn) {
            std::ifstream map("/proc/" + std::to_string(m_pid) + "/maps");

            std::string addr;
            std::getline(map, addr, '-');
            m_load_address = std::stol(addr, 0, 16);
        }
    }

    uint64_t debugger::offset_load_address(uint64_t addr) {
        return addr - m_load_address;
    }

    void debugger::print_source(const std::string &file_name, unsigned line,
                                unsigned n_lines_context) {
        std::ifstream file{file_name};

        auto start_line = line <= n_lines_context ? 1 : line - n_lines_context;
        auto end_line =
            line + n_lines_context + (line < n_lines_context ? n_lines_context - line : 0) + 1;

        char c{};
        auto current_line = 1u;

        while (current_line != start_line && file.get(c)) {
            if (c == '\n') {
                ++current_line;
            }
        }

        std::cout << (current_line == line ? ">" : "  ");

        while (current_line <= end_line && file.get(c)) {
            std::cout << c;
            if (c == '\n') {
                ++current_line;
                std::cout << (current_line == line ? "> " : "  ");
            }
        }
        std::cout << std::endl;
    }

    siginfo_t debugger::get_signal_info() {
        siginfo_t info;
        ptrace(PTRACE_GETSIGINFO, m_pid, nullptr, &info);
        return info;
    }

    void debugger::handle_sigtrap(siginfo_t info) {
        switch (info.si_code) {
        case SI_KERNEL:
        case TRAP_BRKPT: {
            set_pc(get_pc() - 1);
            std::cout << "Hit breakpoint at address 0x" << std::hex << get_pc() << std::endl;
            auto offset_pc = offset_load_address(get_pc());

            // Source view fault tolerance: Neither DWARF parsing failures nor missing source files
            // should cause the debugger to crash entirely.
            try {
                auto line_entry = get_line_entry_from_pc(offset_pc);
                if (line_entry->file) {
                    print_source(line_entry->file->path, line_entry->line);
                } else {
                    std::cout << "  (No source file information found for this address.)\n";
                }
            } catch (const std::exception &e) {
                std::cout << "  (Cannot display source code: " << e.what()
                          << " - The source view unavailable at this address. Normal when the PC is in code without debug info, e.g. libc.)\n";
            }
            return;
        }
        case TRAP_TRACE:
            return;
        default:
            if (info.si_code != 0) {
                std::cout << "Unknown SIGTRAP code " << info.si_code << std::endl;
            }
            return;
        }
    }

    void debugger::single_step_instruction() {
        ptrace(PTRACE_SINGLESTEP, m_pid, nullptr, nullptr);
        wait_for_signal();
    }

    void debugger::single_step_instruction_with_breakpoint_check() {
        if (m_breakpoints.count(get_pc())) {
            step_over_breakpoint();
        } else {
            single_step_instruction();
        }
    }

    void debugger::step_out() {
        auto frame_pointer = get_register_value(m_pid, reg::rbp);
        auto return_address = read_memory(frame_pointer + 8);

        bool should_remove_breakpoint = false;
        if (!m_breakpoints.count(return_address)) {
            set_breakpoint_at_address(return_address, true);
            should_remove_breakpoint = true;
        }

        continue_execution();

        if (should_remove_breakpoint) {
            remove_breakpoint(return_address);
        }
    }

    void debugger::step_in() {
        try {
        auto line = get_line_entry_from_pc(get_offset_pc())->line;

        while (get_line_entry_from_pc(get_offset_pc())->line == line) {
            single_step_instruction_with_breakpoint_check();
        }

        auto line_entry = get_line_entry_from_pc(get_offset_pc());
        print_source(line_entry->file->path, line_entry->line);
        } catch (const std::exception &e) {
            std::cout << "  (Cannot step in: " << e.what()
                      << ". The PC may be in code without debug info; use 'c' to continue to a breakpoint.)";
        }
    }

    void debugger::step_over() {
        try {
        //
        auto func = get_function_from_pc(get_offset_pc());
        auto func_entry = at_low_pc(func);
        auto func_end = at_high_pc(func);

        //
        auto line = get_line_entry_from_pc(func_entry);
        auto start_line = get_line_entry_from_pc(get_offset_pc());

        std::vector<std::uintptr_t> to_delete{};

        while (line->address < func_end) {
            auto load_address = offset_dwarf_address(line->address);
            if (line->address != start_line->address && !m_breakpoints.count(load_address)) {
                set_breakpoint_at_address(load_address, true);
                to_delete.push_back(load_address);
            }
            ++line;
        }

        //
        auto frame_pointer = get_register_value(m_pid, reg::rbp);
        auto return_address = read_memory(frame_pointer + 8);
        if (!m_breakpoints.count(return_address)) {
            set_breakpoint_at_address(return_address, true);
            to_delete.push_back(return_address);
        }

        //
        continue_execution();
        for (auto addr : to_delete) {
            remove_breakpoint(addr);
        }
        } catch (const std::exception &e) {
            std::cout << "  (Cannot step over: " << e.what()
                      << ". The PC may be in code without debug info (e.g. the dynamic loader at startup); use 'c' to continue to a breakpoint.)";
        }
    }

    void debugger::remove_breakpoint(std::uintptr_t addr) {
        if (m_breakpoints.at(addr).is_enabled()) {
            m_breakpoints.at(addr).disable();
        }
        m_breakpoints.erase(addr);
    }

    uint64_t debugger::get_offset_pc() {
        return offset_load_address(get_pc());
    }

    uint64_t debugger::offset_dwarf_address(uint64_t addr) {
        return addr + m_load_address;
    }

    std::vector<symbol> debugger::lookup_symbol(const std::string& name){
        std::vector<symbol> syms;

        for(auto &sec : m_elf.sections()){
            if(sec.get_hdr().type != elf::sht::symtab && sec.get_hdr().type != elf::sht::dynsym) continue;

            for (auto sym : sec.as_symtab()){
                if (sym.get_name() == name){
                    auto &d = sym.get_data();
                    syms.push_back(symbol{to_symbol_type(d.type()),sym.get_name(),d.value});
                }
            }
        }
        return syms;
    }


} // namespace minidbg