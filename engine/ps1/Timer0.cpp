#include "Timer0.hpp"
namespace nexus::ps1 {
void Timer0::reset() noexcept { counter_=target_=0; mode_=0; reachedTarget_=reachedOverflow_=false; irqArmed_=true; irqRequest_=true; hBlankActive_=syncStarted_=false; }
std::uint32_t Timer0::mode() const noexcept { return (mode_&0x3FFu)|(irqRequest_?(1u<<10):0u)|(reachedTarget_?(1u<<11):0u)|(reachedOverflow_?(1u<<12):0u); }
void Timer0::setHBlankActive(bool active) noexcept {
    if(active&&!hBlankActive_&&(mode_&kSyncEnable)){
        const auto sync=(mode_>>1)&3u;
        if(sync==1u||sync==2u) counter_=0;
        if(sync==3u) syncStarted_=true;
    }
    hBlankActive_=active;
}
bool Timer0::clockAllowed() const noexcept {
    if(!(mode_&kSyncEnable)) return true;
    switch((mode_>>1)&3u){
        case 0: return !hBlankActive_;
        case 1: return true;
        case 2: return hBlankActive_;
        default: return syncStarted_;
    }
}
bool Timer0::advance(std::uint32_t cycles) noexcept {
    const auto source=(mode_>>8)&3u;
    if((source&1u)||!clockAllowed()) return false;
    return advanceTicks(cycles);
}
bool Timer0::advanceDotClockPulses(std::uint32_t pulses) noexcept {
    const auto source=(mode_>>8)&3u;
    if(!(source&1u)||!clockAllowed()) return false;
    return advanceTicks(pulses);
}
bool Timer0::advanceTicks(std::uint32_t ticks) noexcept {
    bool irq=false;
    for(std::uint32_t i=0;i<ticks;++i){
        const auto next=static_cast<std::uint32_t>(counter_)+1u;
        const bool overflow=next>0xFFFFu;
        const bool targetHit=(next==target_&&counter_<=target_)||(overflow&&target_==0u);
        if(targetHit) reachedTarget_=true;
        if(overflow) reachedOverflow_=true;
        const bool event=(targetHit&&(mode_&kIrqAtTarget))||(overflow&&(mode_&kIrqAtOverflow));
        if(event&&(irqArmed_||(mode_&kIrqRepeat))){
            if(mode_&kIrqToggle){ irqRequest_=!irqRequest_; irq=irq||!irqRequest_; }
            else { irqRequest_=false; irq=true; }
            if(!(mode_&kIrqRepeat)) irqArmed_=false;
        }
        if(targetHit&&(mode_&kResetAtTarget)) counter_=0;
        else counter_=static_cast<std::uint16_t>(next);
    }
    return irq;
}
bool Timer0::read32(std::uint32_t a,std::uint32_t& v) noexcept {
    if(a==kCounterAddress){v=counter_;return true;}
    if(a==kModeAddress){v=mode();reachedTarget_=reachedOverflow_=false;return true;}
    if(a==kTargetAddress){v=target_;return true;} return false;
}
bool Timer0::write32(std::uint32_t a,std::uint32_t v) noexcept {
    if(a==kCounterAddress){counter_=static_cast<std::uint16_t>(v);return true;}
    if(a==kModeAddress){mode_=v&0x3FFu;counter_=0;reachedTarget_=reachedOverflow_=false;irqArmed_=true;irqRequest_=true;syncStarted_=false;return true;}
    if(a==kTargetAddress){target_=static_cast<std::uint16_t>(v);return true;} return false;
}
} // namespace nexus::ps1
