#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nexus::ps1 {

// Register-level model of the seven PS1 DMA channels.
// Transfer execution and bus arbitration are deliberately implemented later.
class DmaController final {
public:
    static constexpr std::uint32_t kBaseAddress = 0x1F801080u;
    static constexpr std::uint32_t kDpcrAddress = 0x1F8010F0u;
    static constexpr std::uint32_t kDicrAddress = 0x1F8010F4u;
    static constexpr std::size_t kChannelCount = 7;

    DmaController() noexcept { reset(); }
    void reset() noexcept;

    bool read32(std::uint32_t address, std::uint32_t& value) const noexcept;
    bool write32(std::uint32_t address, std::uint32_t value) noexcept;

    std::uint32_t base(std::size_t channel) const noexcept;
    std::uint32_t blockControl(std::size_t channel) const noexcept;
    std::uint32_t channelControl(std::size_t channel) const noexcept;
    std::uint32_t dpcr() const noexcept { return dpcr_; }
    std::uint32_t dicr() const noexcept;

private:
    struct Channel {
        std::uint32_t base = 0;
        std::uint32_t blockControl = 0;
        std::uint32_t channelControl = 0;
    };
    std::array<Channel, kChannelCount> channels_{};
    std::uint32_t dpcr_ = 0;
    std::uint32_t dicrControl_ = 0;
    std::uint32_t dicrFlags_ = 0;

    bool masterIrq() const noexcept;
};

} // namespace nexus::ps1
