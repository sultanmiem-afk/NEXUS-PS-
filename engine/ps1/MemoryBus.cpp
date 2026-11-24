#include "MemoryBus.hpp"

namespace nexus::ps1 {

void MemoryBus::clear() noexcept {
    ram_.clear();
    cdrom_.reset();
    scratchpad_.clear();
    dma_.reset();
    interrupts_.reset();
    timer0_.reset();
    timer1_.reset();
    timer2_.reset();
}

bool MemoryBus::read8(std::uint32_t address, std::uint8_t& value) {
    if (cdrom_.read8(address, value)) return true;
    if (ram_.read8(address, value)) return true;
    if (scratchpad_.read8(address, value)) return true;
    return bios_.read8(address, value);
}

bool MemoryBus::read16(std::uint32_t address, std::uint16_t& value) {
    if (ram_.read16(address, value)) return true;
    if (scratchpad_.read16(address, value)) return true;
    return bios_.read16(address, value);
}

bool MemoryBus::read32(std::uint32_t address, std::uint32_t& value) {
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
    if (cdrom_.write8(address, value)) return true;
    if (ram_.write8(address, value)) return true;
    return scratchpad_.write8(address, value);
}

bool MemoryBus::write16(std::uint32_t address, std::uint16_t value) {
    if (ram_.write16(address, value)) return true;
    return scratchpad_.write16(address, value);
}

bool MemoryBus::write32(std::uint32_t address, std::uint32_t value) {
    if (ram_.write32(address, value)) return true;
    if (scratchpad_.write32(address, value)) return true;
    if (dma_.write32(address, value)) return true;
    if (interrupts_.write32(address, value)) return true;
    if (timer0_.write32(address, value)) return true;
    if (timer1_.write32(address, value)) return true;
    return timer2_.write32(address, value);
}

} // namespace nexus::ps1
