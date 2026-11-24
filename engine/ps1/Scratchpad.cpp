#include "Scratchpad.hpp"

#include <algorithm>

namespace nexus::ps1 {

Scratchpad::Scratchpad() noexcept = default;

void Scratchpad::clear(std::uint8_t value) noexcept {
    bytes_.fill(value);
}

bool Scratchpad::translate(std::uint32_t address, std::size_t& offset) const noexcept {
    if (address >= 0xC0000000u) return false;
    const std::uint32_t physical = address & 0x1FFFFFFFu;
    if (physical < kBaseAddress || physical >= kBaseAddress + kSize) return false;
    offset = static_cast<std::size_t>(physical - kBaseAddress);
    return true;
}

bool Scratchpad::read8(std::uint32_t address, std::uint8_t& value) const noexcept {
    std::size_t offset = 0;
    if (!translate(address, offset)) return false;
    value = bytes_[offset];
    return true;
}

bool Scratchpad::read16(std::uint32_t address, std::uint16_t& value) const noexcept {
    if ((address & 1u) != 0) return false;
    std::size_t offset = 0;
    if (!translate(address, offset) || offset + 1 >= kSize) return false;
    value = static_cast<std::uint16_t>(bytes_[offset]) |
            (static_cast<std::uint16_t>(bytes_[offset + 1]) << 8);
    return true;
}

bool Scratchpad::read32(std::uint32_t address, std::uint32_t& value) const noexcept {
    if ((address & 3u) != 0) return false;
    std::size_t offset = 0;
    if (!translate(address, offset) || offset + 3 >= kSize) return false;
    value = static_cast<std::uint32_t>(bytes_[offset]) |
            (static_cast<std::uint32_t>(bytes_[offset + 1]) << 8) |
            (static_cast<std::uint32_t>(bytes_[offset + 2]) << 16) |
            (static_cast<std::uint32_t>(bytes_[offset + 3]) << 24);
    return true;
}

bool Scratchpad::write8(std::uint32_t address, std::uint8_t value) noexcept {
    std::size_t offset = 0;
    if (!translate(address, offset)) return false;
    bytes_[offset] = value;
    return true;
}

bool Scratchpad::write16(std::uint32_t address, std::uint16_t value) noexcept {
    if ((address & 1u) != 0) return false;
    std::size_t offset = 0;
    if (!translate(address, offset) || offset + 1 >= kSize) return false;
    bytes_[offset] = static_cast<std::uint8_t>(value);
    bytes_[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    return true;
}

bool Scratchpad::write32(std::uint32_t address, std::uint32_t value) noexcept {
    if ((address & 3u) != 0) return false;
    std::size_t offset = 0;
    if (!translate(address, offset) || offset + 3 >= kSize) return false;
    bytes_[offset] = static_cast<std::uint8_t>(value);
    bytes_[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes_[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes_[offset + 3] = static_cast<std::uint8_t>(value >> 24);
    return true;
}

} // namespace nexus::ps1
