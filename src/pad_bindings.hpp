#pragma once
#include "gamepad_ui.hpp"
#include <array>
namespace sketch {
enum class PadControl {None,A,B,X,Y,LB,RB,LT,RT,Menu,View,LS,RS,Count,Dpad,LeftStick,RightStick};
enum class PadAction {Draw,Erase,Previous,Next,Undo,Wheel,Quick,Settings,Cancel,RestartNumber,Redo,Count};
inline constexpr const char* PadActionNames[]={"draw","erase","previous","next","undo","wheel","quick","settings","cancel","restart_number","redo"};
inline constexpr const char* PadActionLabels[]={"绘制 / 放置","整笔擦除","上个工具 / 上页","下个工具 / 下页","撤销","长按快捷轮盘","快捷面板 / 确认","设置","取消 / 返回","编号从 1 开始（仅编号模式）","重做"};
inline constexpr const char* PadControlNames[]={"未分配","A","B","X","Y","LB","RB","LT","RT","菜单","视图","左摇杆按下","右摇杆按下"};
struct PadBindings {
 std::array<PadControl,size_t(PadAction::Count)> keys{PadControl::RT,PadControl::LT,PadControl::LB,PadControl::RB,PadControl::Y,PadControl::X,PadControl::A,PadControl::Menu,PadControl::B,PadControl::View,PadControl::None};
 PadControl get(PadAction a)const{return keys[size_t(a)];}
 bool valid()const{uint32_t seen=0;for(size_t i=0;i<keys.size();++i){int k=int(keys[i]);if(k<0||k>=int(PadControl::Count))return false;if(!k){if(i!=size_t(PadAction::Redo))return false;continue;}if(seen&(1u<<k))return false;seen|=1u<<k;}return true;}
 int conflict(PadAction a,PadControl k)const{if(k!=PadControl::None)for(size_t i=0;i<keys.size();++i)if(i!=size_t(a)&&keys[i]==k)return int(i);return -1;}
 // Selecting an occupied control swaps both roles atomically, never leaving an
 // essential navigation action unbound. Rejected assignments do not mutate.
 bool assign(PadAction a,PadControl k){if(int(a)<0||int(a)>=int(PadAction::Count))return false;auto next=*this;int other=conflict(a,k);if(other>=0)next.keys[other]=get(a);next.keys[size_t(a)]=k;if(!next.valid())return false;*this=next;return true;}
};
inline uint32_t padControlBit(PadControl c){return c>PadControl::None&&c<PadControl::Count?1u<<int(c):0;}
inline uint32_t padControls(const PadInput& in,uint32_t previous){
 if(!in.connected)return 0;uint32_t bits=0;
 constexpr uint16_t buttons[]={0,padui::A,padui::B,padui::X,padui::Y,padui::LB,padui::RB,0,0,padui::Menu,padui::Back,64,128};
 for(int i=1;i<int(PadControl::Count);++i)if(buttons[i]&&(in.buttons&buttons[i]))bits|=1u<<i;
 for(auto c:{PadControl::LT,PadControl::RT}){auto bit=padControlBit(c);if((c==PadControl::LT?in.lt:in.rt)>((previous&bit)?55:100))bits|=bit;}
 return bits;
}
}