#include "Spu.hpp"

namespace nexus::ps1 {

void Spu::reset() noexcept {
    clearRam();
    registers_.fill(0);
}

void Spu::clearRam(std::uint8_t value) noexcept {
    ram_.fill(value);
}

bool Spu::readRam8(std::uint32_t address, std::uint8_t& value) const noexcept {
    if (address >= kRamSize) return false;
    value = ram_[address];
    return true;
}

bool Spu::readRam16(std::uint32_t address, std::uint16_t& value) const noexcept {
    if ((address & 1u) != 0 || address > kRamSize - 2u) return false;
    value = static_cast<std::uint16_t>(ram_[address]) |
            (static_cast<std::uint16_t>(ram_[address + 1u]) << 8);
    return true;
}

bool Spu::readRam32(std::uint32_t address, std::uint32_t& value) const noexcept {
    if ((address & 3u) != 0 || address > kRamSize - 4u) return false;
    value = static_cast<std::uint32_t>(ram_[address]) |
            (static_cast<std::uint32_t>(ram_[address + 1u]) << 8) |
            (static_cast<std::uint32_t>(ram_[address + 2u]) << 16) |
            (static_cast<std::uint32_t>(ram_[address + 3u]) << 24);
    return true;
}

bool Spu::writeRam8(std::uint32_t address, std::uint8_t value) noexcept {
    if (address >= kRamSize) return false;
    ram_[address] = value;
    return true;
}

bool Spu::writeRam16(std::uint32_t address, std::uint16_t value) noexcept {
    if ((address & 1u) != 0 || address > kRamSize - 2u) return false;
    ram_[address] = static_cast<std::uint8_t>(value & 0xFFu);
    ram_[address + 1u] = static_cast<std::uint8_t>(value >> 8);
    return true;
}

bool Spu::writeRam32(std::uint32_t address, std::uint32_t value) noexcept {
    if ((address & 3u) != 0 || address > kRamSize - 4u) return false;
    ram_[address] = static_cast<std::uint8_t>(value & 0xFFu);
    ram_[address + 1u] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    ram_[address + 2u] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    ram_[address + 3u] = static_cast<std::uint8_t>(value >> 24);
    return true;
}


bool Spu::isMappedRegister(std::uint32_t offset) noexcept {
    if ((offset & 1u) != 0 || offset >= 0x200u) return false;

    // Voice register bank: 24 voices, 16 bytes per voice.
    if (offset < 0x180u) return true;

    // Global SPU and reverb register window.
    return offset >= 0x180u && offset <= 0x1FEu;
}

bool Spu::readRegister16(std::uint32_t offset, std::uint16_t& value) const noexcept {
    if (!isMappedRegister(offset)) return false;
    value = registers_[offset / 2u];
    return true;
}

bool Spu::writeRegister16(std::uint32_t offset, std::uint16_t value) noexcept {
    if (!isMappedRegister(offset)) return false;
    registers_[offset / 2u] = value;
    return true;
}

} // namespace nexus::ps1
