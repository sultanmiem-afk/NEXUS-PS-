#include "CpuCore.hpp"

#include <limits>

namespace nexus::ps1 {
namespace {
constexpr std::uint32_t sign_extend8(std::uint8_t v) noexcept {
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(v)));
}
constexpr std::uint32_t sign_extend16(std::uint16_t v) noexcept {
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(v)));
}
}

void CpuCore::reset(std::uint32_t reset_vector) noexcept {
    state_.reset(reset_vector);
    in_delay_slot_ = false;
}

void CpuCore::write_reg(std::uint32_t index, std::uint32_t value) noexcept {
    if (index != 0) state_.gpr[index & 31u] = value;
    state_.gpr[0] = 0;
}

void CpuCore::branch(std::uint32_t target) noexcept {
    // state_.pc already points at the delay-slot instruction.
    state_.next_pc = target;
}

CpuStepResult CpuCore::step() {
    CpuStepResult result{};
    result.instruction_pc = state_.pc;

    if ((state_.pc & 3u) != 0) {
        result.status = CpuStepStatus::AddressError;
        return result;
    }

    std::uint32_t raw = 0;
    if (!bus_.read32(state_.pc, raw)) {
        result.status = CpuStepStatus::BusFault;
        return result;
    }
    result.instruction = raw;

    const Instruction ins(raw);
    const auto rs = state_.gpr[ins.rs()];
    const auto rt = state_.gpr[ins.rt()];
    const auto imm = ins.signed_immediate();
    const auto uimm = static_cast<std::uint32_t>(ins.immediate());
    const auto pc = state_.pc;
    const bool was_delay_slot = in_delay_slot_;

    // Advance first. A taken branch replaces next_pc, leaving exactly one
    // instruction at the sequential PC to execute as the branch delay slot.
    state_.pc = state_.next_pc;
    state_.next_pc += 4u;
    in_delay_slot_ = false;

    auto load_address = [&](std::uint32_t address, std::uint32_t& value) -> bool {
        if ((address & 3u) != 0) {
            result.status = CpuStepStatus::AddressError;
            return false;
        }
        if (!bus_.read32(address, value)) {
            result.status = CpuStepStatus::BusFault;
            return false;
        }
        return true;
    };

    switch (ins.opcode()) {
    case 0x00: // SPECIAL
        switch (ins.function()) {
        case 0x00: write_reg(ins.rd(), rt << ins.shift_amount()); break; // SLL
        case 0x02: write_reg(ins.rd(), rt >> ins.shift_amount()); break; // SRL
        case 0x03: write_reg(ins.rd(), static_cast<std::uint32_t>(
            static_cast<std::int32_t>(rt) >> ins.shift_amount())); break; // SRA
        case 0x04: write_reg(ins.rd(), rt << (rs & 31u)); break; // SLLV
        case 0x06: write_reg(ins.rd(), rt >> (rs & 31u)); break; // SRLV
        case 0x07: write_reg(ins.rd(), static_cast<std::uint32_t>(
            static_cast<std::int32_t>(rt) >> (rs & 31u))); break; // SRAV
        case 0x08: branch(rs); in_delay_slot_ = true; break; // JR
        case 0x09: write_reg(ins.rd() ? ins.rd() : 31u, state_.pc + 4u);
                   branch(rs); in_delay_slot_ = true; break; // JALR
        case 0x10: write_reg(ins.rd(), state_.hi); break; // MFHI
        case 0x11: state_.hi = rs; break; // MTHI
        case 0x12: write_reg(ins.rd(), state_.lo); break; // MFLO
        case 0x13: state_.lo = rs; break; // MTLO
        case 0x18: { // MULT
            const auto product = static_cast<std::int64_t>(static_cast<std::int32_t>(rs)) *
                                 static_cast<std::int64_t>(static_cast<std::int32_t>(rt));
            const auto bits = static_cast<std::uint64_t>(product);
            state_.lo = static_cast<std::uint32_t>(bits);
            state_.hi = static_cast<std::uint32_t>(bits >> 32);
            result.cycles = 5;
            break;
        }
        case 0x19: { // MULTU
            const auto product = static_cast<std::uint64_t>(rs) * rt;
            state_.lo = static_cast<std::uint32_t>(product);
            state_.hi = static_cast<std::uint32_t>(product >> 32);
            result.cycles = 5;
            break;
        }
        case 0x1A: // DIV
            if (rt != 0) {
                const auto a = static_cast<std::int32_t>(rs);
                const auto b = static_cast<std::int32_t>(rt);
                if (a == std::numeric_limits<std::int32_t>::min() && b == -1) {
                    state_.lo = static_cast<std::uint32_t>(a);
                    state_.hi = 0;
                } else {
                    state_.lo = static_cast<std::uint32_t>(a / b);
                    state_.hi = static_cast<std::uint32_t>(a % b);
                }
            } else {
                state_.lo = (static_cast<std::int32_t>(rs) >= 0) ? 0xFFFFFFFFu : 1u;
                state_.hi = rs;
            }
            result.cycles = 36;
            break;
        case 0x1B: // DIVU
            if (rt != 0) { state_.lo = rs / rt; state_.hi = rs % rt; }
            else { state_.lo = 0xFFFFFFFFu; state_.hi = rs; }
            result.cycles = 36;
            break;
        case 0x20: write_reg(ins.rd(), rs + rt); break; // ADD (overflow trap pending)
        case 0x21: write_reg(ins.rd(), rs + rt); break; // ADDU
        case 0x22: write_reg(ins.rd(), rs - rt); break; // SUB (overflow trap pending)
        case 0x23: write_reg(ins.rd(), rs - rt); break; // SUBU
        case 0x24: write_reg(ins.rd(), rs & rt); break; // AND
        case 0x25: write_reg(ins.rd(), rs | rt); break; // OR
        case 0x26: write_reg(ins.rd(), rs ^ rt); break; // XOR
        case 0x27: write_reg(ins.rd(), ~(rs | rt)); break; // NOR
        case 0x2A: write_reg(ins.rd(), static_cast<std::int32_t>(rs) <
                                      static_cast<std::int32_t>(rt)); break; // SLT
        case 0x2B: write_reg(ins.rd(), rs < rt); break; // SLTU
        default: result.status = CpuStepStatus::ReservedInstruction; break;
        }
        break;

    case 0x02: // J
        branch((state_.pc & 0xF0000000u) | (ins.jump_index() << 2));
        in_delay_slot_ = true;
        break;
    case 0x03: // JAL
        write_reg(31, state_.pc + 4u);
        branch((state_.pc & 0xF0000000u) | (ins.jump_index() << 2));
        in_delay_slot_ = true;
        break;
    case 0x04: if (rs == rt) { branch(state_.pc + (static_cast<std::uint32_t>(imm) << 2)); in_delay_slot_ = true; } break;
    case 0x05: if (rs != rt) { branch(state_.pc + (static_cast<std::uint32_t>(imm) << 2)); in_delay_slot_ = true; } break;
    case 0x06: if (static_cast<std::int32_t>(rs) <= 0) { branch(state_.pc + (static_cast<std::uint32_t>(imm) << 2)); in_delay_slot_ = true; } break;
    case 0x07: if (static_cast<std::int32_t>(rs) > 0) { branch(state_.pc + (static_cast<std::uint32_t>(imm) << 2)); in_delay_slot_ = true; } break;
    case 0x08: write_reg(ins.rt(), rs + static_cast<std::uint32_t>(imm)); break; // ADDI (overflow trap pending)
    case 0x09: write_reg(ins.rt(), rs + static_cast<std::uint32_t>(imm)); break; // ADDIU
    case 0x0A: write_reg(ins.rt(), static_cast<std::int32_t>(rs) < imm); break; // SLTI
    case 0x0B: write_reg(ins.rt(), rs < static_cast<std::uint32_t>(imm)); break; // SLTIU
    case 0x0C: write_reg(ins.rt(), rs & uimm); break; // ANDI
    case 0x0D: write_reg(ins.rt(), rs | uimm); break; // ORI
    case 0x0E: write_reg(ins.rt(), rs ^ uimm); break; // XORI
    case 0x0F: write_reg(ins.rt(), uimm << 16); break; // LUI

    case 0x20: { // LB
        std::uint8_t v = 0;
        if (bus_.read8(rs + static_cast<std::uint32_t>(imm), v))
            write_reg(ins.rt(), sign_extend8(v));
        else result.status = CpuStepStatus::BusFault;
        break;
    }
    case 0x21: { // LH
        const auto a = rs + static_cast<std::uint32_t>(imm);
        std::uint16_t v = 0;
        if (a & 1u) result.status = CpuStepStatus::AddressError;
        else if (bus_.read16(a, v)) write_reg(ins.rt(), sign_extend16(v));
        else result.status = CpuStepStatus::BusFault;
        break;
    }
    case 0x23: { std::uint32_t v = 0; if (load_address(rs + static_cast<std::uint32_t>(imm), v)) write_reg(ins.rt(), v); break; } // LW
    case 0x24: { std::uint8_t v = 0; if (bus_.read8(rs + static_cast<std::uint32_t>(imm), v)) write_reg(ins.rt(), v); else result.status = CpuStepStatus::BusFault; break; } // LBU
    case 0x25: { const auto a = rs + static_cast<std::uint32_t>(imm); std::uint16_t v = 0; if (a & 1u) result.status = CpuStepStatus::AddressError; else if (bus_.read16(a, v)) write_reg(ins.rt(), v); else result.status = CpuStepStatus::BusFault; break; } // LHU
    case 0x28: if (!bus_.write8(rs + static_cast<std::uint32_t>(imm), static_cast<std::uint8_t>(rt))) result.status = CpuStepStatus::BusFault; break;
    case 0x29: { const auto a = rs + static_cast<std::uint32_t>(imm); if (a & 1u) result.status = CpuStepStatus::AddressError; else if (!bus_.write16(a, static_cast<std::uint16_t>(rt))) result.status = CpuStepStatus::BusFault; break; }
    case 0x2B: { const auto a = rs + static_cast<std::uint32_t>(imm); if (a & 3u) result.status = CpuStepStatus::AddressError; else if (!bus_.write32(a, rt)) result.status = CpuStepStatus::BusFault; break; }
    default:
        result.status = CpuStepStatus::ReservedInstruction;
        break;
    }

    state_.gpr[0] = 0;
    (void)was_delay_slot; // Used by the upcoming exception/CP0 implementation.
    return result;
}

} // namespace nexus::ps1
