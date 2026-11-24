#include "Ram.hpp"

#include <cassert>
#include <cstdint>

using namespace nexus::ps1;

int main() {
    Ram ram;
    assert(ram.size() == 2u * 1024u * 1024u);

    // RAM starts cleared and supports little-endian byte/halfword/word access.
    std::uint8_t byte = 0xFF;
    std::uint16_t half = 0xFFFF;
    std::uint32_t word = 0xFFFFFFFFu;
    assert(ram.read8(0x100u, byte) && byte == 0);
    assert(ram.write32(0x100u, 0x12345678u));
    assert(ram.read8(0x100u, byte) && byte == 0x78u);
    assert(ram.read16(0x100u, half) && half == 0x5678u);
    assert(ram.read32(0x100u, word) && word == 0x12345678u);

    // The same physical RAM is visible through KSEG0 and KSEG1.
    assert(ram.write32(0x80000100u, 0xAABBCCDDu));
    assert(ram.read32(0x00000100u, word) && word == 0xAABBCCDDu);
    assert(ram.write32(0xA0000100u, 0x0BADF00Du));
    assert(ram.read32(0x80000100u, word) && word == 0x0BADF00Du);

    // The 8 MiB physical RAM window mirrors the 2 MiB chip four times.
    assert(ram.write32(0x00200020u, 0xCAFEBABEu));
    assert(ram.read32(0x00000020u, word) && word == 0xCAFEBABEu);
    assert(ram.write32(0x00400020u, 0x11223344u));
    assert(ram.read32(0x00000020u, word) && word == 0x11223344u);
    assert(ram.write32(0x00600020u, 0x55667788u));
    assert(ram.read32(0x00000020u, word) && word == 0x55667788u);

    // Alignment and non-RAM addresses are rejected by this RAM device.
    assert(!ram.read16(0x101u, half));
    assert(!ram.write16(0x101u, 0x1234u));
    assert(!ram.read32(0x102u, word));
    assert(!ram.write32(0x102u, 0x12345678u));
    assert(!ram.read8(0x1F800000u, byte)); // scratchpad is a separate device
    assert(!ram.write8(0x1FC00000u, 0x12u)); // BIOS is a separate device


    // Every 2 MiB mirror is also reachable through the direct-mapped KSEG aliases.
    const std::uint32_t mirrorOffsets[] = {0x00000000u, 0x00200000u,
                                           0x00400000u, 0x00600000u};
    for (const auto mirror : mirrorOffsets) {
        assert(ram.write32(mirror + 0x100u, mirror + 0xABCDEFu));
        assert(ram.read32(0x80000000u + mirror + 0x100u, word));
        assert(word == mirror + 0xABCDEFu);
        assert(ram.read32(0xA0000000u + mirror + 0x100u, word));
        assert(word == mirror + 0xABCDEFu);
    }

    // The final aligned locations in the 2 MiB RAM chip remain accessible
    // through the final mirror, including KSEG0 and KSEG1.
    assert(ram.write8(0x001FFFFFu, 0xE7u));
    assert(ram.read8(0x007FFFFFu, byte) && byte == 0xE7u);
    assert(ram.write16(0x007FFFFEu, 0xBEEFu));
    assert(ram.read16(0x001FFFFEu, half) && half == 0xBEEFu);
    assert(ram.write32(0x807FFFFCu, 0x89ABCDEFu));
    assert(ram.read32(0xA07FFFFCu, word) && word == 0x89ABCDEFu);

    // The RAM window ends at 8 MiB; KSEG2 is not a direct-mapped alias.
    assert(!ram.read8(0x00800000u, byte));
    assert(!ram.write8(0x00800000u, 0x12u));
    assert(!ram.read8(0xC0000000u, byte));
    assert(!ram.write8(0xC0000000u, 0x12u));
    assert(!ram.read32(0xC0000100u, word));
    assert(!ram.write32(0xC0000100u, 0x12345678u));

    // Misalignment is rejected consistently through virtual aliases.
    assert(!ram.read16(0x80000101u, half));
    assert(!ram.write16(0xA0000101u, 0x1234u));
    assert(!ram.read32(0x807FFFFEu, word));
    assert(!ram.write32(0xA07FFFFEu, 0x12345678u));

    ram.clear(0x5Au);
    assert(ram.read8(0x100u, byte) && byte == 0x5Au);
    assert(ram.read32(0x100u, word) && word == 0x5A5A5A5Au);
    ram.clear();
    assert(ram.read32(0x100u, word) && word == 0u);

    return 0;
}
