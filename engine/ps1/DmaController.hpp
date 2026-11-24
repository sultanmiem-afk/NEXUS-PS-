#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nexus::ps1 {

class Ram;
class Gpu;
class Cdrom;

// Seven-channel DMA register model with explicit GPU transfer execution.
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

    // Executes a pending channel-2 transfer synchronously. Bus timing,
    // arbitration and IRQ delivery are handled by later system integration.
    bool executeGpuTransfer(Ram& ram, Gpu& gpu);
    bool executeCdromTransfer(Ram& ram, Cdrom& cdrom);
    std::uint32_t lastCdromTransferWords() const noexcept { return lastCdromTransferWords_; }
    std::uint32_t lastTransferWords() const noexcept { return lastTransferWords_; }

    std::uint32_t base(std::size_t channel) const noexcept;
    std::uint32_t blockControl(std::size_t channel) const noexcept;
    std::uint32_t channelControl(std::size_t channel) const noexcept;
    std::uint32_t dpcr() const noexcept { return dpcr_; }
    std::uint32_t dicr() const noexcept;
    bool irqPending() const noexcept { return masterIrq(); }

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
    std::uint32_t lastTransferWords_ = 0;
    std::uint32_t lastCdromTransferWords_ = 0;
    bool cdromDmaActive_ = false;
    std::uint32_t cdromDmaBlockSize_ = 0;
    std::uint32_t cdromDmaBlocksRemaining_ = 0;
    std::uint32_t cdromDmaWordsInBlock_ = 0;

    bool masterIrq() const noexcept;
    bool executeGpuLinearTransfer(Ram& ram, Gpu& gpu, std::uint32_t& address,
                                  std::uint32_t words, bool toGpu, bool decrement);
    bool executeGpuLinkedList(Ram& ram, Gpu& gpu, std::uint32_t& address,
                              bool toGpu);
};

} // namespace nexus::ps1
