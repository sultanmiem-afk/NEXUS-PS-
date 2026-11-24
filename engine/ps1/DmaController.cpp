#include "DmaController.hpp"

#include "Cdrom.hpp"

#include "Gpu.hpp"
#include "Ram.hpp"

#include <algorithm>

namespace nexus::ps1 {

void DmaController::reset() noexcept {
    channels_ = {};
    // PS1 power-on DPCR priorities/enables default to zero.
    dpcr_ = 0x07654321u;
    dicrControl_ = 0;
    dicrFlags_ = 0;
    lastTransferWords_ = 0;
    lastCdromTransferWords_ = 0;
    cdromDmaActive_ = false;
    cdromDmaBlockSize_ = 0;
    cdromDmaBlocksRemaining_ = 0;
    cdromDmaWordsInBlock_ = 0;
}

std::uint32_t DmaController::base(std::size_t channel) const noexcept {
    return channel < kChannelCount ? channels_[channel].base : 0;
}
std::uint32_t DmaController::blockControl(std::size_t channel) const noexcept {
    return channel < kChannelCount ? channels_[channel].blockControl : 0;
}
std::uint32_t DmaController::channelControl(std::size_t channel) const noexcept {
    return channel < kChannelCount ? channels_[channel].channelControl : 0;
}

bool DmaController::masterIrq() const noexcept {
    const bool force = (dicrControl_ & (1u << 15)) != 0;
    const bool masterEnable = (dicrControl_ & (1u << 23)) != 0;
    const std::uint32_t enabledChannels = (dicrControl_ << 8) & 0x7F000000u;
    const bool enabledFlag = (enabledChannels & dicrFlags_) != 0;
    return force || (masterEnable && enabledFlag);
}

std::uint32_t DmaController::dicr() const noexcept {
    return (dicrControl_ & 0x00FFFFFFu) |
           (dicrFlags_ & 0x7F000000u) |
           (masterIrq() ? 0x80000000u : 0u);
}

bool DmaController::read32(std::uint32_t address,
                           std::uint32_t& value) const noexcept {
    if (address == kDpcrAddress) {
        value = dpcr_;
        return true;
    }
    if (address == kDicrAddress) {
        value = dicr();
        return true;
    }
    if (address < kBaseAddress || address >= kDpcrAddress ||
        (address & 3u) != 0) return false;

    const std::uint32_t offset = address - kBaseAddress;
    const std::size_t channel = offset / 0x10u;
    const std::uint32_t reg = offset & 0x0Fu;
    if (channel >= kChannelCount) return false;
    const Channel& ch = channels_[channel];
    if (reg == 0x0u) value = ch.base;
    else if (reg == 0x4u) value = ch.blockControl;
    else if (reg == 0x8u) value = ch.channelControl;
    else return false;
    return true;
}

bool DmaController::write32(std::uint32_t address,
                            std::uint32_t value) noexcept {
    if (address == kDpcrAddress) {
        dpcr_ = value;
        return true;
    }
    if (address == kDicrAddress) {
        // Control/enable bits are writable; channel flags are write-one-to-clear.
        dicrControl_ = value & 0x00FFFFFFu;
        dicrFlags_ &= ~(value & 0x7F000000u);
        return true;
    }
    if (address < kBaseAddress || address >= kDpcrAddress ||
        (address & 3u) != 0) return false;

    const std::uint32_t offset = address - kBaseAddress;
    const std::size_t channel = offset / 0x10u;
    const std::uint32_t reg = offset & 0x0Fu;
    if (channel >= kChannelCount) return false;
    Channel& ch = channels_[channel];
    if (reg == 0x0u) {
        ch.base = value & 0x00FFFFFFu;
    } else if (reg == 0x4u) {
        ch.blockControl = value;
    } else if (reg == 0x8u) {
        // Preserve the modeled writable control bits. The system integration
        // layer explicitly invokes the device-specific DMA execution paths.
        ch.channelControl = value & 0x71770703u;
        if (channel == 3u && (ch.channelControl & (1u << 24u)) != 0u) {
            cdromDmaActive_ = false;
        }
    } else {
        return false;
    }
    return true;
}


bool DmaController::executeCdromTransfer(Ram& ram, Cdrom& cdrom) {
    constexpr std::size_t kCdromChannel = 3u;
    constexpr std::uint32_t kMaxTransferWords = 1u << 20;
    Channel& channel = channels_[kCdromChannel];
    lastCdromTransferWords_ = 0u;

    const std::uint32_t control = channel.channelControl;
    const bool started = (control & (1u << 24u)) != 0u;
    const bool enabled = (dpcr_ & (1u << (kCdromChannel * 4u + 3u))) != 0u;
    const bool deviceToRam = (control & 1u) == 0u;
    const std::uint32_t sync = (control >> 9u) & 3u;
    if (!started) {
        cdromDmaActive_ = false;
        return false;
    }
    // A disabled DPCR channel is stalled, not cancelled: preserve the in-flight
    // block counters so the transfer can resume when the channel is re-enabled.
    if (!enabled) return false;
    if (!deviceToRam || sync != 1u) {
        cdromDmaActive_ = false;
        return false;
    }

    if (!cdromDmaActive_) {
        const std::uint32_t configuredBlockSize = channel.blockControl & 0xFFFFu;
        const std::uint32_t configuredBlockCount = channel.blockControl >> 16u;
        cdromDmaBlockSize_ = configuredBlockSize == 0u ? 0x10000u : configuredBlockSize;
        cdromDmaBlocksRemaining_ = configuredBlockCount == 0u
            ? 0x10000u : configuredBlockCount;
        if (cdromDmaBlockSize_ > kMaxTransferWords ||
            cdromDmaBlocksRemaining_ > kMaxTransferWords / cdromDmaBlockSize_) {
            return false;
        }
        cdromDmaWordsInBlock_ = cdromDmaBlockSize_;
        cdromDmaActive_ = true;
    }

    const std::size_t availableWords = cdrom.dataAvailableBytes() / 4u;
    if (availableWords == 0u) return false;

    std::uint32_t address = channel.base & 0x00FFFFFCu;
    const std::uint32_t wordsToTransfer = static_cast<std::uint32_t>(
        std::min<std::size_t>(availableWords, cdromDmaWordsInBlock_));
    for (std::uint32_t i = 0; i < wordsToTransfer; ++i) {
        std::uint32_t value = 0u;
        if (!cdrom.readDataWord(value) || !ram.write32(address, value)) return false;
        address = (address + 4u) & 0x00FFFFFFu;
        ++lastCdromTransferWords_;
        --cdromDmaWordsInBlock_;
    }
    channel.base = address & 0x00FFFFFFu;

    if (cdromDmaWordsInBlock_ == 0u) {
        --cdromDmaBlocksRemaining_;
        if (cdromDmaBlocksRemaining_ != 0u)
            cdromDmaWordsInBlock_ = cdromDmaBlockSize_;
    }

    const std::uint32_t visibleBlockCount = cdromDmaBlocksRemaining_ & 0xFFFFu;
    const std::uint32_t visibleBlockSize = cdromDmaWordsInBlock_ & 0xFFFFu;
    channel.blockControl = (visibleBlockCount << 16u) | visibleBlockSize;

    if (cdromDmaBlocksRemaining_ == 0u) {
        channel.channelControl &= ~(1u << 24u);
        dicrFlags_ |= (1u << (24u + kCdromChannel));
        cdromDmaActive_ = false;
    }
    return lastCdromTransferWords_ != 0u;
}

bool DmaController::executeGpuLinearTransfer(
    Ram& ram, Gpu& gpu, std::uint32_t& address, std::uint32_t words,
    bool toGpu, bool decrement) {
    constexpr std::uint32_t kMaxTransferWords = 1u << 20;
    if (words > kMaxTransferWords) return false;
    for (std::uint32_t i = 0; i < words; ++i) {
        std::uint32_t value = 0;
        if (toGpu) {
            if (!ram.read32(address, value)) return false;
            gpu.writeGp0(value);
        } else {
            value = gpu.readGp0();
            if (!ram.write32(address, value)) return false;
        }
        address = (address + (decrement ? 0x00FFFFFCu : 4u)) & 0x00FFFFFFu;
        ++lastTransferWords_;
    }
    return true;
}

bool DmaController::executeGpuLinkedList(
    Ram& ram, Gpu& gpu, std::uint32_t& address, bool toGpu) {
    if (!toGpu) return false;
    constexpr std::uint32_t kMaxHeaders = 1u << 16;
    constexpr std::uint32_t kMaxWords = 1u << 20;
    std::uint32_t headers = 0;
    while (address != 0x00FFFFFFu && headers++ < kMaxHeaders &&
           lastTransferWords_ < kMaxWords) {
        std::uint32_t header = 0;
        if (!ram.read32(address & 0x00FFFFFCu, header)) return false;
        const std::uint32_t count = header >> 24;
        const std::uint32_t next = header & 0x00FFFFFFu;
        std::uint32_t dataAddress = (address + 4u) & 0x00FFFFFFu;
        if (count > kMaxWords - lastTransferWords_) return false;
        if (!executeGpuLinearTransfer(ram, gpu, dataAddress, count, true, false))
            return false;
        address = next;
    }
    return address == 0x00FFFFFFu && headers <= kMaxHeaders;
}

bool DmaController::executeGpuTransfer(Ram& ram, Gpu& gpu) {
    constexpr std::size_t kGpuChannel = 2;
    Channel& channel = channels_[kGpuChannel];
    lastTransferWords_ = 0;

    const std::uint32_t control = channel.channelControl;
    const bool started = (control & (1u << 24)) != 0;
    const bool enabled = (dpcr_ & (1u << (kGpuChannel * 4u + 3u))) != 0;
    if (!started || !enabled) return false;

    const bool toGpu = (control & 1u) != 0;
    const bool decrement = (control & 2u) != 0;
    const std::uint32_t sync = (control >> 9) & 3u;
    std::uint32_t address = channel.base & 0x00FFFFFCu;
    bool success = false;

    if (sync == 2u) {
        success = executeGpuLinkedList(ram, gpu, address, toGpu);
    } else if (sync == 0u) {
        // Manual mode is launched by both START and TRIGGER.
        if ((control & (1u << 28)) == 0) return false;
        const std::uint32_t words = (channel.blockControl & 0xFFFFu) == 0
            ? 0x10000u : (channel.blockControl & 0xFFFFu);
        success = executeGpuLinearTransfer(ram, gpu, address, words,
                                           toGpu, decrement);
    } else if (sync == 1u) {
        const std::uint32_t blockSize = (channel.blockControl & 0xFFFFu) == 0
            ? 0x10000u : (channel.blockControl & 0xFFFFu);
        const std::uint32_t blockCount = (channel.blockControl >> 16) == 0
            ? 0x10000u : (channel.blockControl >> 16);
        if (blockCount <= (1u << 20) / blockSize) {
            success = executeGpuLinearTransfer(ram, gpu, address,
                blockSize * blockCount, toGpu, decrement);
        }
    }

    if (!success) return false;
    channel.base = address & 0x00FFFFFFu;
    channel.channelControl &= ~((1u << 24) | (1u << 28));
    // Completion latches this channel's DICR flag; channel/master enables
    // determine whether the DMA controller asserts its IRQ output.
    dicrFlags_ |= (1u << (24u + kGpuChannel));
    return true;
}

} // namespace nexus::ps1
