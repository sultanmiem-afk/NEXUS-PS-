#include "Ps1System.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

using namespace nexus::ps1;

namespace {
void putWord(Ram& ram, std::uint32_t address, std::uint32_t value) {
    assert(ram.write32(address, value));
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
    for (int i = 0; i < 10; ++i)
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

    // BIOS loading validates exact image size and remains loaded across reset.
    std::vector<std::uint8_t> bios(BiosRom::kSize, 0);
    assert(system.loadBios(bios));
    assert(system.memory().bios().loaded());
    assert(!system.loadBios(std::vector<std::uint8_t>(32, 0)));
    system.reset(0x00000000u);
    assert(system.memory().bios().loaded());
    return 0;
}
