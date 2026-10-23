#pragma once
#include <string>
namespace sketch {
// Published identifiers are stable: add new entries rather than renumbering/reusing them.
enum class ErrorId {
 None,UnsupportedGameBuild,EntityManagerUnavailable,SettingsArrayUnavailable,SettingsGridInvalid,
 ActorLayerUnavailable,FreshCameraFrameUnavailable,VisibleActorLayerUnavailable,LevelChanged,
 ProjectedGridInvalid,CameraNotFound,CameraOrientationInvalid,UnknownGameGrid,ImageGridUnconfirmed,ImageGridUnreliable,
 UnsupportedCaptureFormat,InvalidCalibration,Count
};
struct ErrorInfo {const char* code;const char* diagnostic;const char* message;};
inline constexpr ErrorInfo Errors[]={
 {"","",""},
 {"SS-GRID-001","unsupported_game_build","当前游戏版本未验证或版本校验失败，请使用手动校准"},
 {"SS-GRID-002","active_entity_manager_unavailable","未读到当前关卡，请进入关卡后重试或手动校准"},
 {"SS-GRID-003","settings_array_unavailable","未读到关卡设置，请重试或手动校准"},
 {"SS-GRID-004","settings_grid_invalid","关卡网格数据无效，请重试或手动校准"},
 {"SS-GRID-005","actor_layer_unavailable","未读到角色高度层，请进入关卡后重试或手动校准"},
 {"SS-GRID-006","fresh_camera_frame_unavailable","等待新相机帧超时，请返回普通游戏视角后重新识别，或手动校准"},
 {"SS-GRID-007","visible_actor_layer_unavailable","无法确定可见角色高度层，请返回角色附近后重试或手动校准"},
 {"SS-GRID-008","level_changed_during_localization","定位期间关卡已变化，请等待切换完成后重新识别"},
 {"SS-GRID-009","projected_grid_invalid","当前高度层或视角无法生成有效网格，请重试或手动校准"},
 {"SS-GRID-010","validated_camera_unavailable","未找到通过校验的相机数据，请返回普通游戏视角后重新识别，或手动校准"},
 {"SS-GRID-011","camera_orientation_invalid","检测到翻转或方向异常的相机，已拒绝定位，请重新识别或手动校准"},
 {"SS-GRID-099","unknown_game_grid_error","数据定位失败，请重新识别或按 Shift+G 手动校准"},
 {"SS-IMAGE-001","image_grid_unconfirmed","检测到候选网格，请确认或手动调整；暂不启用吸附"},
 {"SS-IMAGE-002","image_grid_unreliable","未能可靠识别网格，请手动调整或按 Shift+G 校准"},
 {"SS-IMAGE-003","unsupported_capture_format","当前画面格式不支持图像识别，请手动调整网格"},
 {"SS-CAL-001","invalid_calibration","校准点无效，请按提示顺序重新选择"}
};
static_assert(sizeof(Errors)/sizeof(Errors[0])==static_cast<unsigned>(ErrorId::Count));
inline const ErrorInfo& errorInfo(ErrorId id){
 auto index=static_cast<unsigned>(id);
 return Errors[index<static_cast<unsigned>(ErrorId::Count)?index:static_cast<unsigned>(ErrorId::UnknownGameGrid)];
}
inline std::string errorMessage(ErrorId id){
 const auto& info=errorInfo(id);
 return info.code[0]?std::string("[")+info.code+"] "+info.message:std::string();
}
}
