#include "Cop0.hpp"

namespace nexus::ps1 {

void Cop0::reset() noexcept {
    regs_.fill(0);
}

std::uint32_t Cop0::read(std::uint32_t index) const noexcept {
    return index < regs_.size() ? regs_[index] : 0;
}

void Cop0::write(std::uint32_t index, std::uint32_t value) noexcept {
    if (index >= regs_.size()) return;
    // Cause software may only write the software interrupt-pending bits.
    if (index == kCause) {
        constexpr std::uint32_t software_pending_mask = 0x00000300u;
        regs_[index] = (regs_[index] & ~software_pending_mask) |
                       (value & software_pending_mask);
        return;
    }
    // BadVaddr, PRId, and other read-only registers are not writable here.
    if (index == kBadVaddr || index == 15u) return;
    regs_[index] = value;
}

void Cop0::enter_exception(CpuState& cpu, CpuException code,
                           std::uint32_t fault_pc, bool in_delay_slot,
                           std::uint32_t bad_vaddr) noexcept {
    auto& status = regs_[kStatus];
    auto& cause = regs_[kCause];

    cause &= ~((0x1Fu << 2) | kCauseBranchDelay);
    cause |= (static_cast<std::uint32_t>(code) & 0x1Fu) << 2;
    if (in_delay_slot) {
        cause |= kCauseBranchDelay;
        regs_[kEpc] = fault_pc - 4u;
    } else {
        regs_[kEpc] = fault_pc;
    }

    if (code == CpuException::AddressLoad ||
        code == CpuException::AddressStore) {
        regs_[kBadVaddr] = bad_vaddr;
    }

    // Push the low three KU/IE mode pairs onto the R3000A status stack.
    status = (status & ~0x3Fu) | ((status << 2) & 0x3Fu);

    const std::uint32_t vector = (status & kStatusBev)
        ? 0xBFC00180u : 0x80000080u;
    cpu.pc = vector;
    cpu.next_pc = vector + 4u;
}

void Cop0::setHardwareInterruptPending(bool pending) noexcept {
    constexpr std::uint32_t hardwareInterruptBit = 1u << 10;
    if (pending) regs_[kCause] |= hardwareInterruptBit;
    else regs_[kCause] &= ~hardwareInterruptBit;
}

bool Cop0::interruptPending() const noexcept {
    const auto status = regs_[kStatus];
    const auto cause = regs_[kCause];
    constexpr std::uint32_t globalEnable = 1u;
    constexpr std::uint32_t interruptMasks = 0x0000FF00u;
    return (status & globalEnable) != 0u &&
           (status & cause & interruptMasks) != 0u;
}

void Cop0::return_from_exception() noexcept {
    auto& status = regs_[kStatus];
    status = (status & ~0x0Fu) | ((status >> 2) & 0x0Fu);
}

} // namespace nexus::ps1
