#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nexus::ps1 {

// First-stage SPU foundation: the PlayStation's 512 KiB sound RAM.
// Voice playback, ADPCM decoding, registers and audio output are added later.
class Spu final {
public:
    static constexpr std::size_t kRamSize = 512u * 1024u;

    Spu() noexcept { reset(); }

    void reset() noexcept;
    void clearRam(std::uint8_t value = 0) noexcept;

    std::size_t ramSize() const noexcept { return ram_.size(); }

    bool readRam8(std::uint32_t address, std::uint8_t& value) const noexcept;
    bool readRam16(std::uint32_t address, std::uint16_t& value) const noexcept;
    bool readRam32(std::uint32_t address, std::uint32_t& value) const noexcept;

    bool writeRam8(std::uint32_t address, std::uint8_t value) noexcept;
    bool writeRam16(std::uint32_t address, std::uint16_t value) noexcept;
    bool writeRam32(std::uint32_t address, std::uint32_t value) noexcept;

private:
    std::array<std::uint8_t, kRamSize> ram_{};
};

} // namespace nexus::ps1
