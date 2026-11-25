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
    // Early BIOS scratch/expansion registers are intentionally benign.
    const std::uint32_t ignoredAddresses[] = {
        0x1F801000u, 0x1F801060u, 0xFFFE0130u, 0x1F802041u
    };
    for (const auto address : ignoredAddresses) {
        assert(bus.write32(address, 0xDEADBEEFu));
        word = 0xFFFFFFFFu;
        assert(bus.read32(address, word) && word == 0u);
    }
    word = 0xFFFFFFFFu;
    assert(bus.read32(0x1F801044u, word) && word == 0x5u);

    // BIOS startup probes are benign across both physical and KSEG1 aliases.
    const std::uint32_t benignAddresses[] = {
        0x1F801004u, 0x1F801008u, 0x1F80100Cu, 0x1F801010u,
        0x1F801014u, 0x1F801018u, 0x1F80101Cu, 0x1F801020u,
        0x1F80105Au, 0x1F801820u, 0x1F801824u, 0x1F802000u
    };
    for (const auto address : benignAddresses) {
        for (const auto probe : {address, address + 0xA0000000u}) {
            assert(bus.write32(probe, 0xA0000000u));
            word = 0xFFFFFFFFu;
            assert(bus.read32(probe, word) && word == 0u);
        }
    }
    for (const auto address : {0x1F801004u, 0x1F80105Au,
                               0x1F801820u, 0x1F801824u, 0x1F802000u}) {
        const auto alias = address + 0xA0000000u;
        std::uint8_t readByte = 0xFFu;
        std::uint16_t readHalf = 0xFFFFu;
        assert(bus.write8(alias, 0xA0u) && bus.read8(alias, readByte) && readByte == 0u);
        assert(bus.write16(alias, 0xA000u) && bus.read16(alias, readHalf) && readHalf == 0u);
    }

    // KSEG1 aliases of interrupt, DMA, timer and CD-ROM MMIO use physical decode.
    assert(bus.write32(0xBF801074u, 0x20u));
    assert(bus.read32(0x1F801074u, word) && word == 0x20u);
    assert(bus.write32(0xBF8010F0u, 0x12345678u));
    assert(bus.read32(0x1F8010F0u, word) && word == 0x12345678u);
    assert(bus.write16(0xBF801100u, 0x3456u));
    std::uint16_t aliasHalf = 0u;
    assert(bus.read16(0x1F801100u, aliasHalf) && aliasHalf == 0x3456u);
    std::uint8_t aliasByte = 0u;
    assert(bus.write8(0xBF801800u, 0u));
    assert(bus.read8(0x1F801800u, aliasByte) && aliasByte == 0x18u);

    // DMA registers are mapped through the CPU-visible memory bus.
    assert(bus.write32(0x1F801080u, 0x00123456u));
    assert(bus.read32(0x1F801080u, word) && word == 0x00123456u);
    assert(bus.write32(0x1F8010F0u, 0x0FFFFFFFu));
    assert(bus.read32(0x1F8010F0u, word) && word == 0x0FFFFFFFu);
    assert(!bus.read32(0x1F8010F8u, word));

    // Timer 0 counter/mode/target registers are mapped at 1F801100-108.
    assert(bus.write32(0x1F801108u, 123u));
    assert(bus.write32(0x1F801100u, 7u));
    assert(bus.read32(0x1F801100u, word) && word == 7u);
    assert(bus.read32(0x1F801108u, word) && word == 123u);
    assert(bus.write32(0x1F801104u, 0u));
    assert(bus.read32(0x1F801100u, word) && word == 0u);

    // Timer 1 counter/mode/target registers are mapped at 1F801110-118.
    assert(bus.write32(0x1F801118u, 456u));
    assert(bus.write32(0x1F801110u, 9u));
    assert(bus.read32(0x1F801110u, word) && word == 9u);
    assert(bus.read32(0x1F801118u, word) && word == 456u);
    assert(bus.write32(0x1F801114u, 0u));
    assert(bus.read32(0x1F801110u, word) && word == 0u);

    // SIO0 digital controller poll: 0x01, 0x42 returns ID and active-low buttons.
    bus.controller().setButtons(static_cast<std::uint16_t>((1u << 0u) | (1u << 4u) | (1u << 15u)));
    assert(bus.write16(0x1F80104Au, 0x0002u)); // select controller port
    assert(bus.write8(0x1F801040u, 0x01u));
    assert(bus.read32(0x1F801044u, word) && (word & 0x02u) != 0u);
    assert(bus.read8(0x1F801040u, byte) && byte == 0xFFu);
    assert(bus.write8(0x1F801040u, 0x42u));
    assert(bus.read8(0x1F801040u, byte) && byte == 0x41u);
    assert(bus.write8(0x1F801040u, 0x00u));
    assert(bus.read8(0x1F801040u, byte) && byte == 0x5Au);
    assert(bus.write8(0x1F801040u, 0x00u));
    assert(bus.read8(0x1F801040u, byte) && byte == 0xEEu);
    assert(bus.write8(0x1F801040u, 0x00u));
    assert(bus.read8(0x1F801040u, byte) && byte == 0x7Fu);
    assert(bus.read32(0x1F801044u, word) && (word & 0x02u) == 0u);

    // DualShock analog poll reports ID 0x73 and six payload bytes.
    bus.controller().setAnalogMode(true);
    bus.controller().setAnalogAxes(200u, 64u);
    assert(bus.write8(0x1F801040u, 0x01u));
    assert(bus.read8(0x1F801040u, byte) && byte == 0xFFu);
    assert(bus.write8(0x1F801040u, 0x42u));
    assert(bus.read8(0x1F801040u, byte) && byte == 0x73u);
    assert(bus.write8(0x1F801040u, 0x00u));
    assert(bus.read8(0x1F801040u, byte) && byte == 0x5Au);
    for (int i = 0; i < 2; ++i) {
        assert(bus.write8(0x1F801040u, 0x00u));
        assert(bus.read8(0x1F801040u, byte));
    }
    assert(bus.write8(0x1F801040u, 0x00u));
    assert(bus.read8(0x1F801040u, byte) && byte == 128u); // Right X
    assert(bus.write8(0x1F801040u, 0x00u));
    assert(bus.read8(0x1F801040u, byte) && byte == 128u); // Right Y
    assert(bus.write8(0x1F801040u, 0x00u));
    assert(bus.read8(0x1F801040u, byte) && byte == 200u); // Left X
    assert(bus.write8(0x1F801040u, 0x00u));
    assert(bus.read8(0x1F801040u, byte) && byte == 64u); // Left Y
    assert(bus.read32(0x1F801044u, word) && (word & 0x02u) == 0u);
    assert(bus.write16(0x1F80104Au, 0x0000u));

    // Alignment rules apply to halfword and word accesses.
    std::uint16_t half = 0;
    assert(!bus.read16(0x1F800001u, half));

    bus.clear();
    assert(bus.read32(0x00000020u, word) && word == 0u);
    assert(bus.read32(0x1F800000u, word) && word == 0u);
    return 0;
}
