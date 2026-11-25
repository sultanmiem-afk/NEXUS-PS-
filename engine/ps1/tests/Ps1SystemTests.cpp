#include "Ps1System.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

using namespace nexus::ps1;

namespace {
void putWord(Ram& ram, std::uint32_t address, std::uint32_t value) {
    assert(ram.write32(address, value));
}
void putBiosWord(std::vector<std::uint8_t>& bios, std::size_t offset, std::uint32_t value) {
    bios[offset] = static_cast<std::uint8_t>(value);
    bios[offset + 1u] = static_cast<std::uint8_t>(value >> 8u);
    bios[offset + 2u] = static_cast<std::uint8_t>(value >> 16u);
    bios[offset + 3u] = static_cast<std::uint8_t>(value >> 24u);
}
std::uint32_t iType(std::uint32_t opcode, std::uint32_t rs,
                    std::uint32_t rt, std::uint16_t immediate) {
    return (opcode << 26u) | (rs << 21u) | (rt << 16u) | immediate;
}
}

int main() {
    Ps1System system;
    system.reset(0x00000000u);

    // ADDIU r1, r0, 42; ADDIU r2, r1, 8
    putWord(system.memory().ram(), 0, 0x2401002Au);
    putWord(system.memory().ram(), 4, 0x24220008u);
    assert(system.step().cpu.status == CpuStepStatus::Executed);
    assert(system.cpu().state().gpr[1] == 42u);
    assert(system.step().cpu.status == CpuStepStatus::Executed);
    assert(system.cpu().state().gpr[2] == 50u);

    // The system derives 44.1 kHz SPU output frames from accumulated CPU cycles.
    system.reset(0x00000000u);
    for (unsigned i = 0; i < 800u; ++i)
        assert(system.step().cpu.status == CpuStepStatus::Executed);
    assert(system.queuedAudioSamples() >= 1u);
    Spu::StereoSample systemAudio{};
    assert(system.popAudioSample(systemAudio));

    // KSEG1 aliases route to the same interrupt, DMA, timer and CD-ROM registers.
    system.reset(0x00000000u);
    std::uint32_t ioWord = 0u;
    assert(system.memory().write32(0xBF801074u, 0x00000020u)); // I_MASK
    assert(system.memory().read32(0x1F801074u, ioWord) && ioWord == 0x20u);
    system.memory().interrupts().request(1u << 5);
    assert(system.memory().read32(0xBF801070u, ioWord) && (ioWord & 0x20u) != 0u);
    assert(system.memory().write32(0xBF8010F0u, 0x12345678u)); // DMA DPCR
    assert(system.memory().read32(0x1F8010F0u, ioWord) && ioWord == 0x12345678u);
    assert(system.memory().write16(0xBF801100u, 0x3456u)); // Timer 0 counter
    std::uint16_t ioHalf = 0u;
    assert(system.memory().read16(0x1F801100u, ioHalf) && ioHalf == 0x3456u);
    assert(system.memory().write8(0xBF801800u, 0u)); // CD-ROM index
    std::uint8_t ioByte = 0u;
    assert(system.memory().read8(0x1F801800u, ioByte) && ioByte == 0x18u);

    // BIOS early scratch/expansion accesses are benign: writes ignored, reads zero.
    for (const std::uint32_t address : {0x1F801000u, 0x1F801060u, 0xFFFE0130u, 0x1F802041u}) {
        assert(system.memory().write8(address, 0xA5u));
        assert(system.memory().write16(address, 0xA5A5u));
        assert(system.memory().write32(address, 0xA5A5A5A5u));
        std::uint8_t b = 0xFFu;
        std::uint16_t h = 0xFFFFu;
        std::uint32_t w = 0xFFFFFFFFu;
        assert(system.memory().read8(address, b) && b == 0u);
        assert(system.memory().read16(address, h) && h == 0u);
        assert(system.memory().read32(address, w) && w == 0u);
    }
    std::uint32_t ignoredWord = 0xFFFFFFFFu;
    assert(system.memory().read32(0x1F801044u, ignoredWord) && ignoredWord == 0u);

    // CPU KSEG1 store to GP0 is translated before GPU MMIO decode (no BusFault).
    system.reset(0x00000000u);
    putWord(system.memory().ram(), 0, 0x3C01BF80u); // LUI r1, BF80
    putWord(system.memory().ram(), 4, 0x34211810u); // ORI r1, r1, 1810
    putWord(system.memory().ram(), 8, 0x3C020200u);
    putWord(system.memory().ram(), 12, 0x344200FFu);
    putWord(system.memory().ram(), 16, 0xAC220000u);
    putWord(system.memory().ram(), 20, 0x24020000u);
    putWord(system.memory().ram(), 24, 0xAC220000u);
    putWord(system.memory().ram(), 28, 0x3C020001u);
    putWord(system.memory().ram(), 32, 0x34420001u);
    putWord(system.memory().ram(), 36, 0xAC220000u);
    for (int i = 0; i < 30; ++i) assert(system.step().cpu.status == CpuStepStatus::Executed);
    std::uint16_t ksegPixel = 0u;
    assert(system.vram().readPixel(0u, 0u, ksegPixel) && ksegPixel == 0x001Fu);

    // VBlank rising edge requests I_STAT bit 0 and is observable by the CPU system.
    system.reset(0x00000000u);
    system.gpu().writeGp1(0x07000800u); // VBlank interval: scanlines [0, 2).
    bool sawVblankIrq = false;
    for (unsigned i = 0; i < 4400u; ++i) {
        (void)system.step();
        if ((system.memory().interrupts().status() & 1u) != 0u) {
            sawVblankIrq = true;
            break;
        }
    }
    assert(sawVblankIrq);

    // CPU can write/read SPU voice registers through 16-bit MMIO.
    system.reset(0x00000000u);
    putWord(system.memory().ram(), 0, 0x3C011F80u); // LUI r1, 1F80
    putWord(system.memory().ram(), 4, 0x34211C00u); // ORI r1, r1, 1C00 (SPU)
    putWord(system.memory().ram(), 8, 0x34021234u); // ORI r2, r0, 1234
    putWord(system.memory().ram(), 12, iType(0x29u, 1u, 2u, 0u)); // SH r2, 0(r1)
    putWord(system.memory().ram(), 16, iType(0x25u, 1u, 3u, 0u)); // LHU r3, 0(r1)
    putWord(system.memory().ram(), 20, 0x00000000u); // NOP commits delayed LHU result
    for (int i = 0; i < 6; ++i) assert(system.step().cpu.status == CpuStepStatus::Executed);
    assert(system.cpu().state().gpr[3] == 0x1234u);
    std::uint16_t spuVoiceVolume = 0;
    assert(system.memory().spu().readRegister16(0u, spuVoiceVolume));
    assert(spuVoiceVolume == 0x1234u);

    // CPU SH/LHU accesses I_STAT, I_MASK and all three timer register banks.
    system.reset(0x00000000u);
    system.memory().interrupts().request(1u << 4);
    putWord(system.memory().ram(), 0, 0x3C011F80u);
    putWord(system.memory().ram(), 4, 0x34211070u); // I_STAT
    putWord(system.memory().ram(), 8, iType(0x25u, 1u, 2u, 0u)); // LHU I_STAT
    putWord(system.memory().ram(), 12, 0x3C031F80u);
    putWord(system.memory().ram(), 16, 0x34631074u); // I_MASK
    putWord(system.memory().ram(), 20, 0x34040010u);
    putWord(system.memory().ram(), 24, iType(0x29u, 3u, 4u, 0u)); // SH I_MASK
    putWord(system.memory().ram(), 28, iType(0x25u, 3u, 5u, 0u)); // LHU I_MASK
    putWord(system.memory().ram(), 32, 0x3C061F80u);
    putWord(system.memory().ram(), 36, 0x34C61108u); // Timer 0 target
    putWord(system.memory().ram(), 40, 0x34070033u);
    putWord(system.memory().ram(), 44, iType(0x29u, 6u, 7u, 0u)); // SH target
    putWord(system.memory().ram(), 48, iType(0x25u, 6u, 8u, 0u)); // LHU target
    putWord(system.memory().ram(), 52, 0x00000000u); // NOP commits delayed LHU result
    for (int i = 0; i < 14; ++i) assert(system.step().cpu.status == CpuStepStatus::Executed);
    assert(system.cpu().state().gpr[2] == (1u << 4));
    assert(system.cpu().state().gpr[5] == (1u << 4));
    assert(system.cpu().state().gpr[8] == 0x33u);
    // I_STAT halfword writes preserve ones and clear zeroes (write-zero-to-clear).
    assert(system.memory().write16(InterruptController::kStatusAddress, 0xFFFFu));
    assert((system.memory().interrupts().status() & (1u << 4)) != 0u);
    assert(system.memory().write16(InterruptController::kStatusAddress, 0u));
    assert((system.memory().interrupts().status() & (1u << 4)) == 0u);
    // All timer banks accept 16-bit counter/mode/target accesses.
    assert(system.memory().write16(Timer0::kCounterAddress, 0x11u));
    assert(system.memory().write16(Timer1::kCounterAddress, 0x22u));
    assert(system.memory().write16(Timer2::kCounterAddress, 0x33u));
    std::uint16_t timerValue = 0;
    assert(system.memory().read16(Timer0::kCounterAddress, timerValue) && timerValue == 0x11u);
    assert(system.memory().read16(Timer1::kCounterAddress, timerValue) && timerValue == 0x22u);
    assert(system.memory().read16(Timer2::kCounterAddress, timerValue) && timerValue == 0x33u);
    assert(system.memory().write16(Timer1::kTargetAddress, 0x44u));
    assert(system.memory().read16(Timer1::kTargetAddress, timerValue) && timerValue == 0x44u);
    assert(system.memory().write16(Timer2::kTargetAddress, 0x55u));
    assert(system.memory().read16(Timer2::kTargetAddress, timerValue) && timerValue == 0x55u);

    // CP0 reset enables BEV; exceptions use the bootstrap vector until software clears it.
    system.reset(0x00000000u);
    assert((system.cpu().cop0().read(Cop0::kStatus) & Cop0::kStatusBev) != 0u);
    system.memory().interrupts().request(1u << 0);
    assert(system.memory().write32(InterruptController::kMaskAddress, 1u << 0));
    system.cpu().cop0().write(Cop0::kStatus, Cop0::kStatusBev | 1u | (1u << 10));
    assert(system.step().cpu.status == CpuStepStatus::ExceptionTaken);
    assert(system.cpu().state().pc == 0xBFC00180u);

    // The CPU writes a complete GP0 fill-rectangle packet through MMIO.
    system.reset(0x00000000u);
    putWord(system.memory().ram(), 0, 0x3C011F80u); // LUI r1, 1F80
    putWord(system.memory().ram(), 4, 0x34211810u); // ORI r1, r1, 1810 (GP0)
    putWord(system.memory().ram(), 8, 0x3C020200u); // LUI r2, 0200
    putWord(system.memory().ram(), 12, 0x344200FFu); // ORI r2, r2, 00FF
    putWord(system.memory().ram(), 16, 0xAC220000u); // SW r2, 0(r1): GP0 fill command
    putWord(system.memory().ram(), 20, 0x24020000u); // ADDIU r2, r0, 0
    putWord(system.memory().ram(), 24, 0xAC220000u); // top-left (0,0)
    putWord(system.memory().ram(), 28, 0x3C020001u); // LUI r2, 1 (height in high half)
    putWord(system.memory().ram(), 32, 0x34420001u); // ORI r2, r2, 1 (width=1)
    putWord(system.memory().ram(), 36, 0xAC220000u); // dimensions
    // Three GP0 words now require about 30 CPU cycles at 10 cycles per word.
    for (int i = 0; i < 30; ++i)
        assert(system.step().cpu.status == CpuStepStatus::Executed);
    std::uint16_t pixel = 0;
    assert(system.vram().readPixel(0, 0, pixel));
    assert(pixel == 0x001Fu);

    // Interrupt MMIO updates I_STAT/I_MASK and drives the CP0 hardware pending bit.
    system.reset(0x00000000u);
    assert(system.memory().write32(InterruptController::kMaskAddress, 1u << 3));
    system.memory().interrupts().request(1u << 3);
    assert(system.memory().interrupts().active());
    // Enable global interrupts and the IP2 hardware interrupt mask in CP0.
    system.cpu().cop0().write(Cop0::kStatus, (1u << 0) | (1u << 10));
    const auto irqStep = system.step();
    assert(irqStep.cpu.status == CpuStepStatus::ExceptionTaken);
    assert(system.cpu().state().pc == 0x80000080u);
    assert((system.cpu().cop0().read(Cop0::kCause) & (1u << 10)) != 0u);
    assert((system.cpu().cop0().read(Cop0::kCause) & (0x1Fu << 2)) == 0u);
    assert(system.memory().write32(InterruptController::kStatusAddress, ~(1u << 3)));
    assert(!system.memory().interrupts().active());
    assert(system.step().cpu.status == CpuStepStatus::Executed);
    assert((system.cpu().cop0().read(Cop0::kCause) & (1u << 10)) == 0u);

    // DMA channel 2 completion raises DICR master IRQ and latches I_STAT DMA.
    system.reset(0x00000000u);
    system.gpu().writeGp0(0xA0000000u);
    system.gpu().writeGp0(0x00000000u);
    system.gpu().writeGp0(0x00010002u);
    assert(system.memory().ram().write32(0x100u, 0x22221111u));
    assert(system.memory().write32(InterruptController::kMaskAddress, 1u << 3));
    assert(system.memory().write32(DmaController::kDpcrAddress, 1u << 11));
    assert(system.memory().write32(DmaController::kDicrAddress, (1u << 23) | (1u << 18)));
    assert(system.memory().write32(DmaController::kBaseAddress + 0x20u, 0x100u));
    assert(system.memory().write32(DmaController::kBaseAddress + 0x24u, 1u));
    assert(system.memory().write32(DmaController::kBaseAddress + 0x28u, 0x11000001u));
    const auto dmaStep = system.step();
    assert(dmaStep.dmaTransferred && dmaStep.dmaWords == 1u);
    assert((system.memory().interrupts().status() & (1u << 3)) != 0u);
    assert((system.memory().dma().dicr() & 0x80000000u) != 0u);
    system.cpu().cop0().write(Cop0::kStatus, (1u << 0) | (1u << 10));
    assert(system.step().cpu.status == CpuStepStatus::ExceptionTaken);
    assert(system.cpu().state().pc == 0x80000080u);

    // CD-ROM request-mode DMA channel 3 transfers one sector through the system.
    system.reset(0x00000000u);
    std::vector<std::uint8_t> discSector(2048u, 0x5Au);
    assert(system.loadDiscImage(discSector));
    assert(system.memory().write32(DmaController::kDpcrAddress, 1u << 15));
    assert(system.memory().write32(DmaController::kDicrAddress, (1u << 23) | (1u << 19)));
    assert(system.memory().write32(DmaController::kBaseAddress + 0x30u, 0x1000u));
    assert(system.memory().write32(DmaController::kBaseAddress + 0x34u, 0x00010200u));
    assert(system.memory().write32(DmaController::kBaseAddress + 0x38u, 0x01000200u));
    assert(system.memory().write8(0x1F801800u, 0u));
    assert(system.memory().write8(0x1F801802u, 0x00u));
    assert(system.memory().write8(0x1F801802u, 0x02u));
    assert(system.memory().write8(0x1F801802u, 0x00u));
    assert(system.memory().write8(0x1F801801u, 0x02u));
    assert(system.memory().write8(0x1F801801u, 0x06u));
    system.memory().cdrom().advance(Cdrom::kCyclesPerSector);
    const auto cdDmaStep = system.step();
    assert(cdDmaStep.cdromDmaTransferred && cdDmaStep.cdromDmaWords == 512u);
    std::uint32_t cdWord = 0u;
    assert(system.memory().ram().read32(0x1000u, cdWord) && cdWord == 0x5A5A5A5Au);
    assert((system.memory().interrupts().status() & (1u << 3)) != 0u);
    assert((system.memory().dma().dicr() & (1u << 27)) != 0u);

    // Timer 0 advances from CPU cycle estimates and raises I_STAT source 4.
    system.reset(0x00000000u);
    assert(system.memory().write32(InterruptController::kMaskAddress, 1u << 4));
    assert(system.memory().write32(Timer0::kTargetAddress, 1u));
    assert(system.memory().write32(Timer0::kModeAddress, 1u << 4)); // IRQ at target
    putWord(system.memory().ram(), 0, 0x00000000u); // NOP: one modeled CPU cycle
    const auto timerStep = system.step();
    assert(timerStep.cpu.status == CpuStepStatus::Executed);
    assert(timerStep.timer0Irq);
    assert((system.memory().interrupts().status() & (1u << 4)) != 0u);
    system.cpu().cop0().write(Cop0::kStatus, (1u << 0) | (1u << 10));
    assert(system.step().cpu.status == CpuStepStatus::ExceptionTaken);
    assert(system.cpu().state().pc == 0x80000080u);

    // Timer 1 CPU-clock mode raises I_STAT source 5 and reaches the CPU IRQ path.
    system.reset(0x00000000u);
    assert(system.memory().write32(InterruptController::kMaskAddress, 1u << 5));
    assert(system.memory().write32(Timer1::kTargetAddress, 1u));
    assert(system.memory().write32(Timer1::kModeAddress, 1u << 4));
    putWord(system.memory().ram(), 0, 0x00000000u);
    const auto timer1Step = system.step();
    assert(timer1Step.cpu.status == CpuStepStatus::Executed);
    assert(timer1Step.timer1Irq);
    assert((system.memory().interrupts().status() & (1u << 5)) != 0u);
    system.cpu().cop0().write(Cop0::kStatus, (1u << 0) | (1u << 10));
    assert(system.step().cpu.status == CpuStepStatus::ExceptionTaken);
    assert(system.cpu().state().pc == 0x80000080u);

    // Timer 2 CPU-clock mode raises I_STAT source 6 and reaches the CPU IRQ path.
    system.reset(0x00000000u);
    assert(system.memory().write32(InterruptController::kMaskAddress, 1u << 6));
    assert(system.memory().write32(Timer2::kTargetAddress, 1u));
    assert(system.memory().write32(Timer2::kModeAddress, 1u << 4));
    putWord(system.memory().ram(), 0, 0x00000000u);
    const auto timer2Step = system.step();
    assert(timer2Step.cpu.status == CpuStepStatus::Executed);
    assert(timer2Step.timer2Irq);
    assert((system.memory().interrupts().status() & (1u << 6)) != 0u);
    system.cpu().cop0().write(Cop0::kStatus, (1u << 0) | (1u << 10));
    assert(system.step().cpu.status == CpuStepStatus::ExceptionTaken);
    assert(system.cpu().state().pc == 0x80000080u);

    // GPU-generated dot-clock and HBlank events feed Timer 0 and Timer 1.
    system.reset(0x00000000u);
    assert(system.memory().write32(Timer0::kTargetAddress, 1u));
    assert(system.memory().write32(Timer0::kModeAddress, (1u << 8) | (1u << 4)));
    assert(system.memory().write32(Timer1::kTargetAddress, 1u));
    assert(system.memory().write32(Timer1::kModeAddress, (1u << 8) | (1u << 4)));
    bool sawTimer0GpuIrq = false;
    bool sawTimer1GpuIrq = false;
    for (std::uint32_t i = 0; i < 2172u; ++i) {
        const auto timedStep = system.step();
        sawTimer0GpuIrq = sawTimer0GpuIrq || timedStep.timer0Irq;
        sawTimer1GpuIrq = sawTimer1GpuIrq || timedStep.timer1Irq;
    }
    assert(sawTimer0GpuIrq);
    assert(sawTimer1GpuIrq);
    assert((system.memory().interrupts().status() & (1u << 4)) != 0u);
    assert((system.memory().interrupts().status() & (1u << 5)) != 0u);

    // Synthetic BIOS smoke test: MIPS code issues Setloc and ReadN against a CUE/BIN test disc.
    system.reset();
    std::vector<std::uint8_t> testDisc(151u * 2048u, 0u);
    for (std::size_t i = 0; i < 2048u; ++i)
        testDisc[150u * 2048u + i] = static_cast<std::uint8_t>(0x80u + (i & 0x7Fu));
    const std::string testCue =
        "FILE \"bios-test.bin\" BINARY\n"
        " TRACK 01 MODE1/2048\n"
        " INDEX 01 00:02:00\n";
    assert(system.loadDiscCue(testCue, "bios-test.bin", testDisc));
    std::vector<std::uint8_t> syntheticBios(BiosRom::kSize, 0u);
    const std::uint32_t biosProgram[] = {
        0x3C011F80u,                         // LUI r1, 1F80
        0x34211800u,                         // ORI r1, r1, 1800 (CD-ROM ports)
        iType(0x09u, 0u, 2u, 0x0000u),        // ADDIU r2, r0, 0
        iType(0x28u, 1u, 2u, 0x0002u),        // SB r2, 2(r1): minute
        iType(0x09u, 0u, 2u, 0x0002u),        // ADDIU r2, r0, 2
        iType(0x28u, 1u, 2u, 0x0002u),        // SB r2, 2(r1): second
        iType(0x09u, 0u, 2u, 0x0000u),        // ADDIU r2, r0, 0
        iType(0x28u, 1u, 2u, 0x0002u),        // SB r2, 2(r1): frame
        iType(0x09u, 0u, 2u, 0x0002u),        // ADDIU r2, r0, 2
        iType(0x28u, 1u, 2u, 0x0001u),        // SB r2, 1(r1): Setloc
        iType(0x09u, 0u, 2u, 0x0006u),        // ADDIU r2, r0, 6
        iType(0x28u, 1u, 2u, 0x0001u)         // SB r2, 1(r1): ReadN
    };
    for (std::size_t i = 0; i < sizeof(biosProgram) / sizeof(biosProgram[0]); ++i)
        putBiosWord(syntheticBios, i * 4u, biosProgram[i]);
    assert(system.loadBios(syntheticBios));
    system.reset(); // Reset vector now fetches the synthetic BIOS program.
    assert(system.memory().write32(InterruptController::kMaskAddress, 1u << 2));
    assert(system.memory().write8(0x1F801800u, 1u)); // Select CD-ROM IRQ enable register.
    assert(system.memory().write8(0x1F801801u, 0x04u)); // Enable command-complete IRQ.
    assert(system.memory().write8(0x1F801800u, 0u));
    for (std::size_t i = 0; i < sizeof(biosProgram) / sizeof(biosProgram[0]); ++i)
        assert(system.step().cpu.status == CpuStepStatus::Executed);
    assert(system.memory().cdrom().reading());
    system.memory().cdrom().advance(Cdrom::kCyclesPerSector);
    assert(system.memory().cdrom().dataAvailableBytes() == 2048u);
    assert((system.memory().interrupts().status() & (1u << 2)) != 0u);
    system.cpu().cop0().write(Cop0::kStatus, (1u << 0) | (1u << 10));
    assert(system.step().cpu.status == CpuStepStatus::ExceptionTaken);
    assert(system.cpu().state().pc == 0x80000080u);
    std::uint8_t bootSectorByte = 0u;
    assert(system.memory().read8(0x1F801802u, bootSectorByte));
    assert(bootSectorByte == 0x80u);

    // BIOS loading validates exact image size and remains loaded across reset.
    std::vector<std::uint8_t> bios(BiosRom::kSize, 0);
    assert(system.loadBios(bios));
    assert(system.memory().bios().loaded());
    assert(!system.loadBios(std::vector<std::uint8_t>(32, 0)));
    system.reset(0x00000000u);
    assert(system.memory().bios().loaded());

    // Full CPU/RAM/GPU/DMA integration through KSEG1 MMIO.
    // The CPU writes BIOS probe registers, submits a GP0 fill through the GPU FIFO,
    // programs GP1 display timing, and continuously samples GPUSTAT via GP1.
    system.reset(0x00000000u);
    std::vector<std::uint32_t> integrationProgram;
    auto emit = [&](std::uint32_t instruction) { integrationProgram.push_back(instruction); };
    emit(0x3C01BF80u); // LUI r1, BF80: KSEG1 MMIO base.
    emit(0x34211000u); // ORI r1, r1, 1000.
    emit(0x3C02A000u); // LUI r2, A000: poison-looking value for BIOS probes.
    for (std::uint16_t offset = 0; offset <= 0x20u; offset += 4u)
        emit(iType(0x2Bu, 1u, 2u, offset)); // SW r2, offset(r1): ignored, no BusFault.
    emit(0x34211810u); // r1 = BF801810 (GP0).
    emit(0x3C020200u); // GP0 fill command 020000FF.
    emit(0x344200FFu);
    emit(iType(0x2Bu, 1u, 2u, 0u));
    emit(0x24020000u); // XY=(0,0).
    emit(iType(0x2Bu, 1u, 2u, 0u));
    emit(0x3C020001u); // 1x1 dimensions.
    emit(0x34420001u);
    emit(iType(0x2Bu, 1u, 2u, 0u));
    emit(0x34210004u); // r1 = BF801814 (GPUSTAT/GP1).
    emit(0x3C020800u); // GP1(08): display mode 45h.
    emit(0x3442007Fu);
    emit(iType(0x2Bu, 1u, 2u, 0u));
    emit(0x3C020700u); // GP1(07): VBlank interval [0,2).
    emit(0x34420800u);
    emit(iType(0x2Bu, 1u, 2u, 0u));
    emit(iType(0x23u, 1u, 4u, 0u)); // LW r4, 0(r1): CPU reads GPUSTAT.
    emit(0u);                       // MIPS load delay slot.
    emit(iType(0x2Bu, 0u, 4u, 0x0200u)); // Save initial status.
    emit(0u);
    const std::size_t statusLoop = integrationProgram.size();
    emit(iType(0x23u, 1u, 4u, 0u)); // Repeated CPU GPUSTAT read via KSEG1.
    emit(0u);
    emit(iType(0x2Bu, 0u, 4u, 0x0204u)); // Store latest status snapshot in RAM.
    emit((0x02u << 26u) | static_cast<std::uint32_t>(statusLoop)); // J statusLoop.
    emit(0u); // Jump delay slot.
    for (std::size_t i = 0; i < integrationProgram.size(); ++i)
        putWord(system.memory().ram(), static_cast<std::uint32_t>(i * 4u), integrationProgram[i]);

    for (std::size_t i = 0; i < integrationProgram.size() + 6u; ++i)
        assert(system.step().cpu.status == CpuStepStatus::Executed);
    assert(system.gpu().gp0FifoSize() == 0u);
    std::uint16_t integratedPixel = 0u;
    assert(system.vram().readPixel(0u, 0u, integratedPixel) && integratedPixel == 0x001Fu);
    std::uint32_t initialGpuStatus = 0u;
    assert(system.memory().ram().read32(0x0200u, initialGpuStatus));
    assert(((initialGpuStatus >> 16u) & 0x7Fu) == 0x7Fu); // GPUSTAT bits 16-22.
    assert((initialGpuStatus & (1u << 31u)) == 0u);

    // VBlank I_STAT bit 0 must pulse once per frame; GPUSTAT bit 31 alternates
    // at each frame boundary. The CPU itself reads GPUSTAT in its RAM-storing loop.
    std::uint32_t observedVblankFrames = 0u;
    for (std::uint64_t step = 0; step < 2000000u && observedVblankFrames < 3u; ++step) {
        assert(system.step().cpu.status == CpuStepStatus::Executed);
        std::uint32_t interruptStatus = 0u;
        assert(system.memory().read32(InterruptController::kStatusAddress, interruptStatus));
        if ((interruptStatus & 1u) == 0u) continue;
        ++observedVblankFrames;
        for (unsigned settle = 0; settle < 8u; ++settle)
            assert(system.step().cpu.status == CpuStepStatus::Executed);
        std::uint32_t sampledStatus = 0u;
        assert(system.memory().ram().read32(0x0204u, sampledStatus));
        assert(((sampledStatus >> 16u) & 0x7Fu) == 0x7Fu);
        const bool expectedOddField = observedVblankFrames == 2u;
        assert(((sampledStatus >> 31u) & 1u) == (expectedOddField ? 1u : 0u));
        assert(system.memory().write32(InterruptController::kStatusAddress, ~1u));
        assert(system.memory().read32(InterruptController::kStatusAddress, interruptStatus));
        assert((interruptStatus & 1u) == 0u);
    }
    assert(observedVblankFrames == 3u);

    // DMA channel 2: RAM -> GP0 -> VRAM. A000 in the pixel payload is data,
    // not a command; FIFO processing must preserve the complete image packet.
    const std::uint32_t uploadWords[] = {
        0xA0000000u,              // GP0(A0) image upload command.
        400u | (400u << 16u),     // Destination.
        2u | (2u << 16u),         // 2x2 pixels.
        0xA0001111u,              // Poison-looking pixel payload.
        0x22223333u
    };
    for (std::size_t i = 0; i < 5u; ++i)
        assert(system.memory().ram().write32(0x1000u + static_cast<std::uint32_t>(i * 4u),
                                             uploadWords[i]));
    assert(system.memory().write32(0xBF8010F0u, 1u << 11u)); // Enable GPU DMA.
    assert(system.memory().write32(0xBF8010F4u, (1u << 23u) | (1u << 18u)));
    assert(system.memory().write32(0xBF8010A0u, 0x1000u)); // Channel 2 MADR.
    assert(system.memory().write32(0xBF8010A4u, 5u));      // Manual word count.
    assert(system.memory().write32(0xBF8010A8u, 0x11000001u)); // To GPU + START + TRIGGER.
    const auto integratedUpload = system.step();
    assert(integratedUpload.cpu.status == CpuStepStatus::Executed);
    assert(integratedUpload.dmaTransferred && integratedUpload.dmaWords == 5u);
    assert(system.vram().readPixel(400u, 400u, integratedPixel) && integratedPixel == 0x1111u);
    assert(system.vram().readPixel(401u, 400u, integratedPixel) && integratedPixel == 0xA000u);
    assert(system.vram().readPixel(400u, 401u, integratedPixel) && integratedPixel == 0x3333u);
    assert(system.vram().readPixel(401u, 401u, integratedPixel) && integratedPixel == 0x2222u);

    // GPU -> RAM through DMA channel 2, using GP0(C0) readback and the same VRAM.
    system.gpu().writeGp0(0xC0000000u);
    system.gpu().writeGp0(400u | (400u << 16u));
    system.gpu().writeGp0(2u | (2u << 16u));
    assert(system.memory().write32(0xBF8010A0u, 0x1200u));
    assert(system.memory().write32(0xBF8010A4u, 2u));
    assert(system.memory().write32(0xBF8010A8u, 0x11000000u)); // From GPU + START + TRIGGER.
    const auto integratedReadback = system.step();
    assert(integratedReadback.cpu.status == CpuStepStatus::Executed);
    assert(integratedReadback.dmaTransferred && integratedReadback.dmaWords == 2u);
    std::uint32_t readbackWord = 0u;
    assert(system.memory().ram().read32(0x1200u, readbackWord) && readbackWord == 0xA0001111u);
    assert(system.memory().ram().read32(0x1204u, readbackWord) && readbackWord == 0x22223333u);

    // The CPU/GPU/DMA integration also latches the DMA interrupt source.
    std::uint32_t integratedIstat = 0u;
    assert(system.memory().read32(InterruptController::kStatusAddress, integratedIstat));
    assert((integratedIstat & (1u << 3u)) != 0u);

    return 0;
}
