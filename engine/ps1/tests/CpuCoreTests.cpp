#include "CpuCore.hpp"
#include "Cop0.hpp"

#include <array>
#include <cassert>
#include <cstdint>

using namespace nexus::ps1;

class TestBus final : public CpuBus {
public:
    std::array<std::uint8_t, 4096> memory{};

    bool read8(std::uint32_t a, std::uint8_t& v) override {
        if (a >= memory.size()) return false;
        v = memory[a]; return true;
    }
    bool read16(std::uint32_t a, std::uint16_t& v) override {
        if (a + 1 >= memory.size()) return false;
        v = static_cast<std::uint16_t>(memory[a]) |
            (static_cast<std::uint16_t>(memory[a + 1]) << 8); return true;
    }
    bool read32(std::uint32_t a, std::uint32_t& v) override {
        if (a + 3 >= memory.size()) return false;
        v = static_cast<std::uint32_t>(memory[a]) |
            (static_cast<std::uint32_t>(memory[a + 1]) << 8) |
            (static_cast<std::uint32_t>(memory[a + 2]) << 16) |
            (static_cast<std::uint32_t>(memory[a + 3]) << 24); return true;
    }
    bool write8(std::uint32_t a, std::uint8_t v) override {
        if (a >= memory.size()) return false;
        memory[a] = v; return true;
    }
    bool write16(std::uint32_t a, std::uint16_t v) override {
        return write8(a, static_cast<std::uint8_t>(v)) &&
               write8(a + 1, static_cast<std::uint8_t>(v >> 8));
    }
    bool write32(std::uint32_t a, std::uint32_t v) override {
        return write16(a, static_cast<std::uint16_t>(v)) &&
               write16(a + 2, static_cast<std::uint16_t>(v >> 16));
    }
};

static std::uint32_t i_type(unsigned op, unsigned rs, unsigned rt, std::int16_t imm) {
    return (op << 26) | (rs << 21) | (rt << 16) |
           static_cast<std::uint16_t>(imm);
}
static void put(TestBus& bus, unsigned address, std::uint32_t word) {
    assert(bus.write32(address, word));
}
static std::uint32_t r_type(unsigned rs, unsigned rt, unsigned rd, unsigned sh, unsigned fn) {
    return (rs << 21) | (rt << 16) | (rd << 11) | (sh << 6) | fn;
}
static std::uint32_t j_type(unsigned op, unsigned target) {
    return (op << 26) | ((target >> 2) & 0x03FFFFFFu);
}

int main() {
    {
        TestBus bus;
        CpuCore cpu(bus);
        cpu.reset(0);
        put(bus, 0, i_type(0x09, 0, 1, 42)); // ADDIU r1, r0, 42
        assert(cpu.step().status == CpuStepStatus::Executed);
        assert(cpu.state().gpr[1] == 42);
        assert(cpu.state().gpr[0] == 0);
    }
    {
        TestBus bus;
        CpuCore cpu(bus);
        cpu.reset(0);
        cpu.state().gpr[1] = 0xFFFFFFFFu;
        put(bus, 0, i_type(0x01, 1, 0, 2)); // BLTZ r1, +2
        put(bus, 4, i_type(0x09, 0, 2, 7));  // delay slot
        put(bus, 12, i_type(0x09, 0, 3, 9)); // branch target
        assert(cpu.step().status == CpuStepStatus::Executed);
        assert(cpu.state().pc == 4);
        assert(cpu.step().status == CpuStepStatus::Executed);
        assert(cpu.state().gpr[2] == 7);
        assert(cpu.state().pc == 12);
        assert(cpu.step().status == CpuStepStatus::Executed);
        assert(cpu.state().gpr[3] == 9);
    }
    {
        TestBus bus;
        CpuCore cpu(bus);
        cpu.reset(0);
        cpu.state().gpr[1] = 1;
        put(bus, 0, i_type(0x01, 1, 1, 2)); // BGEZ r1, +2
        put(bus, 4, 0);
        assert(cpu.step().status == CpuStepStatus::Executed);
        assert(cpu.state().pc == 4);
        assert(cpu.state().next_pc == 12);
    }
    {
        TestBus bus;
        CpuCore cpu(bus);
        cpu.reset(0);
        cpu.state().gpr[1] = 0x100;
        cpu.state().gpr[2] = 7;
        assert(bus.write32(0x100, 0x12345678u));
        put(bus, 0, i_type(0x23, 1, 2, 0)); // LW r2, 0(r1)
        put(bus, 4, i_type(0x09, 2, 3, 1)); // ADDIU r3, r2, 1 (sees old r2)
        put(bus, 8, i_type(0x09, 2, 4, 0)); // ADDIU r4, r2, 0 (sees loaded value)
        assert(cpu.step().status == CpuStepStatus::Executed);
        assert(cpu.state().gpr[2] == 7); // load is still pending
        assert(cpu.step().status == CpuStepStatus::Executed);
        assert(cpu.state().gpr[3] == 8); // delay instruction read old r2
        assert(cpu.state().gpr[2] == 0x12345678u); // load commits after operand read
        assert(cpu.step().status == CpuStepStatus::Executed);
        assert(cpu.state().gpr[4] == 0x12345678u);
    }
    {
        TestBus bus;
        CpuCore cpu(bus);
        cpu.reset(0);
        cpu.state().gpr[1] = 0x100;
        assert(bus.write32(0x100, 0xAABBCCDDu));
        put(bus, 0, i_type(0x23, 1, 2, 0)); // LW r2, 0(r1)
        put(bus, 4, i_type(0x09, 0, 2, 9)); // ADDIU r2, r0, 9
        assert(cpu.step().status == CpuStepStatus::Executed);
        assert(cpu.step().status == CpuStepStatus::Executed);
        assert(cpu.state().gpr[2] == 9); // following instruction's write wins
    }

    // Register ALU and shift instruction coverage.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=0x80000001u; cpu.state().gpr[2]=3;
        put(bus,0,r_type(0,1,3,1,0x00)); put(bus,4,r_type(0,1,4,1,0x02));
        put(bus,8,r_type(0,1,5,1,0x03)); put(bus,12,r_type(2,1,6,0,0x04));
        put(bus,16,r_type(2,1,7,0,0x06)); put(bus,20,r_type(2,1,8,0,0x07));
        put(bus,24,r_type(1,2,9,0,0x24)); put(bus,28,r_type(1,2,10,0,0x25));
        put(bus,32,r_type(1,2,11,0,0x26)); put(bus,36,r_type(1,2,12,0,0x27));
        put(bus,40,r_type(1,2,13,0,0x2A)); put(bus,44,r_type(1,2,14,0,0x2B));
        for(int i=0;i<12;i++) assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[3]==2 && cpu.state().gpr[4]==0x40000000u);
        assert(cpu.state().gpr[5]==0xC0000000u && cpu.state().gpr[6]==8u);
        assert(cpu.state().gpr[7]==0x10000000u && cpu.state().gpr[8]==0xF0000000u);
        assert(cpu.state().gpr[9]==1 && cpu.state().gpr[10]==0x80000003u);
        assert(cpu.state().gpr[11]==0x80000002u && cpu.state().gpr[12]==0x7FFFFFFCu);
        assert(cpu.state().gpr[13]==1 && cpu.state().gpr[14]==0);
    }
    // Immediate arithmetic/logical instructions.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0); cpu.state().gpr[1]=10;
        put(bus,0,i_type(0x09,1,2,-3)); put(bus,4,i_type(0x0A,1,3,11));
        put(bus,8,i_type(0x0B,1,4,-1)); put(bus,12,i_type(0x0C,1,5,7));
        put(bus,16,i_type(0x0D,1,6,5)); put(bus,20,i_type(0x0E,1,7,3));
        put(bus,24,i_type(0x0F,0,8,0x1234));
        for(int i=0;i<7;i++) assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[2]==7 && cpu.state().gpr[3]==1 && cpu.state().gpr[4]==1);
        assert(cpu.state().gpr[5]==2 && cpu.state().gpr[6]==15 && cpu.state().gpr[7]==9);
        assert(cpu.state().gpr[8]==0x12340000u);
    }
    // Byte, halfword and word load/store opcodes.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=0x100; cpu.state().gpr[2]=0x12345678u;
        assert(bus.write32(0x100,0x80FF7F01u));
        put(bus,0,i_type(0x20,1,3,0)); put(bus,4,0); // LB + load delay slot
        put(bus,8,i_type(0x21,1,4,2)); put(bus,12,0); // LH
        put(bus,16,i_type(0x23,1,5,0)); put(bus,20,0); // LW
        put(bus,24,i_type(0x24,1,6,1)); put(bus,28,0); // LBU
        put(bus,32,i_type(0x25,1,7,0)); put(bus,36,0); // LHU
        put(bus,40,i_type(0x28,1,2,4)); // SB
        put(bus,44,i_type(0x29,1,2,6)); // SH
        put(bus,48,i_type(0x2B,1,2,8)); // SW
        for(int i=0;i<13;i++) assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[3]==1u);
        assert(cpu.state().gpr[4]==0xFFFF80FFu);
        assert(cpu.state().gpr[5]==0x80FF7F01u);
        assert(cpu.state().gpr[6]==0x7Fu && cpu.state().gpr[7]==0x7F01u);
        std::uint32_t stored=0; assert(bus.read32(0x108,stored)); assert(stored==0x12345678u);
    }
    // JAL target, link address, and delay-slot execution.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        put(bus,0,j_type(0x03,16)); put(bus,4,i_type(0x09,0,1,5));
        put(bus,16,i_type(0x09,0,2,6));
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[31]==8 && cpu.state().pc==4 && cpu.state().next_pc==16);
        assert(cpu.step().status==CpuStepStatus::Executed && cpu.state().gpr[1]==5);
        assert(cpu.step().status==CpuStepStatus::Executed && cpu.state().gpr[2]==6);
    }
    // HI/LO multiply/divide and transfer operations.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=6; cpu.state().gpr[2]=7;
        put(bus,0,r_type(1,2,0,0,0x18)); put(bus,4,r_type(0,0,3,0,0x12));
        put(bus,8,r_type(0,0,4,0,0x10)); put(bus,12,r_type(1,2,0,0,0x1A));
        put(bus,16,r_type(1,2,0,0,0x1B)); put(bus,20,r_type(1,0,0,0,0x11));
        put(bus,24,r_type(2,0,0,0,0x13)); put(bus,28,r_type(0,0,5,0,0x10));
        put(bus,32,r_type(0,0,6,0,0x12));
        for(int i=0;i<9;i++) assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[3]==42 && cpu.state().gpr[4]==0);
        assert(cpu.state().gpr[5]==6 && cpu.state().gpr[6]==7);
    }

    // Not-taken conditional branches still execute exactly one delay-slot instruction.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1] = 1;
        put(bus,0,i_type(0x04,1,0,3)); // BEQ r1, r0, +3 (not taken)
        put(bus,4,i_type(0x09,0,2,11)); // delay slot
        put(bus,8,i_type(0x09,0,3,13)); // sequential instruction
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().pc==4 && cpu.state().next_pc==8);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[2]==11 && cpu.state().pc==8);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[3]==13);
    }
    // Signed divide corner cases: overflow and division by zero.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=0x80000000u; cpu.state().gpr[2]=0xFFFFFFFFu;
        put(bus,0,r_type(1,2,0,0,0x1A)); // INT_MIN / -1
        put(bus,4,r_type(1,0,0,0,0x1A)); // INT_MIN / 0
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().lo==0x80000000u && cpu.state().hi==0u);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().lo==1u && cpu.state().hi==0x80000000u);
    }
    // Unsigned divide by zero follows the R3000A HI/LO result convention.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=123u; cpu.state().gpr[2]=0u;
        put(bus,0,r_type(1,2,0,0,0x1B));
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().lo==0xFFFFFFFFu && cpu.state().hi==123u);
    }


    // CP0 exception entry: EPC, Cause.ExcCode, BD, and normal exception vector.
    {
        Cop0 cp0;
        CpuState cpu;
        cp0.reset();
        cpu.reset(0);
        cp0.write(Cop0::kStatus, 0x00000015u);
        cp0.enter_exception(cpu, CpuException::Syscall, 0x00001000u, false);
        assert(cpu.pc == 0x80000080u && cpu.next_pc == 0x80000084u);
        assert(cp0.read(Cop0::kEpc) == 0x00001000u);
        assert(((cp0.read(Cop0::kCause) >> 2) & 0x1Fu) ==
               static_cast<std::uint32_t>(CpuException::Syscall));
        assert((cp0.read(Cop0::kCause) & Cop0::kCauseBranchDelay) == 0);
        assert((cp0.read(Cop0::kStatus) & 0x3Fu) == 0x00000014u);
    }
    // CP0 delay-slot exception records the branch PC and sets Cause.BD.
    {
        Cop0 cp0;
        CpuState cpu;
        cp0.reset();
        cpu.reset(0);
        cp0.enter_exception(cpu, CpuException::Breakpoint, 0x00002004u, true);
        assert(cp0.read(Cop0::kEpc) == 0x00002000u);
        assert((cp0.read(Cop0::kCause) & Cop0::kCauseBranchDelay) != 0);
        assert(((cp0.read(Cop0::kCause) >> 2) & 0x1Fu) ==
               static_cast<std::uint32_t>(CpuException::Breakpoint));
    }
    // BEV selects the bootstrap exception vector; address faults update BadVaddr.
    {
        Cop0 cp0;
        CpuState cpu;
        cp0.reset();
        cpu.reset(0);
        cp0.write(Cop0::kStatus, Cop0::kStatusBev);
        cp0.enter_exception(cpu, CpuException::AddressStore, 0x00003000u,
                            false, 0x00001234u);
        assert(cpu.pc == 0xBFC00180u && cpu.next_pc == 0xBFC00184u);
        assert(cp0.read(Cop0::kBadVaddr) == 0x00001234u);
    }
    // CP0 write masks and RFE restore the previous KU/IE mode stack.
    {
        Cop0 cp0;
        cp0.reset();
        cp0.write(Cop0::kCause, 0xFFFFFFFFu);
        assert(cp0.read(Cop0::kCause) == 0x00000300u);
        cp0.write(Cop0::kBadVaddr, 0x12345678u);
        assert(cp0.read(Cop0::kBadVaddr) == 0u);
        cp0.write(Cop0::kStatus, 0x00000015u);
        CpuState cpu;
        cpu.reset(0);
        cp0.enter_exception(cpu, CpuException::Syscall, 0x400u, false);
        cp0.return_from_exception();
        assert((cp0.read(Cop0::kStatus) & 0x3Fu) == 0x00000015u);
    }

    // CPU-level COP0 transfers and RFE decode use their architectural encodings.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.cop0().write(Cop0::kStatus, 0x12345678u);
        const std::uint32_t mfc0_status =
            (0x10u << 26) | (0x00u << 21) | (5u << 16) | (Cop0::kStatus << 11);
        put(bus, 0, mfc0_status);
        assert(cpu.step().status == CpuStepStatus::Executed);
        assert(cpu.state().gpr[5] == 0x12345678u);
    }
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[6] = 0x00000015u;
        const std::uint32_t mtc0_status =
            (0x10u << 26) | (0x04u << 21) | (6u << 16) | (Cop0::kStatus << 11);
        put(bus, 0, mtc0_status);
        assert(cpu.step().status == CpuStepStatus::Executed);
        assert(cpu.cop0().read(Cop0::kStatus) == 0x15u);
    }
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.cop0().write(Cop0::kStatus, 0x15u);
        put(bus, 0, 0x42000010u); // RFE
        assert(cpu.step().status == CpuStepStatus::Executed);
        assert(cpu.cop0().read(Cop0::kStatus) == 0x05u);
    }
    // Nonzero reserved fields in MFC0/MTC0/RFE encodings are RI exceptions.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        put(bus, 0, 0x40000001u); // MFC0 with a reserved low bit set
        const auto result = cpu.step();
        assert(result.status == CpuStepStatus::ExceptionTaken);
        assert(((cpu.cop0().read(Cop0::kCause) >> 2) & 0x1Fu) ==
               static_cast<std::uint32_t>(CpuException::ReservedInstruction));
    }
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        put(bus, 0, 0x42010010u); // RFE with nonzero reserved rt field
        const auto result = cpu.step();
        assert(result.status == CpuStepStatus::ExceptionTaken);
        assert(((cpu.cop0().read(Cop0::kCause) >> 2) & 0x1Fu) ==
               static_cast<std::uint32_t>(CpuException::ReservedInstruction));
    }

    // A syscall in a branch delay slot must report the branch PC and set Cause.BD.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        put(bus,0,i_type(0x04,0,0,2)); // BEQ r0, r0, +2
        put(bus,4,0x0000000Cu);       // SYSCALL in delay slot
        assert(cpu.step().status==CpuStepStatus::Executed);
        const auto result=cpu.step();
        assert(result.status==CpuStepStatus::ExceptionTaken);
        assert(cpu.cop0().read(Cop0::kEpc)==0u);
        assert((cpu.cop0().read(Cop0::kCause)&Cop0::kCauseBranchDelay)!=0);
        assert(((cpu.cop0().read(Cop0::kCause)>>2)&0x1Fu)==
               static_cast<std::uint32_t>(CpuException::Syscall));
    }
    // Misaligned halfword/word accesses raise address exceptions and update BadVaddr.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=0x101u;
        put(bus,0,i_type(0x21,1,2,0)); // LH from odd address
        const auto result=cpu.step();
        assert(result.status==CpuStepStatus::ExceptionTaken);
        assert(((cpu.cop0().read(Cop0::kCause)>>2)&0x1Fu)==
               static_cast<std::uint32_t>(CpuException::AddressLoad));
        assert(cpu.cop0().read(Cop0::kBadVaddr)==0x101u);
    }
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=0x102u; cpu.state().gpr[2]=0xAABBCCDDu;
        put(bus,0,i_type(0x2B,1,2,0)); // SW to non-word-aligned address
        const auto result=cpu.step();
        assert(result.status==CpuStepStatus::ExceptionTaken);
        assert(((cpu.cop0().read(Cop0::kCause)>>2)&0x1Fu)==
               static_cast<std::uint32_t>(CpuException::AddressStore));
        assert(cpu.cop0().read(Cop0::kBadVaddr)==0x102u);
    }

    // Untaken REGIMM, BLEZ and BGTZ branches still execute their delay slots.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=1u;
        put(bus,0,i_type(0x01,1,0,2)); // BLTZ r1, not taken
        put(bus,4,i_type(0x09,0,2,21));
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().next_pc==8u);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[2]==21u);
    }
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=1u;
        put(bus,0,i_type(0x06,1,0,2)); // BLEZ r1, not taken
        put(bus,4,i_type(0x09,0,2,22));
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().next_pc==8u);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[2]==22u);
    }
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=0u;
        put(bus,0,i_type(0x07,1,0,2)); // BGTZ r1, not taken
        put(bus,4,i_type(0x09,0,2,23));
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().next_pc==8u);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[2]==23u);
    }
    // JR and JALR execute one delay-slot instruction; JALR writes PC+8 to rd.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=16u;
        put(bus,0,r_type(1,0,0,0,0x08)); // JR r1
        put(bus,4,i_type(0x09,0,2,31));
        put(bus,16,i_type(0x09,0,3,32));
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().pc==4u && cpu.state().next_pc==16u);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[2]==31u);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[3]==32u);
    }
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=20u;
        put(bus,0,r_type(1,0,5,0,0x09)); // JALR r5, r1
        put(bus,4,i_type(0x09,0,2,41));
        put(bus,20,i_type(0x09,0,3,42));
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[5]==8u);
        assert(cpu.state().pc==4u && cpu.state().next_pc==20u);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[2]==41u);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[3]==42u);
    }

    // Signed ADD, SUB and ADDI overflow must trap without modifying the destination.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=0x7FFFFFFFu;
        cpu.state().gpr[2]=1u;
        cpu.state().gpr[3]=0x13579BDFu;
        put(bus,0,r_type(1,2,3,0,0x20)); // ADD r3, r1, r2
        const auto result=cpu.step();
        assert(result.status==CpuStepStatus::ExceptionTaken);
        assert(cpu.state().gpr[3]==0x13579BDFu);
        assert(cpu.cop0().read(Cop0::kEpc)==0u);
        assert(((cpu.cop0().read(Cop0::kCause)>>2)&0x1Fu)==
               static_cast<std::uint32_t>(CpuException::ArithmeticOverflow));
    }
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=0x80000000u;
        cpu.state().gpr[2]=1u;
        cpu.state().gpr[3]=0x2468ACE0u;
        put(bus,0,r_type(1,2,3,0,0x22)); // SUB r3, r1, r2
        const auto result=cpu.step();
        assert(result.status==CpuStepStatus::ExceptionTaken);
        assert(cpu.state().gpr[3]==0x2468ACE0u);
        assert(cpu.cop0().read(Cop0::kEpc)==0u);
        assert(((cpu.cop0().read(Cop0::kCause)>>2)&0x1Fu)==
               static_cast<std::uint32_t>(CpuException::ArithmeticOverflow));
    }
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=0x7FFFFFFFu;
        cpu.state().gpr[2]=0x55AA55AAu;
        put(bus,0,i_type(0x08,1,2,1)); // ADDI r2, r1, 1
        const auto result=cpu.step();
        assert(result.status==CpuStepStatus::ExceptionTaken);
        assert(cpu.state().gpr[2]==0x55AA55AAu);
        assert(cpu.cop0().read(Cop0::kEpc)==0u);
        assert(((cpu.cop0().read(Cop0::kCause)>>2)&0x1Fu)==
               static_cast<std::uint32_t>(CpuException::ArithmeticOverflow));
    }
    // Unsigned arithmetic wraps instead of raising signed-overflow exceptions.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=0xFFFFFFFFu; cpu.state().gpr[2]=1u;
        put(bus,0,r_type(1,2,3,0,0x21)); // ADDU
        put(bus,4,r_type(2,1,4,0,0x23)); // SUBU
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[3]==0u);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[4]==2u);
    }

    // LWL merges the addressed left portion for every byte offset.
    {
        const std::uint32_t memoryWord=0x11223344u;
        const std::uint32_t initial=0xAABBCCDDu;
        const std::uint32_t expected[4]={
            0x44BBCCDDu, 0x3344CCDDu, 0x223344DDu, 0x11223344u
        };
        for(unsigned offset=0;offset<4;++offset) {
            TestBus bus; CpuCore cpu(bus); cpu.reset(0);
            cpu.state().gpr[1]=0x100u+offset; cpu.state().gpr[2]=initial;
            assert(bus.write32(0x100u,memoryWord));
            put(bus,0,i_type(0x22,1,2,0)); // LWL r2, 0(r1)
            put(bus,4,0); // load delay
            assert(cpu.step().status==CpuStepStatus::Executed);
            assert(cpu.step().status==CpuStepStatus::Executed);
            assert(cpu.state().gpr[2]==expected[offset]);
        }
    }
    // LWR merges the addressed right portion for every byte offset.
    {
        const std::uint32_t memoryWord=0x11223344u;
        const std::uint32_t initial=0xAABBCCDDu;
        const std::uint32_t expected[4]={
            0x11223344u, 0xAA112233u, 0xAABB1122u, 0xAABBCC11u
        };
        for(unsigned offset=0;offset<4;++offset) {
            TestBus bus; CpuCore cpu(bus); cpu.reset(0);
            cpu.state().gpr[1]=0x100u+offset; cpu.state().gpr[2]=initial;
            assert(bus.write32(0x100u,memoryWord));
            put(bus,0,i_type(0x26,1,2,0)); // LWR r2, 0(r1)
            put(bus,4,0); // load delay
            assert(cpu.step().status==CpuStepStatus::Executed);
            assert(cpu.step().status==CpuStepStatus::Executed);
            assert(cpu.state().gpr[2]==expected[offset]);
        }
    }
    // The conventional LWL/LWR pair must merge into one unaligned 32-bit word.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=0x101u; // unaligned word starts at byte 1
        assert(bus.write32(0x100u,0x44332211u));
        assert(bus.write32(0x104u,0x88776655u));
        put(bus,0,i_type(0x22,1,2,3)); // LWL r2, 3(r1): byte 0x55 -> high byte
        put(bus,4,i_type(0x26,1,2,0)); // LWR r2, 0(r1): low 3 bytes
        put(bus,8,0);                  // normal load delay after pair
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[2]==0x55443322u);
    }

    // BLTZAL links only when its signed condition is true and always runs a delay slot.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=0xFFFFFFFFu;
        cpu.state().gpr[31]=0xDEADBEEFu;
        put(bus,0,i_type(0x01,1,0x10,3)); // BLTZAL r1, +3
        put(bus,4,i_type(0x09,0,2,51));   // delay slot
        put(bus,16,i_type(0x09,0,3,52));  // branch target
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[31]==8u);
        assert(cpu.state().pc==4u && cpu.state().next_pc==16u);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[2]==51u);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[3]==52u);
    }
    // A not-taken BLTZAL does not change the link register but still has a delay slot.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=1u;
        cpu.state().gpr[31]=0xCAFEBABEu;
        put(bus,0,i_type(0x01,1,0x10,3)); // BLTZAL r1, not taken
        put(bus,4,i_type(0x09,0,2,53));
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[31]==0xCAFEBABEu);
        assert(cpu.state().pc==4u && cpu.state().next_pc==8u);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[2]==53u);
    }
    // BGEZAL taken and not-taken cases preserve the same link/delay-slot rules.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=0u;
        put(bus,0,i_type(0x01,1,0x11,2)); // BGEZAL r1, +2
        put(bus,4,i_type(0x09,0,2,54));
        put(bus,12,i_type(0x09,0,3,55));
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[31]==8u && cpu.state().next_pc==12u);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[2]==54u);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[3]==55u);
    }
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=0x80000000u;
        cpu.state().gpr[31]=0x12345678u;
        put(bus,0,i_type(0x01,1,0x11,2)); // BGEZAL r1, not taken
        put(bus,4,i_type(0x09,0,2,56));
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[31]==0x12345678u && cpu.state().next_pc==8u);
        assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[2]==56u);
    }

    // Arithmetic right shifts are defined for all shift amounts, including 0 and 31.
    {
        TestBus bus; CpuCore cpu(bus); cpu.reset(0);
        cpu.state().gpr[1]=0x80000001u; cpu.state().gpr[5]=31u; cpu.state().gpr[7]=0u;
        put(bus,0,r_type(0,1,2,0,0x03));  // SRA r2, r1, 0
        put(bus,4,r_type(0,1,3,31,0x03)); // SRA r3, r1, 31
        put(bus,8,r_type(5,1,4,0,0x07));  // SRAV r4, r1, r5
        put(bus,12,r_type(7,1,6,0,0x07)); // SRAV r6, r1, r7
        for(int i=0;i<4;i++) assert(cpu.step().status==CpuStepStatus::Executed);
        assert(cpu.state().gpr[2]==0x80000001u);
        assert(cpu.state().gpr[3]==0xFFFFFFFFu);
        assert(cpu.state().gpr[4]==0xFFFFFFFFu);
        assert(cpu.state().gpr[6]==0x80000001u);
    }

    return 0;
}
