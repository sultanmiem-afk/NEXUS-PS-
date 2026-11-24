#include "MemoryBus.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

using namespace nexus::ps1;

int main() {
    MemoryBus bus;
    std::uint32_t word = 0;

    // Main RAM is exactly 2 MiB and mirrored in its 8 MiB physical window.
    assert(bus.ram().size() == 2u * 1024u * 1024u);
    assert(bus.write32(0x00000020u, 0x12345678u));
    assert(bus.read32(0x80200020u, word));
    assert(word == 0x12345678u);


    // All four physical RAM mirrors must resolve through both KSEG aliases.
    const std::uint32_t mirrors[] = {0x00000000u, 0x00200000u,
                                     0x00400000u, 0x00600000u};
    for (const auto mirror : mirrors) {
        assert(bus.write32(mirror + 0x40u, 0x600D0000u | mirror));
        assert(bus.read32(0x80000000u + mirror + 0x40u, word));
        assert(word == (0x600D0000u | mirror));
        assert(bus.read32(0xA0000000u + mirror + 0x40u, word));
        assert(word == (0x600D0000u | mirror));
    }

    // KSEG2 and addresses beyond the 8 MiB RAM window are not RAM aliases.
    std::uint8_t invalidByte = 0;
    assert(!bus.read8(0x00800000u, invalidByte));
    assert(!bus.write8(0x00800000u, 0x77u));
    assert(!bus.read8(0xC0000000u, invalidByte));
    assert(!bus.write8(0xC0000000u, 0x77u));

    // Scratchpad is an independent 1 KiB region.
    assert(bus.scratchpad().size() == 1024u);
    assert(bus.write32(0x1F800000u, 0xA1B2C3D4u));
    assert(bus.read32(0x9F800000u, word));
    assert(word == 0xA1B2C3D4u);

    // The two devices must not alias one another.
    assert(bus.write32(0x00000000u, 0xCAFEBABEu));
    assert(bus.read32(0x1F800000u, word));
    assert(word == 0xA1B2C3D4u);

    // Out-of-range scratchpad, BIOS, and not-yet-implemented MMIO fail cleanly.
    std::uint8_t byte = 0;
    assert(!bus.read8(0x1F800400u, byte));
    assert(!bus.read32(0x1FC00000u, word));
    std::vector<std::uint8_t> biosImage(BiosRom::kSize, 0x5Au);
    assert(bus.bios().load(biosImage));
    assert(bus.read32(0x1FC00000u, word) && word == 0x5A5A5A5Au);
    assert(!bus.write32(0x1FC00000u, 0xFFFFFFFFu));
    assert(bus.read32(0xBFC00000u, word) && word == 0x5A5A5A5Au);
    assert(!bus.read32(0x1F801000u, word));

    // Alignment rules apply to halfword and word accesses.
    std::uint16_t half = 0;
    assert(!bus.read16(0x1F800001u, half));

    bus.clear();
    assert(bus.read32(0x00000020u, word) && word == 0u);
    assert(bus.read32(0x1F800000u, word) && word == 0u);
    return 0;
}
