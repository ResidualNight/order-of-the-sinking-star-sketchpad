#pragma once
namespace sketch::probe {
using Log=void(*)(const char*);
inline constexpr char Glyphs[]="手柄隔离测试基础内核已观测游戏查询入口已过滤非零输入尚未捕获可能提前缓存函数本轮无法验证请先连接并操作手柄开始停止立即恢复关闭草稿纸后结束仍按住的控件松开回中后恢复本版仅验证输入隔离不支持手柄绘图连接按钮扳机摇杆不可用返回游戏操作仍为零时反馈日志";
void start(Log log);
void maintain();
void close();
void emergency();
void panel(bool drawing);
}
