#include "stack.hpp"

#include "debugger.hpp"
#include "registers.hpp"
#include "variables.hpp"

#include <dwarf++.hh>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace minidbg {
    namespace {
        constexpr int kMaxFrames = 64;
        constexpr int kDefaultCount = 16;
        constexpr int kMaxTelescopeDepth = 4;

        struct frame_info {
            uint64_t pc = 0;
            uint64_t rbp = 0;
            uint64_t ret = 0;
            dwarf::die func;
            bool symbolized = false;
        };

        std::string frame_name(const frame_info &f) {
            if (!f.symbolized)
                return "<unknown>";
            try {
                return dwarf::at_name(f.func);
            } catch (const std::exception &) {
                return "<unnamed>";
            }
        }

        std::string offset_suffix(int64_t off) {
            if (off == 0)
                return " (cfa)";
            if (off < 0)
                return " (cfa-" + std::to_string(-off) + ")";
            return " (cfa+" + std::to_string(off) + ")";
        }

        std::string hex_str(uint64_t v) {
            std::ostringstream os;
            os << "0x" << std::hex << v;
            return os.str();
        }


        int probe_mapping(pid_t pid, uint64_t addr) {
            if (addr == 0 || addr >= 0x800000000000ULL)
                return 0;
            std::ifstream maps("/proc/" + std::to_string(pid) + "/maps");
            std::string line;
            while (std::getline(maps, line)) {
                uint64_t start = 0, end = 0;
                char perms[8] = {0};
                if (std::sscanf(line.c_str(), "%lx-%lx %7s", &start, &end, perms) != 3)
                    continue;
                if (addr >= start && addr < end)
                    return std::strchr(perms, 'x') ? 2 : 1;
            }
            return 0;
        }

        std::string code_symbol(debugger &dbg, uint64_t addr) {
            try {
                auto func = dbg.get_function_from_pc(dbg.offset_load_address(addr));
                if (func.valid() && func.has(dwarf::DW_AT::name) &&
                    func.has(dwarf::DW_AT::low_pc)) {
                    auto low = func[dwarf::DW_AT::low_pc].as_address();
                    int64_t delta = (int64_t)dbg.offset_load_address(addr) - (int64_t)low;
                    std::string name = dwarf::at_name(func);
                    if (delta > 0)
                        name += "+" + std::to_string(delta);
                    return "(" + name + ")";
                }
            } catch (const std::exception &) {
            }
            return "";
        }

        // Raw bytes at addr as "48 8b 45 f8 ..." (8 bytes; Capstone disassembly would be a nicer follow-up).
        std::string instr_bytes(debugger &dbg, uint64_t addr) {
            uint64_t v = dbg.read_memory(addr);
            std::ostringstream os;
            for (int i = 7; i >= 0; --i)
                os << std::hex << std::setw(2) << std::setfill('0') << ((v >> (8 * i)) & 0xff)
                   << (i ? " " : "");
            return os.str();
        }

        std::string render_slot_value(debugger &dbg, uint64_t value) {
            std::string out = hex_str(value);
            int probe = probe_mapping(dbg.get_pid(), value);
            if (probe == 0)
                return out;
            if (probe == 2) {
                std::string sym = code_symbol(dbg, value);
                out += (sym.empty() ? " " : " " + sym + " ");
                out += "◂— " + instr_bytes(dbg, value);
                return out;
            }
            uint64_t cur = value;
            for (int depth = 0; depth < kMaxTelescopeDepth; ++depth) {
                uint64_t next = dbg.read_memory(cur);
                int p = probe_mapping(dbg.get_pid(), next);
                if (p == 0) {
                    out += " ◂— " + hex_str(next);
                    break;
                }
                out += " —▸ " + hex_str(next);
                if (p == 2) {
                    std::string sym = code_symbol(dbg, next);
                    out += (sym.empty() ? " " : " " + sym + " ");
                    out += "◂— " + instr_bytes(dbg, next);
                    break;
                }
                cur = next;
            }
            return out;
        }
    } // namespace

    void print_stack(debugger &dbg) {
        uint64_t rsp = get_register_value(dbg.get_pid(), reg::rsp);
        print_telescope(dbg, rsp, kDefaultCount);
    }

    void print_telescope(debugger &dbg, uint64_t base, int count) {
        uint64_t rsp = get_register_value(dbg.get_pid(), reg::rsp);
        uint64_t rbp = get_register_value(dbg.get_pid(), reg::rbp);
        for (int i = 0; i < count; ++i) {
            uint64_t addr = base + 8 * (uint64_t)i;
            std::ostringstream idx;
            idx << std::hex << std::setw(2) << std::setfill('0') << i << ":" << std::setw(4)
                << (addr - base);
            std::cout << idx.str() << "│";

            if (addr == rsp && addr == rbp) {
                std::cout << " rbp rsp ";
            } else if (addr == rbp) {
                std::cout << " rbp     ";
            } else if (addr == rsp) {
                std::cout << " rsp     ";
            } else {
                std::ostringstream off;
                off << "+" << std::hex << std::setw(3) << std::setfill('0') << (addr - base);
                std::cout << off.str() << "     ";
            }

            std::cout << render_slot_value(dbg, dbg.read_memory(addr)) << "\n";
        }
        std::cout << std::dec << std::setfill(' ');
    }

    void print_frames(debugger &dbg) {
        if (!dbg.dwarf_ok()) {
            std::cout << "DWARF not available; rebuild the target with -gdwarf-4.\n";
            return;
        }

        std::vector<frame_info> frames;
        uint64_t pc = dbg.get_pc();
        uint64_t rbp = get_register_value(dbg.get_pid(), reg::rbp);

        while ((int)frames.size() < kMaxFrames) {
            frame_info f;
            f.pc = pc;
            f.rbp = rbp;
            try {
                dwarf::die func = dbg.get_function_from_pc(dbg.offset_load_address(pc));
                if (func.valid() && func.has(dwarf::DW_AT::name)) {
                    f.func = func;
                    f.symbolized = true;
                }
            } catch (const std::exception &) {
            }
            if (f.symbolized)
                f.ret = dbg.read_memory(f.rbp + 8);
            frames.push_back(f);
            if (!f.symbolized || f.ret == 0)
                break;
            pc = f.ret;
            rbp = dbg.read_memory(f.rbp);
            if (rbp == 0)
                break;
        }

        uint64_t rsp = get_register_value(dbg.get_pid(), reg::rsp);

        std::cout << "frames (" << frames.size() << "):\n";
        for (size_t i = 0; i < frames.size(); ++i) {
            const frame_info &f = frames[i];
            uint64_t cfa = f.rbp + 16;

            std::cout << "frame " << i << ": " << frame_name(f) << "  pc=0x" << std::hex << f.pc
                      << "  rbp=0x" << f.rbp << "  cfa=0x" << cfa;

            uint64_t size = 0;
            bool have_size = false;
            if (i == 0) {
                if (cfa >= rsp) {
                    size = cfa - rsp;
                    have_size = true;
                }
            } else if (f.rbp >= frames[i - 1].rbp) {
                size = f.rbp - frames[i - 1].rbp;
                have_size = true;
            }
            if (have_size)
                std::cout << "  size=0x" << size;
            std::cout << std::dec << "\n";

            std::cout << "         ret=0x" << std::hex << f.ret << std::dec;
            if (i + 1 >= frames.size() || !frames[i + 1].symbolized)
                std::cout << "  (stack bottom)";
            std::cout << "\n";

            if (f.symbolized) {

                uint64_t frame_base = cfa;
                auto vars = collect_local_variables(f.func, frame_base);
                if (!vars.empty()) {
                    std::cout << "         locals:";
                    for (const auto &v : vars)
                        std::cout << " " << v.name << " @ 0x" << std::hex << v.address << std::dec
                                  << offset_suffix(v.cfa_offset);
                    std::cout << "\n";
                }
            }
        }
    }
} // namespace minidbg
