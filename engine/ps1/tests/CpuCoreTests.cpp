#include "CpuCore.hpp"

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
    return 0;
}
