#include "dwarf_util.hpp"

namespace minidbg {
    size_t read_uleb128(const uint8_t *p, unsigned *out) {
        unsigned result = 0;
        size_t shift = 0;
        size_t i = 0;
        while (true) {
            uint8_t byte = p[i++];
            result |= (byte & 0x7f) << shift;
            if (!(byte & 0x80))
                break;
            shift += 7;
        }
        *out = result;
        return i;
    }

    size_t read_sleb128(const uint8_t *p, int64_t *out) {
        int64_t result = 0;
        size_t shift = 0;
        size_t i = 0;
        uint8_t byte;
        while (true) {
            byte = p[i++];
            result |= (int64_t)(byte & 0x7f) << shift;
            shift += 7;
            if (!(byte & 0x80))
                break;
        }
        if (shift < 64 && (byte & 0x40))
            result |= -((int64_t)1 << shift);
        *out = result;
        return i;
    }
} // namespace minidbg
