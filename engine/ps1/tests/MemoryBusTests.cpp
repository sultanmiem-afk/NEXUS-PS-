#include "MemoryBus.hpp"

#include <cassert>
#include <cstdint>

using namespace nexus::ps1;

int main() {
    MemoryBus bus;
    std::uint32_t word = 0;

    // Main RAM is exactly 2 MiB and mirrored in its 8 MiB physical window.
    assert(bus.ram().size() == 2u * 1024u * 1024u);
    assert(bus.write32(0x00000020u, 0x12345678u));
    assert(bus.read32(0x80200020u, word));
    assert(word == 0x12345678u);

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
    assert(!bus.read32(0x1F801000u, word));

    // Alignment rules apply to halfword and word accesses.
    assert(!bus.read16(0x1F800001u, *reinterpret_cast<std::uint16_t*>(&word)));

    bus.clear();
    assert(bus.read32(0x00000020u, word) && word == 0u);
    assert(bus.read32(0x1F800000u, word) && word == 0u);
    return 0;
}
