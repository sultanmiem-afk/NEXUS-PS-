#include "Timer2.hpp"
namespace nexus::ps1 {
void Timer2::reset() noexcept{counter_=target_=0;mode_=0;dividerPhase_=0;reachedTarget_=reachedOverflow_=false;irqArmed_=true;irqRequest_=true;irqPulseTicks_=0;}
std::uint32_t Timer2::mode() const noexcept{return(mode_&0x3FFu)|(irqRequest_?(1u<<10):0u)|(reachedTarget_?(1u<<11):0u)|(reachedOverflow_?(1u<<12):0u);}
bool Timer2::advanceTicks(std::uint32_t ticks) noexcept{
 bool irq=false;for(std::uint32_t i=0;i<ticks;++i){if(irqPulseTicks_>0u&&--irqPulseTicks_==0u)irqRequest_=true;const auto next=static_cast<std::uint32_t>(counter_)+1u;const bool ov=next>0xFFFFu;
 const bool hit=(next==target_&&counter_<=target_)||(ov&&target_==0u);if(hit)reachedTarget_=true;if(ov)reachedOverflow_=true;
 const bool event=(hit&&(mode_&kIrqAtTarget))||(ov&&(mode_&kIrqAtOverflow));
 if(event&&(irqArmed_||(mode_&kIrqRepeat))){if(mode_&kIrqToggle){irqRequest_=!irqRequest_;irq=irq||!irqRequest_;}else{irqRequest_=false;irq=true;irqPulseTicks_=1;}if(!(mode_&kIrqRepeat))irqArmed_=false;}
 if(hit&&(mode_&kResetAtTarget))counter_=0;else counter_=static_cast<std::uint16_t>(next);}return irq;
}
bool Timer2::advance(std::uint32_t cycles) noexcept{
 if(mode_&kSyncEnable){const auto sync=(mode_>>1)&3u;if(sync==0u||sync==3u)return false;}
 const auto source=(mode_>>8)&3u;
 if(source<2u)return advanceTicks(cycles);
 const std::uint32_t total=static_cast<std::uint32_t>(dividerPhase_)+cycles;
 const std::uint32_t ticks=total/8u;dividerPhase_=static_cast<std::uint8_t>(total%8u);return advanceTicks(ticks);
}
bool Timer2::read32(std::uint32_t a,std::uint32_t&v) noexcept{if(a==kCounterAddress){v=counter_;return true;}if(a==kModeAddress){v=mode();reachedTarget_=reachedOverflow_=false;return true;}if(a==kTargetAddress){v=target_;return true;}return false;}
bool Timer2::write32(std::uint32_t a,std::uint32_t v) noexcept{if(a==kCounterAddress){counter_=static_cast<std::uint16_t>(v);return true;}if(a==kModeAddress){mode_=v&0x3FFu;counter_=0;dividerPhase_=0;reachedTarget_=reachedOverflow_=false;irqArmed_=true;irqRequest_=true;irqPulseTicks_=0;return true;}if(a==kTargetAddress){target_=static_cast<std::uint16_t>(v);return true;}return false;}
} // namespace nexus::ps1
