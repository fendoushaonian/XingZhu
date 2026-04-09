#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <functional>
#include <string>

namespace sf {

// 悬浮球 - 独立系统级窗口，可在软件外显示
class FloatingBall {
public:
    FloatingBall();
    ~FloatingBall();

    // 初始化
    bool Init(HINSTANCE hInstance);

    // 关闭
    void Shutdown();

    // 显示/隐藏
    void Show();
    void Hide();
    bool IsVisible() const { return visible_; }

    // 设置加速器回调
    void SetBoosterCallback(std::function<void()> callback) { boosterCallback_ = callback; }

    // 加速器状态
    bool IsBoosterEnabled() const { return boosterEnabled_; }
    void SetBoosterEnabled(bool enabled);

    // 更新（在主循环中调用）
    void Update();

    // 窗口过程
    static LRESULT CALLBACK BallWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK MenuWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    void CreateBallWindow(HINSTANCE hInstance);
    void CreateMenuWindow(HINSTANCE hInstance);
    void UpdateBallImage();
    void UpdateMenuImage();
    void ToggleMenu();
    void CloseMenu();
    HBITMAP LoadLogoBitmap();

    // 窗口句柄
    HWND ballHwnd_ = nullptr;
    HWND menuHwnd_ = nullptr;
    HINSTANCE hInstance_ = nullptr;

    // Logo 位图
    HBITMAP logoBitmap_ = nullptr;
    int logoWidth_ = 0;
    int logoHeight_ = 0;

    // 状态
    bool visible_ = false;
    bool menuOpen_ = false;
    bool boosterEnabled_ = false;
    bool dragging_ = false;
    POINT dragStart_ = {};
    POINT windowStart_ = {};
    DWORD lastClickTime_ = 0;

    // 动画
    float rotation_ = 0.0f;
    float pulsePhase_ = 0.0f;
    float hoverScale_ = 1.0f;       // 悬停缩放
    float targetScale_ = 1.0f;      // 目标缩放
    float menuAnimProgress_ = 0.0f; // 菜单展开动画
    bool isHovered_ = false;        // 鼠标悬停状态
    DWORD lastUpdateTime_ = 0;

    // 精灵动画状态
    float blinkTimer_ = 0.0f;       // 眨眼计时器
    float blinkProgress_ = 0.0f;    // 眨眼进度 0-1
    float emotionTimer_ = 0.0f;     // 表情切换计时器
    int currentEmotion_ = 0;        // 当前表情 0=开心 1=惊讶 2=眯眼笑 3=星星眼
    float bouncePhase_ = 0.0f;      // 弹跳相位
    float squishX_ = 1.0f;          // 横向挤压
    float squishY_ = 1.0f;          // 纵向挤压
    float targetSquishX_ = 1.0f;
    float targetSquishY_ = 1.0f;
    float sparklePhase_ = 0.0f;     // 闪烁星星相位
    float wobblePhase_ = 0.0f;      // 摇摆相位

    // 菜单悬停
    int hoveredItem_ = -1;
    float itemHoverProgress_[4] = {0}; // 菜单项悬停动画

    // 回调
    std::function<void()> boosterCallback_;

    // 尺寸
    static constexpr int BALL_SIZE = 56;
    static constexpr int MENU_WIDTH = 160;
    static constexpr int MENU_ITEM_HEIGHT = 44;
};

// 全局访问
FloatingBall& GetFloatingBall();

} // namespace sf
