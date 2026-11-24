#include "Timer1.hpp"
namespace nexus::ps1 {
void Timer1::reset() noexcept{counter_=target_=0;mode_=0;reachedTarget_=reachedOverflow_=false;irqArmed_=true;irqRequest_=true;irqPulseTicks_=0;vBlankActive_=syncStarted_=false;}
std::uint32_t Timer1::mode() const noexcept{return(mode_&0x3FFu)|(irqRequest_?(1u<<10):0u)|(reachedTarget_?(1u<<11):0u)|(reachedOverflow_?(1u<<12):0u);}
void Timer1::setVBlankActive(bool active) noexcept{
 if(active&&!vBlankActive_&&(mode_&kSyncEnable)){const auto s=(mode_>>1)&3u;if(s==1u||s==2u)counter_=0;if(s==3u)syncStarted_=true;}
 vBlankActive_=active;
}
bool Timer1::clockAllowed() const noexcept{
 if(!(mode_&kSyncEnable))return true;
 switch((mode_>>1)&3u){case 0:return !vBlankActive_;case 1:return true;case 2:return vBlankActive_;default:return syncStarted_;}
}
bool Timer1::advance(std::uint32_t cycles) noexcept{const auto s=(mode_>>8)&3u;if((s&1u)||!clockAllowed())return false;return advanceTicks(cycles);}
bool Timer1::advanceHBlankPulses(std::uint32_t pulses) noexcept{const auto s=(mode_>>8)&3u;if(!(s&1u)||!clockAllowed())return false;return advanceTicks(pulses);}
bool Timer1::advanceTicks(std::uint32_t ticks) noexcept{
 bool irq=false;for(std::uint32_t i=0;i<ticks;++i){if(irqPulseTicks_>0u&&--irqPulseTicks_==0u)irqRequest_=true;const auto next=static_cast<std::uint32_t>(counter_)+1u;const bool ov=next>0xFFFFu;
 const bool hit=(next==target_&&counter_<=target_)||(ov&&target_==0u);if(hit)reachedTarget_=true;if(ov)reachedOverflow_=true;
 const bool event=(hit&&(mode_&kIrqAtTarget))||(ov&&(mode_&kIrqAtOverflow));
 if(event&&(irqArmed_||(mode_&kIrqRepeat))){if(mode_&kIrqToggle){irqRequest_=!irqRequest_;irq=irq||!irqRequest_;}else{irqRequest_=false;irq=true;irqPulseTicks_=1;}if(!(mode_&kIrqRepeat))irqArmed_=false;}
 if(hit&&(mode_&kResetAtTarget))counter_=0;else counter_=static_cast<std::uint16_t>(next);}return irq;
}
bool Timer1::read32(std::uint32_t a,std::uint32_t&v) noexcept{if(a==kCounterAddress){v=counter_;return true;}if(a==kModeAddress){v=mode();reachedTarget_=reachedOverflow_=false;return true;}if(a==kTargetAddress){v=target_;return true;}return false;}
bool Timer1::write32(std::uint32_t a,std::uint32_t v) noexcept{if(a==kCounterAddress){counter_=static_cast<std::uint16_t>(v);return true;}if(a==kModeAddress){mode_=v&0x3FFu;counter_=0;reachedTarget_=reachedOverflow_=false;irqArmed_=true;irqRequest_=true;irqPulseTicks_=0;syncStarted_=false;return true;}if(a==kTargetAddress){target_=static_cast<std::uint16_t>(v);return true;}return false;}
} // namespace nexus::ps1
