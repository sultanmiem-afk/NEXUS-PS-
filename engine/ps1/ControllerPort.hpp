#pragma once

#include <cstdint>
#include <deque>

namespace nexus::ps1 {

// PS1 SIO0 digital/analog controller endpoint. Button bits are logical
// pressed-state bits; serial button bytes are active-low.
class ControllerPort final {
public:
    void reset() noexcept;
    void setButtons(std::uint16_t pressed) noexcept { buttons_ = pressed; }
    void setAnalogMode(bool enabled) noexcept { analogMode_ = enabled; }
    void setAnalogAxes(std::uint8_t x, std::uint8_t y) noexcept { analogX_ = x; analogY_ = y; }

    std::uint8_t readData() noexcept;
    std::uint32_t readStatus() const noexcept;
    std::uint16_t readMode() const noexcept { return mode_; }
    std::uint16_t readControl() const noexcept { return control_; }
    std::uint16_t readBaud() const noexcept { return baud_; }

    void writeData(std::uint8_t value) noexcept;
    void writeMode(std::uint16_t value) noexcept { mode_ = value; }
    void writeControl(std::uint16_t value) noexcept;
    void writeBaud(std::uint16_t value) noexcept { baud_ = value; }

private:
    void queueResponse(std::uint8_t value) noexcept;

    std::deque<std::uint8_t> rx_{};
    std::uint16_t buttons_ = 0;
    std::uint16_t latchedButtons_ = 0;
    std::uint16_t mode_ = 0;
    std::uint16_t control_ = 0;
    std::uint16_t baud_ = 0;
    std::uint8_t pollPhase_ = 0;
    bool polling_ = false;
    bool analogMode_ = false;
    std::uint8_t analogX_ = 128;
    std::uint8_t analogY_ = 128;
};

} // namespace nexus::ps1
