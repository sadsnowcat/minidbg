#include "vmmap.hpp"

#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>


namespace minidbg {
    void print_vmmap(pid_t pid) {

        std::ifstream maps("/proc/" + std::to_string(pid) + "/maps");
        if (!maps) {
            std::cerr << "cannot open /proc/" << pid << "/maps" << std::endl;
            return;
        }
        std::cout << std::left
                  << std::setw(34) << "address-range"
                  << std::setw(6)  << "perms"
                  << std::setw(10) << "offset"
                  << std::setw(8)  << "dev"
                  << std::setw(8)  << "inode"
                  << "object" << std::endl;
        std::cout << std::string(70, '-') << std::endl;

        std::string line;
        while (std::getline(maps, line)) {

            std::istringstream iss(line);
            std::string addr_range, perm, offset, dev, inode, path;
            iss >> addr_range >> perm >> offset >> dev >> inode;
            std::getline(iss >> std::ws, path);
            std::cout << std::left
                      << std::setw(34) << addr_range
                      << std::setw(6)  << perm
                      << std::setw(10) << offset
                      << std::setw(8)  << dev
                      << std::setw(8)  << inode
                      << path << std::endl;
        }
    }

} // namespace minidbg