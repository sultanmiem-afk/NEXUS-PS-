#include "BiosRom.hpp"

#include <algorithm>

namespace nexus::ps1 {

BiosRom::BiosRom() noexcept {
    bytes_.fill(0xFFu);
}

bool BiosRom::load(const std::uint8_t* data, std::size_t length) noexcept {
    if (data == nullptr || length != kSize) return false;
    std::copy(data, data + kSize, bytes_.begin());
    loaded_ = true;
    return true;
}

void BiosRom::unload() noexcept {
    bytes_.fill(0xFFu);
    loaded_ = false;
}

bool BiosRom::translate(std::uint32_t address, std::size_t& offset) const noexcept {
    if (address >= 0xC0000000u) return false;
    const std::uint32_t physical = address & 0x1FFFFFFFu;
    if (physical < kBaseAddress || physical >= kBaseAddress + kSize) return false;
    offset = static_cast<std::size_t>(physical - kBaseAddress);
    return true;
}

bool BiosRom::read8(std::uint32_t address, std::uint8_t& value) const noexcept {
    std::size_t offset = 0;
    if (!loaded_ || !translate(address, offset)) return false;
    value = bytes_[offset];
    return true;
}

bool BiosRom::read16(std::uint32_t address, std::uint16_t& value) const noexcept {
    if ((address & 1u) != 0) return false;
    std::size_t offset = 0;
    if (!loaded_ || !translate(address, offset) || offset + 1 >= kSize) return false;
    value = static_cast<std::uint16_t>(bytes_[offset]) |
            (static_cast<std::uint16_t>(bytes_[offset + 1]) << 8);
    return true;
}

bool BiosRom::read32(std::uint32_t address, std::uint32_t& value) const noexcept {
    if ((address & 3u) != 0) return false;
    std::size_t offset = 0;
    if (!loaded_ || !translate(address, offset) || offset + 3 >= kSize) return false;
    value = static_cast<std::uint32_t>(bytes_[offset]) |
            (static_cast<std::uint32_t>(bytes_[offset + 1]) << 8) |
            (static_cast<std::uint32_t>(bytes_[offset + 2]) << 16) |
            (static_cast<std::uint32_t>(bytes_[offset + 3]) << 24);
    return true;
}

} // namespace nexus::ps1
