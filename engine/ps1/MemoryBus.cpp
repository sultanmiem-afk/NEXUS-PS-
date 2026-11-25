#include "MemoryBus.hpp"

namespace nexus::ps1 {
namespace {
std::uint32_t normalizeKseg1(std::uint32_t address) noexcept {
    return (address >= 0xA0000000u && address < 0xC0000000u)
        ? (address & 0x1FFFFFFFu) : address;
}
bool ignoredWriteAddress(std::uint32_t address) noexcept {
    if (address >= 0x1F801004u && address <= 0x1F801020u) return true;
    return address == 0x1F801000u || address == 0x1F801060u ||
           address == 0xFFFE0130u || address == 0x1F802041u ||
           address == 0x1F802000u ||
           address == 0x1F80105Au ||
           address == 0x1F801820u || address == 0x1F801824u;
}
bool ignoredReadAddress(std::uint32_t address) noexcept {
    return ignoredWriteAddress(address);
}
}

void MemoryBus::clear() noexcept {
    ram_.clear();
    cdrom_.reset();
    controller_.reset();
    spu_.reset();
    scratchpad_.clear();
    dma_.reset();
    interrupts_.reset();
    timer0_.reset();
    timer1_.reset();
    timer2_.reset();
}

bool MemoryBus::read8(std::uint32_t address, std::uint8_t& value) {
    address = normalizeKseg1(address);
    if (ignoredReadAddress(address)) { value = 0; return true; }

    if (address == 0x1F801040u) { value = controller_.readData(); return true; }
    if (cdrom_.read8(address, value)) return true;
    if (ram_.read8(address, value)) return true;
    if (scratchpad_.read8(address, value)) return true;
    return bios_.read8(address, value);
}

bool MemoryBus::read16(std::uint32_t address, std::uint16_t& value) {
    address = normalizeKseg1(address);
    if (ignoredReadAddress(address)) { value = 0; return true; }

    if (address == 0x1F801048u) { value = controller_.readMode(); return true; }
    if (address == 0x1F80104Au) { value = controller_.readControl(); return true; }
    if (address == 0x1F80104Eu) { value = controller_.readBaud(); return true; }
    if (address >= 0x1F801C00u && address < 0x1F801E00u)
        return spu_.readRegister16(address - 0x1F801C00u, value);
    if (address == InterruptController::kStatusAddress || address == InterruptController::kMaskAddress) {
        std::uint32_t word = 0;
        if (!interrupts_.read32(address, word)) return false;
        value = static_cast<std::uint16_t>(word);
        return true;
    }
    for (const auto base : {Timer0::kCounterAddress, Timer0::kModeAddress, Timer0::kTargetAddress,
                            Timer1::kCounterAddress, Timer1::kModeAddress, Timer1::kTargetAddress,
                            Timer2::kCounterAddress, Timer2::kModeAddress, Timer2::kTargetAddress}) {
        if (address == base) {
            std::uint32_t word = 0;
            bool ok = timer0_.read32(base, word) || timer1_.read32(base, word) || timer2_.read32(base, word);
            if (!ok) return false;
            value = static_cast<std::uint16_t>(word);
            return true;
        }
    }
    if (ram_.read16(address, value)) return true;
    if (scratchpad_.read16(address, value)) return true;
    return bios_.read16(address, value);
}

bool MemoryBus::read32(std::uint32_t address, std::uint32_t& value) {
    address = normalizeKseg1(address);
    if (ignoredReadAddress(address)) { value = 0; return true; }

    if (address == 0x1F801044u) { value = controller_.readStatus(); return true; }
    if (ram_.read32(address, value)) return true;
    if (scratchpad_.read32(address, value)) return true;
    if (dma_.read32(address, value)) return true;
    if (interrupts_.read32(address, value)) return true;
    if (timer0_.read32(address, value)) return true;
    if (timer1_.read32(address, value)) return true;
    if (timer2_.read32(address, value)) return true;
    return bios_.read32(address, value);
}

bool MemoryBus::write8(std::uint32_t address, std::uint8_t value) {
    address = normalizeKseg1(address);
    if (ignoredWriteAddress(address)) return true;

    if (address == 0x1F801040u) { controller_.writeData(value); return true; }
    if (cdrom_.write8(address, value)) return true;
    if (ram_.write8(address, value)) return true;
    return scratchpad_.write8(address, value);
}

bool MemoryBus::write16(std::uint32_t address, std::uint16_t value) {
    address = normalizeKseg1(address);
    if (ignoredWriteAddress(address)) return true;

    if (address == 0x1F801048u) { controller_.writeMode(value); return true; }
    if (address == 0x1F80104Au) { controller_.writeControl(value); return true; }
    if (address == 0x1F80104Eu) { controller_.writeBaud(value); return true; }
    if (address >= 0x1F801C00u && address < 0x1F801E00u)
        return spu_.writeRegister16(address - 0x1F801C00u, value);
    if (address == InterruptController::kStatusAddress || address == InterruptController::kMaskAddress)
        return interrupts_.write32(address, value);
    for (const auto base : {Timer0::kCounterAddress, Timer0::kModeAddress, Timer0::kTargetAddress,
                            Timer1::kCounterAddress, Timer1::kModeAddress, Timer1::kTargetAddress,
                            Timer2::kCounterAddress, Timer2::kModeAddress, Timer2::kTargetAddress}) {
        if (address == base)
            return timer0_.write32(base, value) || timer1_.write32(base, value) || timer2_.write32(base, value);
    }
    if (ram_.write16(address, value)) return true;
    return scratchpad_.write16(address, value);
}

bool MemoryBus::write32(std::uint32_t address, std::uint32_t value) {
    address = normalizeKseg1(address);
    if (ignoredWriteAddress(address)) return true;

    if (ram_.write32(address, value)) return true;
    if (scratchpad_.write32(address, value)) return true;
    if (dma_.write32(address, value)) return true;
    if (interrupts_.write32(address, value)) return true;
    if (timer0_.write32(address, value)) return true;
    if (timer1_.write32(address, value)) return true;
    return timer2_.write32(address, value);
}

} // namespace nexus::ps1
