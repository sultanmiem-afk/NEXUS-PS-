#include "CpuState.hpp"

namespace nexus::ps1 {

void CpuState::reset(std::uint32_t reset_vector) noexcept {
    gpr.fill(0);
    hi = 0;
    lo = 0;
    pc = reset_vector;
    next_pc = reset_vector + 4u;
    gpr[0] = 0;
}

} // namespace nexus::ps1
