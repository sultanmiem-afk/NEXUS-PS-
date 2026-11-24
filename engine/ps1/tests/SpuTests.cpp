#include "Spu.hpp"

#include <cassert>
#include <cstdint>

using namespace nexus::ps1;

int main() {
    Spu spu;
    assert(spu.ramSize() == 512u * 1024u);

    // Sound RAM starts cleared and uses little-endian storage.
    std::uint8_t byte = 0xFFu;
    std::uint16_t half = 0xFFFFu;
    std::uint32_t word = 0xFFFFFFFFu;
    assert(spu.readRam8(0x100u, byte) && byte == 0u);
    assert(spu.writeRam32(0x100u, 0x12345678u));
    assert(spu.readRam8(0x100u, byte) && byte == 0x78u);
    assert(spu.readRam16(0x100u, half) && half == 0x5678u);
    assert(spu.readRam32(0x100u, word) && word == 0x12345678u);

    // Byte and halfword access can update portions of a word.
    assert(spu.writeRam8(0x101u, 0xABu));
    assert(spu.readRam32(0x100u, word) && word == 0x1234AB78u);
    assert(spu.writeRam16(0x102u, 0xCDEFu));
    assert(spu.readRam32(0x100u, word) && word == 0xCDEFAB78u);

    // The last valid addresses are accessible; out-of-range accesses fail.
    assert(spu.writeRam8(Spu::kRamSize - 1u, 0x5Au));
    assert(spu.readRam8(Spu::kRamSize - 1u, byte) && byte == 0x5Au);
    assert(spu.writeRam16(Spu::kRamSize - 2u, 0xBEEFu));
    assert(spu.readRam16(Spu::kRamSize - 2u, half) && half == 0xBEEFu);
    assert(spu.writeRam32(Spu::kRamSize - 4u, 0x89ABCDEFu));
    assert(spu.readRam32(Spu::kRamSize - 4u, word) && word == 0x89ABCDEFu);
    assert(!spu.readRam8(static_cast<std::uint32_t>(Spu::kRamSize), byte));
    assert(!spu.writeRam8(static_cast<std::uint32_t>(Spu::kRamSize), 0x12u));
    assert(!spu.readRam16(static_cast<std::uint32_t>(Spu::kRamSize - 1u), half));
    assert(!spu.writeRam16(static_cast<std::uint32_t>(Spu::kRamSize - 1u), 0x1234u));
    assert(!spu.readRam32(static_cast<std::uint32_t>(Spu::kRamSize - 2u), word));
    assert(!spu.writeRam32(static_cast<std::uint32_t>(Spu::kRamSize - 2u), 0x12345678u));

    // Multi-byte operations require natural alignment.
    assert(!spu.readRam16(0x101u, half));
    assert(!spu.writeRam16(0x101u, 0x1234u));
    assert(!spu.readRam32(0x102u, word));
    assert(!spu.writeRam32(0x102u, 0x12345678u));

    // Reset clears the entire sound RAM.
    spu.clearRam(0xA5u);
    assert(spu.readRam32(0x100u, word) && word == 0xA5A5A5A5u);
    spu.reset();
    assert(spu.readRam32(0x100u, word) && word == 0u);

    return 0;
}
