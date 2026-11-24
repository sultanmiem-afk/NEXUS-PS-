#include "DmaController.hpp"

namespace nexus::ps1 {

void DmaController::reset() noexcept {
    channels_ = {};
    // PS1 power-on DPCR priorities/enables default to zero.
    dpcr_ = 0x07654321u;
    dicrControl_ = 0;
    dicrFlags_ = 0;
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
    const bool enabledFlag = (dicrControl_ & dicrFlags_ & 0x7F000000u) != 0;
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
        // Preserve writable control bits. Transfer start/clear semantics are
        // intentionally not simulated until the DMA execution engine exists.
        ch.channelControl = value & 0x71770703u;
    } else {
        return false;
    }
    return true;
}

} // namespace nexus::ps1
