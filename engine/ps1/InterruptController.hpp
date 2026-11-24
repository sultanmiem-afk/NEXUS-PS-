#pragma once

#include <cstdint>

namespace nexus::ps1 {

// PS1 interrupt controller registers at 1F801070 (I_STAT) and 1F801074 (I_MASK).
class InterruptController final {
public:
    static constexpr std::uint32_t kStatusAddress = 0x1F801070u;
    static constexpr std::uint32_t kMaskAddress = 0x1F801074u;
    static constexpr std::uint32_t kValidSources = 0x000007FFu;
    static constexpr std::uint32_t kCpuInterruptBit = 1u << 10;

    void reset() noexcept;
    std::uint32_t status() const noexcept { return status_; }
    std::uint32_t mask() const noexcept { return mask_; }
    bool active() const noexcept { return (status_ & mask_ & kValidSources) != 0u; }

    // Hardware devices set one or more pending source bits.
    void request(std::uint32_t sources) noexcept;
    bool read32(std::uint32_t address, std::uint32_t& value) const noexcept;
    bool write32(std::uint32_t address, std::uint32_t value) noexcept;

private:
    std::uint32_t status_ = 0;
    std::uint32_t mask_ = 0;
};

} // namespace nexus::ps1
