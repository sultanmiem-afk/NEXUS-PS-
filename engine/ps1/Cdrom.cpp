#include "Cdrom.hpp"

#include <algorithm>

namespace nexus::ps1 {
namespace {
constexpr std::size_t kCookedSectorSize = 2048u;
constexpr std::size_t kRawSectorSize = 2352u;
constexpr std::size_t kRawUserDataOffset = 24u;
constexpr std::uint8_t kIrqCommand = 0x03u;
constexpr std::uint8_t kIrqDataReady = 0x02u;
constexpr std::uint8_t kIrqError = 0x05u;
}

void Cdrom::reset() noexcept {
    index_ = 0;
    interruptEnable_ = 0;
    interruptFlags_ = 0;
    mode_ = 0;
    request_ = 0;
    currentLba_ = 0;
    cycleRemainder_ = 0;
    reading_ = false;
    dataReady_ = false;
    parameters_.clear();
    responses_.clear();
    data_.clear();
}

bool Cdrom::loadImage(const std::vector<std::uint8_t>& image) noexcept {
    if (image.empty()) return false;
    std::size_t sectorSize = 0;
    if ((image.size() % kCookedSectorSize) == 0u) sectorSize = kCookedSectorSize;
    else if ((image.size() % kRawSectorSize) == 0u) sectorSize = kRawSectorSize;
    else return false;
    image_ = image;
    sectorSize_ = sectorSize;
    reset();
    return true;
}

std::size_t Cdrom::sectorCount() const noexcept {
    return sectorSize_ == 0u ? 0u : image_.size() / sectorSize_;
}

bool Cdrom::irqPending() const noexcept {
    return (interruptFlags_ & interruptEnable_ & 0x1Fu) != 0u;
}

void Cdrom::pushResponse(std::uint8_t value) noexcept {
    responses_.push_back(value);
}

void Cdrom::raiseInterrupt(std::uint8_t code) noexcept {
    interruptFlags_ = static_cast<std::uint8_t>((interruptFlags_ & 0xF8u) | (code & 0x07u));
}

std::uint8_t Cdrom::fromBcd(std::uint8_t value) noexcept {
    return static_cast<std::uint8_t>(((value >> 4u) & 0x0Fu) * 10u + (value & 0x0Fu));
}

bool Cdrom::readSector(std::uint32_t lba, std::vector<std::uint8_t>& output) const {
    if (!imageLoaded() || lba >= sectorCount()) return false;
    const std::size_t offset = static_cast<std::size_t>(lba) * sectorSize_;
    const std::size_t dataOffset = sectorSize_ == kRawSectorSize ? kRawUserDataOffset : 0u;
    if (offset + dataOffset + kCookedSectorSize > image_.size()) return false;
    output.assign(image_.begin() + static_cast<std::ptrdiff_t>(offset + dataOffset),
                  image_.begin() + static_cast<std::ptrdiff_t>(offset + dataOffset + kCookedSectorSize));
    return true;
}

void Cdrom::produceSector() noexcept {
    if (!reading_ || !data_.empty()) return;
    std::vector<std::uint8_t> sector;
    if (!readSector(currentLba_, sector)) {
        reading_ = false;
        pushResponse(0x00u);
        raiseInterrupt(kIrqError);
        return;
    }
    for (const std::uint8_t byte : sector) data_.push_back(byte);
    ++currentLba_;
    dataReady_ = true;
    raiseInterrupt(kIrqDataReady);
}

void Cdrom::advance(std::uint32_t cpuCycles) noexcept {
    if (!reading_ || !imageLoaded()) return;
    if (!data_.empty()) return;
    const std::uint64_t total = static_cast<std::uint64_t>(cycleRemainder_) + cpuCycles;
    if (total < kCyclesPerSector) {
        cycleRemainder_ = static_cast<std::uint32_t>(total);
        return;
    }
    cycleRemainder_ = static_cast<std::uint32_t>(total % kCyclesPerSector);
    produceSector();
}

void Cdrom::executeCommand(std::uint8_t command) noexcept {
    switch (command) {
    case 0x01u: // Getstat
        pushResponse(static_cast<std::uint8_t>(reading_ ? 0x20u : 0x00u));
        raiseInterrupt(kIrqCommand);
        break;
    case 0x02u: { // Setloc (minute, second, frame; BCD)
        if (parameters_.size() < 3u) {
            raiseInterrupt(kIrqError);
            break;
        }
        const std::uint32_t minute = fromBcd(parameters_[0]);
        const std::uint32_t second = fromBcd(parameters_[1]);
        const std::uint32_t frame = fromBcd(parameters_[2]);
        parameters_.clear();
        const std::uint32_t absolute = (minute * 60u + second) * 75u + frame;
        currentLba_ = absolute >= 150u ? absolute - 150u : 0u;
        pushResponse(static_cast<std::uint8_t>(reading_ ? 0x20u : 0x00u));
        raiseInterrupt(kIrqCommand);
        break;
    }
    case 0x06u: // ReadN
    case 0x1Bu: // ReadS (same simplified data path)
        if (!imageLoaded()) {
            pushResponse(0x00u);
            raiseInterrupt(kIrqError);
            break;
        }
        reading_ = true;
        data_.clear();
        dataReady_ = false;
        cycleRemainder_ = 0;
        pushResponse(0x20u);
        raiseInterrupt(kIrqCommand);
        break;
    case 0x08u: // Stop
    case 0x09u: // Pause
        reading_ = false;
        cycleRemainder_ = 0;
        pushResponse(0x00u);
        raiseInterrupt(kIrqCommand);
        break;
    case 0x0Au: // Init
        reading_ = false;
        mode_ = 0;
        request_ = 0;
        data_.clear();
        dataReady_ = false;
        cycleRemainder_ = 0;
        pushResponse(0x00u);
        raiseInterrupt(kIrqCommand);
        break;
    case 0x0Eu: // Setmode
        if (!parameters_.empty()) mode_ = parameters_.front();
        parameters_.clear();
        pushResponse(static_cast<std::uint8_t>(reading_ ? 0x20u : 0x00u));
        raiseInterrupt(kIrqCommand);
        break;
    case 0x10u: // GetlocL (minimal header response)
        pushResponse(0u); pushResponse(0u); pushResponse(0u);
        pushResponse(0u); pushResponse(0u); pushResponse(0u);
        pushResponse(0u); pushResponse(0u);
        raiseInterrupt(kIrqCommand);
        break;
    default:
        pushResponse(0x00u);
        raiseInterrupt(kIrqError);
        break;
    }
}

bool Cdrom::read8(std::uint32_t address, std::uint8_t& value) noexcept {
    if (address < kBaseAddress || address > kBaseAddress + 3u) return false;
    const std::uint32_t reg = address - kBaseAddress;
    if (reg == 0u) {
        value = static_cast<std::uint8_t>((parameters_.empty() ? 0x01u : 0u) |
            (parameters_.size() < 16u ? 0x02u : 0u) |
            (!responses_.empty() ? 0x04u : 0u) |
            (!data_.empty() ? 0x08u : 0u) |
            (reading_ ? 0x20u : 0u));
        return true;
    }
    if (reg == 1u) {
        if (index_ == 1u) value = interruptEnable_;
        else if (index_ == 2u) value = interruptFlags_;
        else {
            value = responses_.empty() ? 0u : responses_.front();
            if (!responses_.empty()) responses_.pop_front();
        }
        return true;
    }
    if (reg == 2u) {
        if (index_ == 0u) {
            value = data_.empty() ? 0u : data_.front();
            if (!data_.empty()) data_.pop_front();
            if (data_.empty()) dataReady_ = false;
        } else {
            value = mode_;
        }
        return true;
    }
    value = index_ == 1u ? interruptFlags_ : interruptEnable_;
    return true;
}

bool Cdrom::write8(std::uint32_t address, std::uint8_t value) noexcept {
    if (address < kBaseAddress || address > kBaseAddress + 3u) return false;
    const std::uint32_t reg = address - kBaseAddress;
    if (reg == 0u) {
        index_ = static_cast<std::uint8_t>(value & 0x03u);
        return true;
    }
    if (reg == 1u) {
        if (index_ == 0u) executeCommand(value);
        else if (index_ == 1u) interruptEnable_ = static_cast<std::uint8_t>(value & 0x1Fu);
        else if (index_ == 2u) mode_ = value;
        return true;
    }
    if (reg == 2u) {
        if (index_ == 0u) {
            if (parameters_.size() < 16u) parameters_.push_back(value);
        } else if (index_ == 1u) {
            interruptEnable_ = static_cast<std::uint8_t>(value & 0x1Fu);
        } else if (index_ == 2u) {
            mode_ = value;
        }
        return true;
    }
    if (index_ == 0u) {
        request_ = value;
    } else if (index_ == 1u) {
        interruptFlags_ = static_cast<std::uint8_t>(interruptFlags_ & ~(value & 0x1Fu));
    } else if (index_ == 2u) {
        data_.clear();
        dataReady_ = false;
    }
    return true;
}

} // namespace nexus::ps1
