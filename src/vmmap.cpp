#include "vmmap.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

namespace minidbg {
    void print_vmmap(pid_t pid, uint64_t pc) {
        std::ifstream maps("/proc/" + std::to_string(pid) + "/maps");
        if (!maps) {
            std::cerr << "cannot open /proc/" << pid << "/maps" << std::endl;
            return;
        }

        std::cout << std::left << std::setw(3) << "" << std::setw(34) << "address-range"
                  << std::setw(6) << "perms" << std::setw(10) << "offset" << std::setw(8) << "dev"
                  << std::setw(8) << "inode"
                  << "object" << std::endl;
        std::cout << std::string(72, '-') << std::endl;

        std::string addr_range, perm, offset, dev, inode, path;

        while (maps >> addr_range >> perm >> offset >> dev >> inode) {
            std::getline(maps >> std::ws, path);

            uint64_t start = 0, end = 0;
            auto dash = addr_range.find('-');
            if (dash != std::string::npos) {
                start = std::stoul(addr_range.substr(0, dash), nullptr, 16);
                end = std::stoul(addr_range.substr(dash + 1), nullptr, 16);
            }
            bool hit = (pc >= start && pc < end);

            std::cout << std::left << std::setw(3) << (hit ? ">" : " ") << std::setw(34)
                      << addr_range << std::setw(6) << perm << std::setw(10) << offset
                      << std::setw(8) << dev << std::setw(8) << inode << path << std::endl;
        }
    }

} // namespace minidbg
