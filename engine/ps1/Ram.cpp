#include "Ram.hpp"

#include <algorithm>

namespace nexus::ps1 {

Ram::Ram() : bytes_(kSize, 0) {}

void Ram::clear(std::uint8_t value) noexcept {
    std::fill(bytes_.begin(), bytes_.end(), value);
}

bool Ram::translate(std::uint32_t address, std::size_t& offset) const noexcept {
    const std::uint32_t physical = address & kPhysicalMask;
    if (physical >= kPhysicalWindowSize) return false;

    // The 8 MiB address window mirrors the physical 2 MiB RAM four times.
    offset = static_cast<std::size_t>(physical & kRamIndexMask);
    return true;
}

bool Ram::read8(std::uint32_t address, std::uint8_t& value) const noexcept {
    std::size_t offset = 0;
    if (!translate(address, offset)) return false;
    value = bytes_[offset];
    return true;
}

bool Ram::read16(std::uint32_t address, std::uint16_t& value) const noexcept {
    if ((address & 1u) != 0) return false;
    std::size_t offset = 0;
    if (!translate(address, offset) || offset + 1 >= bytes_.size()) return false;
    value = static_cast<std::uint16_t>(bytes_[offset]) |
            (static_cast<std::uint16_t>(bytes_[offset + 1]) << 8);
    return true;
}

bool Ram::read32(std::uint32_t address, std::uint32_t& value) const noexcept {
    if ((address & 3u) != 0) return false;
    std::size_t offset = 0;
    if (!translate(address, offset) || offset + 3 >= bytes_.size()) return false;
    value = static_cast<std::uint32_t>(bytes_[offset]) |
            (static_cast<std::uint32_t>(bytes_[offset + 1]) << 8) |
            (static_cast<std::uint32_t>(bytes_[offset + 2]) << 16) |
            (static_cast<std::uint32_t>(bytes_[offset + 3]) << 24);
    return true;
}

bool Ram::write8(std::uint32_t address, std::uint8_t value) noexcept {
    std::size_t offset = 0;
    if (!translate(address, offset)) return false;
    bytes_[offset] = value;
    return true;
}

bool Ram::write16(std::uint32_t address, std::uint16_t value) noexcept {
    if ((address & 1u) != 0) return false;
    std::size_t offset = 0;
    if (!translate(address, offset) || offset + 1 >= bytes_.size()) return false;
    bytes_[offset] = static_cast<std::uint8_t>(value);
    bytes_[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    return true;
}

bool Ram::write32(std::uint32_t address, std::uint32_t value) noexcept {
    if ((address & 3u) != 0) return false;
    std::size_t offset = 0;
    if (!translate(address, offset) || offset + 3 >= bytes_.size()) return false;
    bytes_[offset] = static_cast<std::uint8_t>(value);
    bytes_[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    bytes_[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    bytes_[offset + 3] = static_cast<std::uint8_t>(value >> 24);
    return true;
}

} // namespace nexus::ps1
