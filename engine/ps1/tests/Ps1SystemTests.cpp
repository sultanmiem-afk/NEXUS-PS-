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
    for (std::size_t i = 0; i < sizeof(biosProgram) / sizeof(biosProgram[0]); ++i)
        assert(system.step().cpu.status == CpuStepStatus::Executed);
    assert(system.memory().cdrom().reading());
    system.memory().cdrom().advance(Cdrom::kCyclesPerSector);
    assert(system.memory().cdrom().dataAvailableBytes() == 2048u);
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
    return 0;
}
