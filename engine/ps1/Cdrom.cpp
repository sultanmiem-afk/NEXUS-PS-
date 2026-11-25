#include "Cdrom.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <utility>

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
    index_ = 0; interruptEnable_ = 0; interruptFlags_ = 0; mode_ = 0;
    request_ = 0; currentLba_ = 0; cycleRemainder_ = 0;
    reading_ = false; motorOn_ = false; dataReady_ = false;
    parameters_.clear(); responses_.clear(); pendingInterrupts_.clear(); data_.clear();
}

bool Cdrom::loadImage(const std::vector<std::uint8_t>& image) noexcept {
    if (image.empty()) return false;
    std::size_t sectorSize = 0;
    if (image.size() % kRawSectorSize == 0u) sectorSize = kRawSectorSize;
    else if (image.size() % kCookedSectorSize == 0u) sectorSize = kCookedSectorSize;
    else return false;
    image_ = image;
    tracks_.clear();
    Track track{};
    track.sectorSize = sectorSize;
    track.dataOffset = sectorSize == kRawSectorSize ? kRawUserDataOffset : 0u;
    track.fileEndSector = static_cast<std::uint32_t>(image.size() / sectorSize);
    tracks_.push_back(track);
    cueLoaded_ = false;
    reset();
    return true;
}

bool Cdrom::loadCue(const std::string& cueText, const std::string& binFilename,
                    const std::vector<std::uint8_t>& binImage) noexcept {
    if (cueText.empty() || binFilename.empty() || binImage.empty()) return false;
    std::string referencedFile;
    std::vector<Track> parsedTracks;
    std::istringstream input(cueText);
    std::string line;
    Track* currentTrack = nullptr;
    std::uint32_t previousDiscStart = 0;
    bool havePreviousTrack = false;
    std::size_t detectedSectorSize = 0;

    auto basename = [](std::string value) {
        const std::size_t slash = value.find_last_of("/\\");
        if (slash != std::string::npos) value = value.substr(slash + 1u);
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return value;
    };
    auto parseMsf = [](const std::string& value, std::uint32_t& frames) {
        unsigned minute = 0, second = 0, frame = 0;
        char a = 0, b = 0;
        std::istringstream msf(value);
        if (!(msf >> minute >> a >> second >> b >> frame) ||
            a != ':' || b != ':' || second >= 60u || frame >= 75u) return false;
        frames = (minute * 60u + second) * 75u + frame;
        return true;
    };

    while (std::getline(input, line)) {
        std::istringstream row(line);
        std::string token;
        if (!(row >> token)) continue;
        std::transform(token.begin(), token.end(), token.begin(),
            [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
        if (token == "FILE") {
            std::string filename;
            if (!(row >> std::quoted(filename))) {
                row.clear();
                const std::size_t first = line.find_first_of(" \t");
                if (first == std::string::npos) return false;
                std::istringstream fallback(line.substr(first + 1u));
                if (!(fallback >> filename)) return false;
            }
            if (!referencedFile.empty()) return false;
            referencedFile = filename;
        } else if (token == "TRACK") {
            unsigned number = 0;
            std::string format;
            if (!(row >> number >> format) || number == 0u) return false;
            std::transform(format.begin(), format.end(), format.begin(),
                [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
            Track track{};
            if (format == "MODE1/2048") {
                track.sectorSize = 2048u; track.dataOffset = 0u;
            } else if (format == "MODE1/2352") {
                track.sectorSize = 2352u; track.dataOffset = 16u;
            } else if (format == "MODE2/2352") {
                track.sectorSize = 2352u; track.dataOffset = 24u;
            } else return false;
            if (detectedSectorSize != 0u && detectedSectorSize != track.sectorSize)
                return false;
            detectedSectorSize = track.sectorSize;
            parsedTracks.push_back(track);
            currentTrack = &parsedTracks.back();
        } else if (token == "INDEX") {
            unsigned indexNumber = 0;
            std::string timestamp;
            if (!(row >> indexNumber >> timestamp)) return false;
            if (indexNumber != 1u) continue;
            if (currentTrack == nullptr) return false;
            if (!parseMsf(timestamp, currentTrack->fileStartSector)) return false;
            currentTrack->discStartLba = currentTrack->fileStartSector >= 150u
                ? currentTrack->fileStartSector - 150u : 0u;
            if (havePreviousTrack && currentTrack->discStartLba <= previousDiscStart)
                return false;
            previousDiscStart = currentTrack->discStartLba;
            havePreviousTrack = true;
        }
    }

    if (referencedFile.empty() || parsedTracks.empty() ||
        basename(referencedFile) != basename(binFilename)) return false;
    if (detectedSectorSize == 0u || binImage.size() % detectedSectorSize != 0u)
        return false;
    const std::uint32_t totalSectors =
        static_cast<std::uint32_t>(binImage.size() / detectedSectorSize);
    for (std::size_t i = 0; i < parsedTracks.size(); ++i) {
        Track& track = parsedTracks[i];
        if (track.fileStartSector >= totalSectors) return false;
        track.fileEndSector = totalSectors;
        if (i + 1u < parsedTracks.size()) {
            const std::uint32_t nextStart = parsedTracks[i + 1u].fileStartSector;
            if (nextStart <= track.fileStartSector) return false;
            track.fileEndSector = nextStart;
        }
    }
    image_ = binImage;
    tracks_ = std::move(parsedTracks);
    cueLoaded_ = true;
    reset();
    return true;
}

std::size_t Cdrom::sectorCount() const noexcept {
    if (tracks_.empty()) return 0u;
    const Track& last = tracks_.back();
    return static_cast<std::size_t>(last.discStartLba) +
        (last.fileEndSector - last.fileStartSector);
}

bool Cdrom::irqPending() const noexcept {
    return interruptFlags_ != 0u &&
        (interruptEnable_ & (1u << (interruptFlags_ - 1u))) != 0u;
}

void Cdrom::pushResponse(std::uint8_t value) noexcept {
    responses_.push_back(value);
}

void Cdrom::raiseInterrupt(std::uint8_t code) noexcept {
    if (code >= 1u && code <= 5u) interruptFlags_ = code;
}

std::uint8_t Cdrom::fromBcd(std::uint8_t value) noexcept {
    return static_cast<std::uint8_t>(((value >> 4u) & 0x0Fu) * 10u + (value & 0x0Fu));
}

std::uint8_t Cdrom::toBcd(std::uint32_t value) noexcept {
    return static_cast<std::uint8_t>(((value / 10u) << 4u) | (value % 10u));
}

bool Cdrom::readSector(std::uint32_t lba, std::vector<std::uint8_t>& output) const {
    if (!imageLoaded()) return false;
    const Track* selected = nullptr;
    for (const Track& track : tracks_) {
        const std::uint32_t count = track.fileEndSector - track.fileStartSector;
        if (lba >= track.discStartLba && lba < track.discStartLba + count) {
            selected = &track;
            break;
        }
    }
    if (selected == nullptr) return false;
    const std::uint32_t fileSector = selected->fileStartSector +
        (lba - selected->discStartLba);
    const std::size_t offset = static_cast<std::size_t>(fileSector) * selected->sectorSize;
    std::size_t userDataSize = selected->userDataSize;
    // MODE2 sectors carry a duplicated four-byte subheader at offsets 16 and
    // 20. Submode bit 5 identifies Form 2, whose user-data payload is 2324
    // bytes instead of the 2048-byte Form 1 payload.
    if (selected->sectorSize == kRawSectorSize && selected->dataOffset == 24u &&
        offset + 18u < image_.size() && (image_[offset + 18u] & 0x20u) != 0u) {
        userDataSize = 2324u;
    }
    if (offset + selected->dataOffset + userDataSize > image_.size()) return false;
    output.assign(image_.begin() + static_cast<std::ptrdiff_t>(offset + selected->dataOffset),
                  image_.begin() + static_cast<std::ptrdiff_t>(
                      offset + selected->dataOffset + userDataSize));
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

bool Cdrom::readDataWord(std::uint32_t& value) noexcept {
    if (data_.size() < 4u) return false;
    value = 0u;
    for (unsigned byte = 0; byte < 4u; ++byte) {
        value |= static_cast<std::uint32_t>(data_.front()) << (byte * 8u);
        data_.pop_front();
    }
    if (data_.empty()) dataReady_ = false;
    return true;
}

void Cdrom::advance(std::uint32_t cpuCycles) noexcept {
    if (!reading_ || !imageLoaded()) return;
    if (!data_.empty()) return;
    const std::uint32_t sectorCycles = (mode_ & 0x80u) != 0u
        ? (kCyclesPerSector / 2u) : kCyclesPerSector;
    const std::uint64_t total = static_cast<std::uint64_t>(cycleRemainder_) + cpuCycles;
    if (total < sectorCycles) {
        cycleRemainder_ = static_cast<std::uint32_t>(total);
        return;
    }
    cycleRemainder_ = static_cast<std::uint32_t>(total % sectorCycles);
    produceSector();
}

void Cdrom::executeCommand(std::uint8_t command) noexcept {
    auto status = [this]() { return static_cast<std::uint8_t>(motorOn_ ? 0x02u : 0x00u); };
    auto findTrack = [this](std::uint32_t lba) -> const Track* {
        for (const Track& track : tracks_) {
            const std::uint32_t count = track.fileEndSector - track.fileStartSector;
            if (lba >= track.discStartLba && lba < track.discStartLba + count)
                return &track;
        }
        return nullptr;
    };
    auto pushMsf = [this](std::uint32_t frames) {
        pushResponse(toBcd(frames / (60u * 75u)));
        pushResponse(toBcd((frames / 75u) % 60u));
        pushResponse(toBcd(frames % 75u));
    };

    switch (command) {
    case 0x01u: // Getstat
        pushResponse(status()); raiseInterrupt(kIrqCommand); break;
    case 0x02u: { // Setloc
        if (parameters_.size() < 3u) { raiseInterrupt(kIrqError); break; }
        const std::uint32_t absolute =
            (fromBcd(parameters_[0]) * 60u + fromBcd(parameters_[1])) * 75u +
            fromBcd(parameters_[2]);
        parameters_.clear();
        currentLba_ = absolute >= 150u ? absolute - 150u : 0u;
        data_.clear(); dataReady_ = false; cycleRemainder_ = 0;
        pushResponse(status()); raiseInterrupt(kIrqCommand); break;
    }
    case 0x06u: // ReadN
    case 0x1Bu: // ReadS: raw-sector details remain simplified to user data
        if (!imageLoaded()) { pushResponse(0u); raiseInterrupt(kIrqError); break; }
        motorOn_ = true; reading_ = true; data_.clear(); dataReady_ = false; cycleRemainder_ = 0;
        pushResponse(status()); raiseInterrupt(kIrqCommand); break;
    case 0x07u: // Standby
    case 0x08u: // Stop
    case 0x09u: // Pause
        reading_ = false; cycleRemainder_ = 0;
        if (command == 0x08u || command == 0x0Au) motorOn_ = false;
        pushResponse(status()); raiseInterrupt(kIrqCommand); break;
    case 0x0Au: // Init
        reading_ = false; mode_ = 0; filterFile_ = 0; filterChannel_ = 0;
        request_ = 0; data_.clear(); dataReady_ = false; cycleRemainder_ = 0;
        parameters_.clear(); pushResponse(status()); raiseInterrupt(kIrqCommand); break;
    case 0x0Du: // Setfilter
        if (parameters_.size() < 2u) { raiseInterrupt(kIrqError); break; }
        filterFile_ = parameters_[0]; filterChannel_ = parameters_[1];
        parameters_.clear(); pushResponse(status()); raiseInterrupt(kIrqCommand); break;
    case 0x0Eu: // Setmode
        if (!parameters_.empty()) mode_ = parameters_.front();
        parameters_.clear(); cycleRemainder_ = 0;
        pushResponse(status()); raiseInterrupt(kIrqCommand); break;
    case 0x0Fu: // Getparam
        pushResponse(status()); pushResponse(mode_);
        pushResponse(filterFile_); pushResponse(filterChannel_);
        raiseInterrupt(kIrqCommand); break;
    case 0x10u: { // GetlocL: synthesized sector header
        const Track* track = findTrack(currentLba_);
        pushMsf(currentLba_ + 150u);
        pushResponse(track != nullptr && track->dataOffset == 24u ? 0x02u : 0x01u);
        pushResponse(0u); pushResponse(0u); pushResponse(0u); pushResponse(0u);
        raiseInterrupt(kIrqCommand); break;
    }
    case 0x11u: { // GetlocP: track/index and relative/absolute time
        const Track* track = findTrack(currentLba_);
        if (track == nullptr) {
            for (unsigned i = 0; i < 8u; ++i) pushResponse(0u);
        } else {
            const std::size_t trackIndex = static_cast<std::size_t>(track - tracks_.data());
            pushResponse(toBcd(static_cast<std::uint32_t>(trackIndex + 1u)));
            pushResponse(0x01u);
            pushMsf(currentLba_ - track->discStartLba);
            pushMsf(currentLba_ + 150u);
        }
        raiseInterrupt(kIrqCommand); break;
    }
    case 0x13u: // GetTN
        pushResponse(status());
        pushResponse(tracks_.empty() ? 0u : 0x01u);
        pushResponse(toBcd(static_cast<std::uint32_t>(tracks_.size())));
        raiseInterrupt(kIrqCommand); break;
    case 0x14u: { // GetTD: approximate selected track start MSF
        if (parameters_.empty()) { raiseInterrupt(kIrqError); break; }
        const std::uint32_t number = fromBcd(parameters_.front());
        parameters_.clear();
        if (number == 0u || number > tracks_.size()) {
            pushResponse(0u); raiseInterrupt(kIrqError); break;
        }
        const std::uint32_t absolute = tracks_[number - 1u].discStartLba + 150u;
        pushResponse(status());
        pushResponse(toBcd(absolute / (60u * 75u)));
        pushResponse(toBcd((absolute / 75u) % 60u));
        raiseInterrupt(kIrqCommand); break;
    }
    case 0x15u: // SeekL
    case 0x16u: // SeekP
        if (!imageLoaded() || findTrack(currentLba_) == nullptr) {
            parameters_.clear(); pushResponse(0u); raiseInterrupt(kIrqError); break;
        }
        reading_ = false; motorOn_ = true; data_.clear(); dataReady_ = false; cycleRemainder_ = 0;
        pushResponse(status()); raiseInterrupt(kIrqCommand); break;
    case 0x19u: // Test
        if (!parameters_.empty()) {
            const std::uint8_t sub = parameters_.front();
            parameters_.clear();
            if (sub == 0x20u) {
                pushResponse(0x98u); pushResponse(0x06u); pushResponse(0x10u); pushResponse(0xC3u);
            } else {
                pushResponse(0u);
            }
        } else pushResponse(0u);
        raiseInterrupt(kIrqCommand); break;
    case 0x1Au: // GetID
        parameters_.clear();
        pushResponse(status());
        pushResponse(0x00u); // second response: licensed/valid disc
        pushResponse(0x20u); // SCEA region/product family placeholder
        pushResponse('S'); pushResponse('C'); pushResponse('E'); pushResponse('A');
        raiseInterrupt(kIrqCommand);
        pendingInterrupts_.push_back(2u); // final GetID response interrupt (INT2)
        break;
    case 0x03u: // Play
        parameters_.clear(); motorOn_ = true; reading_ = true; cycleRemainder_ = 0;
        pushResponse(status()); raiseInterrupt(kIrqCommand); break;
    case 0x0Bu: // Mute
    case 0x0Cu: // Demute
        parameters_.clear(); pushResponse(status()); raiseInterrupt(kIrqCommand); break;
    default:
        // Unknown commands must not leak stale arguments into a later Setloc.
        parameters_.clear();
        pushResponse(0x00u); raiseInterrupt(kIrqError); break;
    }
}

bool Cdrom::read8(std::uint32_t address, std::uint8_t& value) noexcept {
    if (address < kBaseAddress || address > kBaseAddress + 3u) return false;
    const std::uint32_t reg = address - kBaseAddress;
    if (reg == 0u) {
        value = static_cast<std::uint8_t>((index_ & 0x03u) |
            (parameters_.empty() ? 0x08u : 0u) |
            (parameters_.size() < 16u ? 0x10u : 0u) |
            (!responses_.empty() ? 0x20u : 0u) |
            (!data_.empty() ? 0x40u : 0u) |
            (reading_ ? 0x80u : 0u));
        return true;
    }
    if (reg == 1u) {
        value = responses_.empty() ? 0u : responses_.front();
        if (!responses_.empty()) responses_.pop_front();
        if (responses_.empty() && !pendingInterrupts_.empty()) {
            interruptFlags_ = pendingInterrupts_.front();
            pendingInterrupts_.pop_front();
        }
        return true;
    }
    if (reg == 2u) {
        if (index_ == 0u) {
            value = data_.empty() ? 0u : data_.front();
            if (!data_.empty()) data_.pop_front();
            if (data_.empty()) dataReady_ = false;
        } else value = 0u;
        return true;
    }
    value = index_ == 1u ? interruptFlags_ : 0u;
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
        // Index 2/3 writes are not aliases for Enable or Mode.
        return true;
    }
    if (reg == 2u) {
        if (index_ == 0u) {
            if (parameters_.size() < 16u) parameters_.push_back(value);
        } else if (index_ == 1u) {
            request_ = value;
        }
        return true;
    }
    if (index_ == 0u) {
        request_ = value;
    } else if (index_ == 1u) {
        // Acknowledge the currently latched interrupt number using its bit.
        if (interruptFlags_ != 0u &&
            (value & (1u << (interruptFlags_ - 1u))) != 0u)
            interruptFlags_ = 0u;
    } else if (index_ == 2u) {
        // Audio volume register pair: never flush the data FIFO.
    }
    return true;
}

} // namespace nexus::ps1
