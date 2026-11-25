#include "Ps1System.hpp"

namespace nexus::ps1 {
namespace {
constexpr std::uint32_t kGpuGp0 = 0x1F801810u;
constexpr std::uint32_t kGpuGp1 = 0x1F801814u;
std::uint32_t normalizeKseg1(std::uint32_t address) noexcept {
    return (address >= 0xA0000000u && address < 0xC0000000u)
        ? (address & 0x1FFFFFFFu) : address;
}
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
    audioSamplePhase_ = 0;
    cdromIrqLinePrevious_ = false;
    dmaIrqLinePrevious_ = false;
}

bool Ps1System::loadBios(const std::vector<std::uint8_t>& image) noexcept {
    return memory_.bios().load(image);
}

bool Ps1System::loadDiscImage(const std::vector<std::uint8_t>& image) noexcept {
    return memory_.cdrom().loadImage(image);
}

bool Ps1System::loadDiscCue(const std::string& cueText, const std::string& binFilename,
                            const std::vector<std::uint8_t>& binImage) noexcept {
    return memory_.cdrom().loadCue(cueText, binFilename, binImage);
}

SystemStepResult Ps1System::step() {
    SystemStepResult result{};
    cpu_.cop0().setHardwareInterruptPending(memory_.interrupts().active());
    result.cpu = cpu_.step();
    memory_.cdrom().advance(result.cpu.cycles);
    // Advance GPU clocks, then service only the GP0 FIFO words earned by this
    // step's modeled GPU time. This is a simplified throughput model, not exact
    // hardware bus arbitration; fractional service phase is retained by the GPU.
    const GpuTimingEvents gpuTiming = gpu_.advanceTiming(result.cpu.cycles);
    gpu_.serviceGp0Fifo(gpuTiming.gp0ServiceWords);
    memory_.timer0().setHBlankActive(gpuTiming.hBlankActive);
    memory_.timer1().setVBlankActive(gpuTiming.vBlankActive);
    if (gpuTiming.vBlankStart)
        memory_.interrupts().request(1u << 0); // I_STAT VBlank source
    result.timer0Irq = memory_.timer0().advance(result.cpu.cycles);
    result.timer1Irq = memory_.timer1().advance(result.cpu.cycles);
    result.timer2Irq = memory_.timer2().advance(result.cpu.cycles);

    // Feed GPU dot-clock and HBlank clock pulses into the corresponding timers.
    result.timer0Irq = memory_.timer0().advanceDotClockPulses(
        gpuTiming.dotClockTicks) || result.timer0Irq;
    result.timer1Irq = memory_.timer1().advanceHBlankPulses(
        gpuTiming.hBlankPulses) || result.timer1Irq;

    if (result.timer0Irq)
        memory_.interrupts().request(1u << 4); // I_STAT Timer 0 source
    if (result.timer1Irq)
        memory_.interrupts().request(1u << 5); // I_STAT Timer 1 source
    if (result.timer2Irq)
        memory_.interrupts().request(1u << 6); // I_STAT Timer 2 source
    const bool cdromIrqLine = memory_.cdrom().irqPending();
    if (cdromIrqLine && !cdromIrqLinePrevious_)
        memory_.interrupts().request(1u << 2); // CD-ROM IRQ rising edge
    cdromIrqLinePrevious_ = cdromIrqLine;
    result.cdromDmaTransferred = memory_.dma().executeCdromTransfer(
        memory_.ram(), memory_.cdrom());
    if (result.cdromDmaTransferred)
        result.cdromDmaWords = memory_.dma().lastCdromTransferWords();
    result.dmaTransferred = memory_.dma().executeGpuTransfer(memory_.ram(), gpu_);
    if (result.dmaTransferred)
        result.dmaWords = memory_.dma().lastTransferWords();
    const bool dmaIrqLine = memory_.dma().irqPending();
    if (dmaIrqLine && !dmaIrqLinePrevious_)
        memory_.interrupts().request(1u << 3); // DMA IRQ rising edge
    dmaIrqLinePrevious_ = dmaIrqLine;

    // 44.1 kHz sample clock derived from the modeled 33.8688 MHz PS1 CPU clock.
    // The fractional phase is retained so samples are emitted at a stable average rate.
    constexpr std::uint64_t kCpuClockHz = 33868800u;
    audioSamplePhase_ += static_cast<std::uint64_t>(result.cpu.cycles) * kAudioSampleRate;
    while (audioSamplePhase_ >= kCpuClockHz) {
        (void)memory_.spu().mixSample();
        audioSamplePhase_ -= kCpuClockHz;
    }
    return result;
}

bool Ps1System::SystemBus::read8(std::uint32_t address, std::uint8_t& value) {
    address = normalizeKseg1(address);
    return memory_.read8(address, value);
}

bool Ps1System::SystemBus::read16(std::uint32_t address, std::uint16_t& value) {
    address = normalizeKseg1(address);
    return memory_.read16(address, value);
}

bool Ps1System::SystemBus::read32(std::uint32_t address, std::uint32_t& value) {
    address = normalizeKseg1(address);
    if (address == kGpuGp0) { value = gpu_.readGp0(); return true; }
    if (address == kGpuGp1) { value = gpu_.readStatus(); return true; }
    return memory_.read32(address, value);
}

bool Ps1System::SystemBus::write8(std::uint32_t address, std::uint8_t value) {
    address = normalizeKseg1(address);
    return memory_.write8(address, value);
}

bool Ps1System::SystemBus::write16(std::uint32_t address, std::uint16_t value) {
    address = normalizeKseg1(address);
    return memory_.write16(address, value);
}

bool Ps1System::SystemBus::write32(std::uint32_t address, std::uint32_t value) {
    address = normalizeKseg1(address);
    if (address == kGpuGp0) return gpu_.enqueueGp0(value);
    if (address == kGpuGp1) { gpu_.writeGp1(value); return true; }
    return memory_.write32(address, value);
}

} // namespace nexus::ps1
