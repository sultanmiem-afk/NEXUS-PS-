#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nexus::ps1 {

// Main PlayStation 1 RAM: 2 MiB, mirrored throughout the 8 MiB
// physical RAM window and accessible through KUSEG/KSEG0/KSEG1 aliases.
class Ram {
public:
    static constexpr std::size_t kSize = 2u * 1024u * 1024u;
    static constexpr std::uint32_t kPhysicalWindowSize = 8u * 1024u * 1024u;
    static constexpr std::uint32_t kPhysicalMask = 0x1FFFFFFFu;
    static constexpr std::uint32_t kRamWindowMask = 0x007FFFFFu;
    static constexpr std::uint32_t kRamIndexMask = 0x001FFFFFu;

    Ram();

    void clear(std::uint8_t value = 0) noexcept;
    constexpr std::size_t size() const noexcept { return kSize; }

    bool read8(std::uint32_t address, std::uint8_t& value) const noexcept;
    bool read16(std::uint32_t address, std::uint16_t& value) const noexcept;
    bool read32(std::uint32_t address, std::uint32_t& value) const noexcept;

    bool write8(std::uint32_t address, std::uint8_t value) noexcept;
    bool write16(std::uint32_t address, std::uint16_t value) noexcept;
    bool write32(std::uint32_t address, std::uint32_t value) noexcept;

private:
    std::vector<std::uint8_t> bytes_;

    bool translate(std::uint32_t address, std::size_t& offset) const noexcept;
};

} // namespace nexus::ps1
