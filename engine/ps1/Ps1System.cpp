#include "Ps1System.hpp"

namespace nexus::ps1 {
namespace {
constexpr std::uint32_t kGpuGp0 = 0x1F801810u;
constexpr std::uint32_t kGpuGp1 = 0x1F801814u;
}

Ps1System::Ps1System() noexcept
    : gpu_(vram_), bus_(memory_, gpu_), cpu_(bus_) {
    reset();
}

void Ps1System::reset(std::uint32_t resetVector) noexcept {
    memory_.clear();
    vram_.clear();
    gpu_.reset();
    cpu_.reset(resetVector);
}

bool Ps1System::loadBios(const std::vector<std::uint8_t>& image) noexcept {
    return memory_.bios().load(image);
}

SystemStepResult Ps1System::step() {
    SystemStepResult result{};
    result.cpu = cpu_.step();
    result.dmaTransferred = memory_.dma().executeGpuTransfer(memory_.ram(), gpu_);
    if (result.dmaTransferred)
        result.dmaWords = memory_.dma().lastTransferWords();
    return result;
}

bool Ps1System::SystemBus::read8(std::uint32_t address, std::uint8_t& value) {
    return memory_.read8(address, value);
}

bool Ps1System::SystemBus::read16(std::uint32_t address, std::uint16_t& value) {
    return memory_.read16(address, value);
}

bool Ps1System::SystemBus::read32(std::uint32_t address, std::uint32_t& value) {
    if (address == kGpuGp0) { value = gpu_.readGp0(); return true; }
    if (address == kGpuGp1) { value = gpu_.readStatus(); return true; }
    return memory_.read32(address, value);
}

bool Ps1System::SystemBus::write8(std::uint32_t address, std::uint8_t value) {
    return memory_.write8(address, value);
}

bool Ps1System::SystemBus::write16(std::uint32_t address, std::uint16_t value) {
    return memory_.write16(address, value);
}

bool Ps1System::SystemBus::write32(std::uint32_t address, std::uint32_t value) {
    if (address == kGpuGp0) { gpu_.writeGp0(value); return true; }
    if (address == kGpuGp1) { gpu_.writeGp1(value); return true; }
    return memory_.write32(address, value);
}

} // namespace nexus::ps1
