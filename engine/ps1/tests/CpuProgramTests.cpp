#include "Ps1System.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

using namespace nexus::ps1;

namespace {
std::uint32_t iType(unsigned op, unsigned rs, unsigned rt, std::int16_t imm) {
    return (op << 26u) | (rs << 21u) | (rt << 16u) |
           static_cast<std::uint16_t>(imm);
}
std::uint32_t rType(unsigned rs, unsigned rt, unsigned rd, unsigned sh, unsigned fn) {
    return (rs << 21u) | (rt << 16u) | (rd << 11u) | (sh << 6u) | fn;
}
std::uint32_t jType(unsigned op, std::uint32_t target) {
    return (op << 26u) | ((target >> 2u) & 0x03FFFFFFu);
}
void loadProgram(Ps1System& system, const std::vector<std::uint32_t>& program,
                 std::uint32_t physicalBase = 0u) {
    for (std::size_t i = 0; i < program.size(); ++i)
        assert(system.memory().ram().write32(physicalBase +
            static_cast<std::uint32_t>(i * 4u), program[i]));
}
void run(Ps1System& system, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        assert(system.step().cpu.status == CpuStepStatus::Executed);
}
void assertException(Ps1System& system, const std::vector<std::uint32_t>& program,
                     CpuException expected) {
    system.reset(0u);
    loadProgram(system, program);
    const auto result = system.step().cpu;
    assert(result.status == CpuStepStatus::ExceptionTaken);
    assert(((system.cpu().cop0().read(Cop0::kCause) >> 2u) & 0x1Fu) ==
           static_cast<std::uint32_t>(expected));
}
}

int main() {
    Ps1System system;
    system.reset(0x80000000u); // Execute the program through the real KSEG0 bus mapping.

    // One connected MIPS program: KSEG1 store -> KSEG0 load, load delay,
    // ALU/shift, multiply/divide, conditional branch, jump delay slot,
    // scratchpad access, and COP0 read.
    const std::vector<std::uint32_t> program = {
        0x3C01A000u,                         // LUI   r1, 0xA000 (KSEG1)
        0x34210100u,                         // ORI   r1, r1, 0x0100
        iType(0x09, 0, 2, 0x1234),           // ADDIU r2, r0, 0x1234
        iType(0x2B, 1, 2, 0),                // SW    r2, 0(r1)
        0x3C018000u,                         // LUI   r1, 0x8000 (KSEG0)
        0x34210100u,                         // ORI   r1, r1, 0x0100
        iType(0x23, 1, 3, 0),                // LW    r3, 0(r1)
        0x00000000u,                         // NOP: load delay slot
        iType(0x09, 3, 4, 1),                // ADDIU r4, r3, 1
        iType(0x09, 0, 5, 3),                // ADDIU r5, r0, 3
        rType(0, 4, 6, 2, 0x00),             // SLL   r6, r4, 2
        rType(6, 2, 7, 0, 0x21),             // ADDU  r7, r6, r2
        rType(2, 5, 0, 0, 0x18),             // MULT  r2, r5
        rType(0, 0, 8, 0, 0x12),             // MFLO  r8
        rType(8, 5, 0, 0, 0x1B),             // DIVU  r8, r5
        rType(0, 0, 9, 0, 0x12),             // MFLO  r9
        iType(0x0A, 9, 10, 0x2000),          // SLTI  r10, r9, 0x2000
        iType(0x04, 10, 0, 2),               // BEQ   not taken; delay slot still runs
        iType(0x09, 0, 11, 0x55),            // delay slot
        iType(0x09, 0, 12, 0x66),            // sequential instruction
        jType(0x02, 0x80000060u),            // J     instruction 24
        iType(0x09, 0, 13, 0x77),            // jump delay slot
        iType(0x09, 0, 14, 0x99),            // skipped
        iType(0x09, 0, 15, 0xAA),            // skipped
        0x3C17BF80u,                         // LUI   r23, 0xBF80 (scratchpad KSEG1)
        iType(0x09, 0, 18, 0x5A5A),          // ADDIU r18, r0, 0x5A5A
        iType(0x2B, 23, 18, 0),              // SW    r18, 0(r23)
        iType(0x23, 23, 19, 0),              // LW    r19, 0(r23)
        0x00000000u,                         // NOP: load delay slot
        (0x10u << 26u) | (5u << 16u) | (Cop0::kStatus << 11u), // MFC0 r5, Status
        0x00000000u                          // NOP
    };
    loadProgram(system, program);
    run(system, 29u); // 22 instructions through the jump delay slot + 7 at the target

    const auto& cpu = system.cpu().state();
    assert(cpu.gpr[2] == 0x1234u);
    assert(cpu.gpr[3] == 0x1234u);
    assert(cpu.gpr[4] == 0x1235u);
    assert(cpu.gpr[6] == 0x48D4u);
    assert(cpu.gpr[7] == 0x5B08u);
    assert(cpu.gpr[8] == 0x369Cu);
    assert(cpu.gpr[9] == 0x1234u);
    assert(cpu.gpr[10] == 1u);
    assert(cpu.gpr[11] == 0x55u && cpu.gpr[12] == 0x66u);
    assert(cpu.gpr[13] == 0x77u);
    assert(cpu.gpr[14] == 0u && cpu.gpr[15] == 0u);
    assert(cpu.gpr[19] == 0x5A5Au);
    assert((cpu.gpr[5] & Cop0::kStatusBev) != 0u);
    std::uint32_t ramWord = 0;
    assert(system.memory().ram().read32(0x100u, ramWord) && ramWord == 0x1234u);
    std::uint32_t scratchWord = 0;
    assert(system.memory().scratchpad().read32(0x1F800000u, scratchWord) && scratchWord == 0x5A5Au);

    // Real instruction streams exercise exception entry from the CPU decoder.
    assertException(system, {0x0000000Cu}, CpuException::Syscall);
    assertException(system, {0x0000000Du}, CpuException::Breakpoint);
    assertException(system, {iType(0x08, 1, 2, 1)}, CpuException::ArithmeticOverflow);
    assertException(system, {0xFFFFFFFFu}, CpuException::ReservedInstruction);

    // Unmapped data read is a DBE from a running program, not a mocked bus callback.
    system.reset(0u);
    loadProgram(system, {0x3C01C000u, iType(0x23, 1, 2, 0)});
    assert(system.step().cpu.status == CpuStepStatus::Executed);
    assert(system.step().cpu.status == CpuStepStatus::ExceptionTaken);
    assert(((system.cpu().cop0().read(Cop0::kCause) >> 2u) & 0x1Fu) ==
           static_cast<std::uint32_t>(CpuException::DataBusError));

    return 0;
}
