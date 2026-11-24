#include "Spu.hpp"

namespace nexus::ps1 {

void Spu::reset() noexcept {
    clearRam();
    registers_.fill(0);
    voices_.fill(VoiceState{});
    keyOnMask_ = 0;
    keyOffMask_ = 0;
}

void Spu::clearRam(std::uint8_t value) noexcept { ram_.fill(value); }

bool Spu::readRam8(std::uint32_t address, std::uint8_t& value) const noexcept {
    if (address >= kRamSize) return false;
    value = ram_[address];
    return true;
}

bool Spu::readRam16(std::uint32_t address, std::uint16_t& value) const noexcept {
    if ((address & 1u) != 0 || address > kRamSize - 2u) return false;
    value = static_cast<std::uint16_t>(ram_[address]) |
            (static_cast<std::uint16_t>(ram_[address + 1u]) << 8);
    return true;
}

bool Spu::readRam32(std::uint32_t address, std::uint32_t& value) const noexcept {
    if ((address & 3u) != 0 || address > kRamSize - 4u) return false;
    value = static_cast<std::uint32_t>(ram_[address]) |
            (static_cast<std::uint32_t>(ram_[address + 1u]) << 8) |
            (static_cast<std::uint32_t>(ram_[address + 2u]) << 16) |
            (static_cast<std::uint32_t>(ram_[address + 3u]) << 24);
    return true;
}

bool Spu::writeRam8(std::uint32_t address, std::uint8_t value) noexcept {
    if (address >= kRamSize) return false;
    ram_[address] = value;
    return true;
}

bool Spu::writeRam16(std::uint32_t address, std::uint16_t value) noexcept {
    if ((address & 1u) != 0 || address > kRamSize - 2u) return false;
    ram_[address] = static_cast<std::uint8_t>(value & 0xFFu);
    ram_[address + 1u] = static_cast<std::uint8_t>(value >> 8);
    return true;
}

bool Spu::writeRam32(std::uint32_t address, std::uint32_t value) noexcept {
    if ((address & 3u) != 0 || address > kRamSize - 4u) return false;
    ram_[address] = static_cast<std::uint8_t>(value & 0xFFu);
    ram_[address + 1u] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    ram_[address + 2u] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    ram_[address + 3u] = static_cast<std::uint8_t>(value >> 24);
    return true;
}

bool Spu::isMappedRegister(std::uint32_t offset) noexcept {
    if ((offset & 1u) != 0 || offset >= 0x200u) return false;
    if (offset < 0x180u) return true;
    return offset >= 0x180u && offset <= 0x1FEu;
}

bool Spu::voiceState(std::size_t index, VoiceState& value) const noexcept {
    if (index >= kVoiceCount) return false;
    value = voices_[index];
    return true;
}

void Spu::applyKeyMask(std::uint32_t mask, bool keyOn) noexcept {
    mask &= 0x00FFFFFFu;
    for (std::size_t i = 0; i < kVoiceCount; ++i) {
        if ((mask & (1u << i)) == 0) continue;
        voices_[i].keyedOn = keyOn;
        if (keyOn) voices_[i].currentAdsrVolume = 0;
    }
}

bool Spu::readRegister16(std::uint32_t offset, std::uint16_t& value) const noexcept {
    if (!isMappedRegister(offset)) return false;

    if (offset < 0x180u) {
        const std::size_t voiceIndex = offset / 0x10u;
        const std::uint32_t voiceOffset = offset & 0x0Fu;
        const VoiceState& v = voices_[voiceIndex];
        switch (voiceOffset) {
            case 0x0: value = v.volumeLeft; return true;
            case 0x2: value = v.volumeRight; return true;
            case 0x4: value = v.pitch; return true;
            case 0x6: value = v.startAddress; return true;
            case 0x8: value = v.adsrLow; return true;
            case 0xA: value = v.adsrHigh; return true;
            case 0xC: value = v.currentAdsrVolume; return true;
            case 0xE: value = v.repeatAddress; return true;
            default: return false;
        }
    }

    if (offset == 0x188u) { value = static_cast<std::uint16_t>(keyOnMask_); return true; }
    if (offset == 0x18Au) { value = static_cast<std::uint16_t>(keyOnMask_ >> 16); return true; }
    if (offset == 0x18Cu) { value = static_cast<std::uint16_t>(keyOffMask_); return true; }
    if (offset == 0x18Eu) { value = static_cast<std::uint16_t>(keyOffMask_ >> 16); return true; }

    value = registers_[offset / 2u];
    return true;
}

bool Spu::writeRegister16(std::uint32_t offset, std::uint16_t value) noexcept {
    if (!isMappedRegister(offset)) return false;

    if (offset < 0x180u) {
        const std::size_t voiceIndex = offset / 0x10u;
        const std::uint32_t voiceOffset = offset & 0x0Fu;
        VoiceState& v = voices_[voiceIndex];
        switch (voiceOffset) {
            case 0x0: v.volumeLeft = value; break;
            case 0x2: v.volumeRight = value; break;
            case 0x4: v.pitch = value; break;
            case 0x6: v.startAddress = value; break;
            case 0x8: v.adsrLow = value; break;
            case 0xA: v.adsrHigh = value; break;
            case 0xC: return false; // Current ADSR volume is read-only.
            case 0xE: v.repeatAddress = value; break;
            default: return false;
        }
        registers_[offset / 2u] = value;
        return true;
    }

    // Key-on and key-off are write-triggered command masks, not ordinary storage.
    if (offset == 0x188u || offset == 0x18Au) {
        const unsigned shift = offset == 0x18Au ? 16u : 0u;
        const std::uint32_t mask = static_cast<std::uint32_t>(value) << shift;
        keyOnMask_ = (keyOnMask_ & ~(0xFFFFu << shift)) | mask;
        applyKeyMask(mask, true);
        return true;
    }
    if (offset == 0x18Cu || offset == 0x18Eu) {
        const unsigned shift = offset == 0x18Eu ? 16u : 0u;
        const std::uint32_t mask = static_cast<std::uint32_t>(value) << shift;
        keyOffMask_ = (keyOffMask_ & ~(0xFFFFu << shift)) | mask;
        applyKeyMask(mask, false);
        return true;
    }

    // The SPU control register is at 0x1A8; transfer control is 0x1AA.
    registers_[offset / 2u] = value;
    return true;
}

} // namespace nexus::ps1
