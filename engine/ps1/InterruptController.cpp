#include "InterruptController.hpp"

namespace nexus::ps1 {

void InterruptController::reset() noexcept {
    status_ = 0;
    mask_ = 0;
}

void InterruptController::request(std::uint32_t sources) noexcept {
    status_ |= sources & kValidSources;
}

bool InterruptController::read32(std::uint32_t address, std::uint32_t& value) const noexcept {
    if (address == kStatusAddress) {
        value = status_;
        return true;
    }
    if (address == kMaskAddress) {
        value = mask_;
        return true;
    }
    return false;
}

bool InterruptController::write32(std::uint32_t address, std::uint32_t value) noexcept {
    if (address == kStatusAddress) {
        // I_STAT is write-zero-to-clear: writing 0 clears a pending bit;
        // writing 1 leaves its current state unchanged.
        status_ &= value & kValidSources;
        return true;
    }
    if (address == kMaskAddress) {
        mask_ = value & kValidSources;
        return true;
    }
    return false;
}

} // namespace nexus::ps1
