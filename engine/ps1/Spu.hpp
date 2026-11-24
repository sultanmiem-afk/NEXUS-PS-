#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nexus::ps1 {

// SPU foundation: 512 KiB sound RAM and the documented 16-bit register map.
// Register storage is implemented before register side effects and audio playback.
class Spu final {
public:
    static constexpr std::size_t kRamSize = 512u * 1024u;

    Spu() noexcept { reset(); }

    void reset() noexcept;
    void clearRam(std::uint8_t value = 0) noexcept;

    std::size_t ramSize() const noexcept { return ram_.size(); }

    // Byte offset relative to the SPU register base (0x1F801C00).
    // Only documented register slots are accepted; reserved holes fail.
    bool readRegister16(std::uint32_t offset, std::uint16_t& value) const noexcept;
    bool writeRegister16(std::uint32_t offset, std::uint16_t value) noexcept;

    bool readRam8(std::uint32_t address, std::uint8_t& value) const noexcept;
    bool readRam16(std::uint32_t address, std::uint16_t& value) const noexcept;
    bool readRam32(std::uint32_t address, std::uint32_t& value) const noexcept;

    bool writeRam8(std::uint32_t address, std::uint8_t value) noexcept;
    bool writeRam16(std::uint32_t address, std::uint16_t value) noexcept;
    bool writeRam32(std::uint32_t address, std::uint32_t value) noexcept;

private:
    static constexpr std::size_t kRegisterCount = 0x400u / 2u;
    std::array<std::uint8_t, kRamSize> ram_{};
    std::array<std::uint16_t, kRegisterCount> registers_{};

    static bool isMappedRegister(std::uint32_t offset) noexcept;
};

} // namespace nexus::ps1
