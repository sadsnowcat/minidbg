#ifndef MINIDBG_BREAKPOINT_HPP
#define MINIDBG_BREAKPOINT_HPP

#include <cstdint>
#include <sys/types.h>

namespace minidbg {
    class breakpoint {
    public:
        breakpoint() = default;
        breakpoint(pid_t pid, std::uintptr_t addr)
            : m_pid{pid}, m_addr{addr}, m_enabled{false}, m_saved_data{} {}

        void enable();
        void disable();

        auto is_enabled() const -> bool { return m_enabled; }
        auto get_address() const -> std::uintptr_t { return m_addr; }

    private:
        pid_t m_pid;
        std::uintptr_t m_addr;
        bool m_enabled;
        uint8_t m_saved_data;
    };
} // namespace minidbg

#endif
