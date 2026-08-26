#include "breakpoint.hpp"

#include <sys/ptrace.h>

namespace minidbg {

    const uint64_t Int3 = 0xcc;

    void breakpoint::enable() {
        auto data = ptrace(PTRACE_PEEKDATA, m_pid, m_addr, nullptr);

        m_saved_data = static_cast<uint8_t>(data & 0xff);

        uint64_t data_with_int3 = ((data & ~0xff) | Int3);

        ptrace(PTRACE_POKEDATA, m_pid, m_addr, data_with_int3);

        m_enabled = true;
    }

    void breakpoint::disable() {
        auto data = ptrace(PTRACE_PEEKDATA, m_pid, m_addr, nullptr);
        auto restored_data = ((data & ~0xff) | m_saved_data);
        ptrace(PTRACE_POKEDATA, m_pid, m_addr, restored_data);

        m_enabled = false;
    }

} // namespace minidbg