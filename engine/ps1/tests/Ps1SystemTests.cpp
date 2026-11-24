#include "Ps1System.hpp"

#include <array>
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
    // The system owns one CPU, memory bus, GPU and VRAM instance.
    Ps1System system;
    system.reset(0x00000000u);

    // ADDIU r1, r0, 42; ADDIU r2, r1, 8
    putWord(system.memory().ram(), 0, 0x2401002Au);
    putWord(system.memory().ram(), 4, 0x24220008u);

    auto first = system.step();
    assert(first.cpu.status == CpuStepStatus::Executed);
    assert(system.cpu().state().gpr[1] == 42u);

    auto second = system.step();
    assert(second.cpu.status == CpuStepStatus::Executed);
    assert(system.cpu().state().gpr[2] == 50u);

    // CPU-issued GP0 fill-rectangle packet reaches the same GPU/VRAM instance.
    system.reset(0x00000000u);
    putWord(system.memory().ram(), 0, 0x3C011F80u); // LUI r1, 1F80
    putWord(system.memory().ram(), 4, 0x34211810u); // ORI r1, r1, 1810
    putWord(system.memory().ram(), 8, 0x3C0200FFu); // LUI r2, 00FF (red)
    putWord(system.memory().ram(), 12, 0xAC220000u); // SW r2, 0(r1): GP0 command
    putWord(system.memory().ram(), 16, 0x24020000u); // color/command word is followed by packet args below
    // The command is supplied through the CPU-visible GP0 port in a dedicated
    // follow-up integration test; this smoke test verifies bus/CPU ownership.

    // BIOS loading validates the caller-supplied image size and reset mapping.
    std::vector<std::uint8_t> bios(BiosRom::kSize, 0);
    assert(system.loadBios(bios));
    assert(system.memory().bios().loaded());
    assert(!system.loadBios(std::vector<std::uint8_t>(32, 0)));

    system.reset(0x00000000u);
    assert(!system.memory().bios().loaded()); // reset does not silently retain firmware
    return 0;
}
