#pragma once

#include <cstddef>
#include <cstdint>
#include <array>

namespace nexus::ps1 {

// The PS1 CPU scratchpad is a separate 1 KiB fast memory region.
class Scratchpad {
public:
    static constexpr std::size_t kSize = 1024;
    static constexpr std::uint32_t kBaseAddress = 0x1F800000u;

    Scratchpad() noexcept;

    constexpr std::size_t size() const noexcept { return kSize; }
    void clear(std::uint8_t value = 0) noexcept;

    bool read8(std::uint32_t address, std::uint8_t& value) const noexcept;
    bool read16(std::uint32_t address, std::uint16_t& value) const noexcept;
    bool read32(std::uint32_t address, std::uint32_t& value) const noexcept;
    bool write8(std::uint32_t address, std::uint8_t value) noexcept;
    bool write16(std::uint32_t address, std::uint16_t value) noexcept;
    bool write32(std::uint32_t address, std::uint32_t value) noexcept;

private:
    std::array<std::uint8_t, kSize> bytes_{};
    bool translate(std::uint32_t address, std::size_t& offset) const noexcept;
};

} // namespace nexus::ps1
