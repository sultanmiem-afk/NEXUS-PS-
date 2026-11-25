#include "ControllerPort.hpp"

namespace nexus::ps1 {

void ControllerPort::reset() noexcept {
    rx_.clear();
    buttons_ = 0;
    latchedButtons_ = 0;
    mode_ = 0;
    control_ = 0;
    baud_ = 0;
    pollPhase_ = 0;
    polling_ = false;
}

void ControllerPort::queueResponse(std::uint8_t value) noexcept {
    rx_.push_back(value);
}

std::uint8_t ControllerPort::readData() noexcept {
    if (rx_.empty()) return 0xFFu;
    const std::uint8_t value = rx_.front();
    rx_.pop_front();
    return value;
}

std::uint32_t ControllerPort::readStatus() const noexcept {
    // TX-ready and TX-empty are modeled as immediately available.
    return 0x00000005u | (rx_.empty() ? 0u : 0x00000002u);
}

void ControllerPort::writeControl(std::uint16_t value) noexcept {
    control_ = value;
    if ((value & 0x0040u) != 0u) {
        rx_.clear();
        pollPhase_ = 0;
        polling_ = false;
        control_ = static_cast<std::uint16_t>(value & ~0x0040u);
    }
    if ((control_ & 0x0002u) == 0u) {
        rx_.clear();
        pollPhase_ = 0;
        polling_ = false;
    }
}

void ControllerPort::writeData(std::uint8_t value) noexcept {
    if ((control_ & 0x0002u) == 0u) return;

    if (!polling_) {
        if (value == 0x01u) {
            polling_ = true;
            pollPhase_ = 1;
            latchedButtons_ = buttons_;
            queueResponse(0xFFu);
        } else {
            queueResponse(0xFFu);
        }
        return;
    }

    switch (pollPhase_) {
        case 1:
            queueResponse(value == 0x42u ? 0x41u : 0xFFu);
            pollPhase_ = 2;
            break;
        case 2:
            queueResponse(0x5Au);
            pollPhase_ = 3;
            break;
        case 3:
            queueResponse(static_cast<std::uint8_t>(~(latchedButtons_ & 0x00FFu)));
            pollPhase_ = 4;
            break;
        case 4:
            queueResponse(static_cast<std::uint8_t>(~((latchedButtons_ >> 8u) & 0x00FFu)));
            pollPhase_ = 0;
            polling_ = false;
            break;
        default:
            queueResponse(0xFFu);
            pollPhase_ = 0;
            polling_ = false;
            break;
    }
}

} // namespace nexus::ps1
