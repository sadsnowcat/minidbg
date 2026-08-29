#include "variables.hpp"

#include "debugger.hpp"
#include "dwarf_util.hpp"
#include "registers.hpp"

#include <dwarf++.hh>

#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

namespace minidbg {
    namespace {
        //
        size_t type_size_bytes(const dwarf::die &var) {
            if (!var.has(dwarf::DW_AT::type))
                return 8;
            dwarf::die t = var[dwarf::DW_AT::type].as_reference();
            int depth = 0;
            while (t.valid() && depth++ < 12) {
                if (t.tag == dwarf::DW_TAG::base_type) {
                    if (t.has(dwarf::DW_AT::byte_size))
                        return (size_t)t[dwarf::DW_AT::byte_size].as_uconstant();
                    return 8;
                }
                if (t.has(dwarf::DW_AT::type))
                    t = t[dwarf::DW_AT::type].as_reference();
                else
                    break;
            }
            return 8;
        }

        void print_one_variable(debugger &dbg, const dwarf::die &var, uint64_t frame_base,
                                int &count) {
            if (!var.has(dwarf::DW_AT::name) || !var.has(dwarf::DW_AT::location))
                return;
            auto loc = var[dwarf::DW_AT::location];
            if (loc.get_type() != dwarf::value::type::exprloc &&
                loc.get_type() != dwarf::value::type::block)
                return;
            size_t sz = 0;
            const auto *b = reinterpret_cast<const uint8_t *>(loc.as_block(&sz));
            if (sz == 0)
                return;
            uint8_t op = b[0];
            std::string name = dwarf::at_name(var);
            if (op == static_cast<uint8_t>(dwarf::DW_OP::fbreg)) {
                int64_t offset = 0;
                read_sleb128(b + 1, &offset);
                uint64_t addr = frame_base + (uint64_t)offset;
                size_t vsize = type_size_bytes(var);
                uint64_t val = dbg.read_memory(addr);
                if (vsize < 8)
                    val &= (vsize == 8) ? ~0ULL : ((1ULL << (8 * vsize)) - 1);
                std::cout << "  " << name << " @ 0x" << std::hex << addr << " = 0x" << val << " ("
                          << std::dec << vsize << " bytes)\n";
                ++count;
            } else if (op >= static_cast<uint8_t>(dwarf::DW_OP::reg0) &&
                       op <= static_cast<uint8_t>(dwarf::DW_OP::reg31)) {
                unsigned regnum = op - static_cast<uint8_t>(dwarf::DW_OP::reg0);
                uint64_t val = get_register_value_from_dwarf_register(dbg.get_pid(), regnum);
                std::cout << "  " << name << " (in reg " << regnum << ") = 0x" << std::hex << val
                          << "\n";
                ++count;
            } else if (op == static_cast<uint8_t>(dwarf::DW_OP::regx)) {
                unsigned regnum = 0;
                read_uleb128(b + 1, &regnum);
                uint64_t val = get_register_value_from_dwarf_register(dbg.get_pid(), regnum);
                std::cout << "  " << name << " (in reg " << regnum << ") = 0x" << std::hex << val
                          << "\n";
                ++count;
            } else if (op == static_cast<uint8_t>(dwarf::DW_OP::addr)) {
                uint64_t addr = 0;
                for (int k = 0; k < 8 && (size_t)(1 + k) < sz; ++k)
                    addr |= (uint64_t)b[1 + k] << (8 * k);
                uint64_t val = dbg.read_memory(addr);
                std::cout << "  " << name << " (global) @ 0x" << std::hex << addr << " = 0x" << val
                          << "\n";
                ++count;
            } else {
                std::cout << "  " << name << " (unsupported location op 0x" << std::hex << (int)op
                          << ")\n";
                ++count;
            }
        }

        void walk_variables(debugger &dbg, const dwarf::die &d, uint64_t frame_base, int &count) {
            for (const auto &child : d) {
                if (child.tag == dwarf::DW_TAG::variable ||
                    child.tag == dwarf::DW_TAG::formal_parameter)
                    print_one_variable(dbg, child, frame_base, count);
                else if (child.tag == dwarf::DW_TAG::lexical_block)
                    walk_variables(dbg, child, frame_base, count);
            }
        }
    } // namespace

    void print_variables(debugger &dbg) {
        if (!dbg.dwarf_ok()) {
            std::cout << "DWARF not available; rebuild the target with -gdwarf-4.\n";
            return;
        }
        auto pc = dbg.offset_load_address(dbg.get_pc());
        dwarf::die func;
        try {
            func = dbg.get_function_from_pc(pc);
        } catch (const std::exception &) {
            std::cout << "Cannot find function at current PC.\n";
            return;
        }
        if (!func.valid()) {
            std::cout << "Cannot find function at current PC.\n";
            return;
        }
        uint64_t frame_base = 0;
        bool have_fb = false;
        if (func.has(dwarf::DW_AT::frame_base)) {
            auto fb = func[dwarf::DW_AT::frame_base];
            if (fb.get_type() == dwarf::value::type::exprloc ||
                fb.get_type() == dwarf::value::type::block) {
                size_t sz = 0;
                const auto *b = reinterpret_cast<const uint8_t *>(fb.as_block(&sz));
                if (sz > 0) {
                    uint8_t op = b[0];
                    if (op == static_cast<uint8_t>(dwarf::DW_OP::call_frame_cfa)) {

                        frame_base = get_register_value(dbg.get_pid(), reg::rbp) + 16;
                        have_fb = true;
                    } else if (op >= static_cast<uint8_t>(dwarf::DW_OP::reg0) &&
                               op <= static_cast<uint8_t>(dwarf::DW_OP::reg31)) {
                        unsigned regnum = op - static_cast<uint8_t>(dwarf::DW_OP::reg0);
                        frame_base = get_register_value_from_dwarf_register(dbg.get_pid(), regnum);
                        have_fb = true;
                    } else if (op == static_cast<uint8_t>(dwarf::DW_OP::regx)) {
                        unsigned regnum = 0;
                        read_uleb128(b + 1, &regnum);
                        frame_base = get_register_value_from_dwarf_register(dbg.get_pid(), regnum);
                        have_fb = true;
                    }
                }
            }
        }
        if (!have_fb) {
            std::cout << "Frame base unavailable at current PC.\n";
            return;
        }
        int count = 0;
        walk_variables(dbg, func, frame_base, count);
        if (count == 0)
            std::cout << "(no local variables at this scope)\n";
    }
} // namespace minidbg
