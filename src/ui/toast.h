#pragma once
#include "imgui.h"
#include <string>
#include <vector>
#include <mutex>

namespace sf {

enum class ToastType { Info, Warning, Error, Success };

struct ToastMessage {
    std::string text;
    ToastType type;
    float lifetime;      // total seconds to show
    float elapsed;       // time elapsed
};

// 游戏卡片通知（安装完成等）
struct GameCardNotification {
    std::string appId;
    std::string gameName;
    std::string message;     // 如 "安装完成"
    float lifetime;          // 总显示时间
    float elapsed;           // 已过时间
};

// Thread-safe toast API
void ShowToast(const std::string& text, ToastType type = ToastType::Info, float duration = 4.0f);
void RenderToasts(float winW, float winH);

// 游戏卡片通知 API
void ShowGameCardNotification(const std::string& appId, const std::string& gameName,
                               const std::string& message, float duration = 4.0f);
void RenderGameCardNotifications(float winW, float winH);

} // namespace sf
