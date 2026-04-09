#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "ui/panels/header.h"
#include "ui/panels/sidebar.h"
#include "ui/panels/library.h"
#include "ui/panels/login.h"
#include "ui/panels/profile.h"
#include "ui/panels/vip.h"
#include "ui/panels/game_properties.h"
#include "ui/panels/game_verify.h"
#include "ui/panels/game_install.h"
#include "ui/panels/game_uninstall.h"
#include "ui/panels/checkin.h"
#include "ui/panels/lottery.h"
#include "ui/panels/task.h"
#include "ui/panels/shop.h"
#include "ui/panels/tools.h"

namespace sf {

class Application {
public:
    Application();
    ~Application();

    bool Init(HINSTANCE hInstance);
    int  Run();
    void Shutdown();

    // 更新游戏安装状态
    void UpdateGameInstallStatus(const std::string& appId, bool installed) {
        sidebar_.UpdateGameInstallStatus(appId, installed);
    }

    // 添加游戏到侧边栏
    void AddGameToSidebar(const std::string& appId, const std::string& name) {
        sidebar_.AddGameToList(appId, name);
    }

    // 获取全局实例
    static Application* GetInstance() { return s_instance; }

    // 打开签到面板
    void OpenCheckInPanel() { checkInPanel_.Open(); }

    // 打开抽奖面板
    void OpenLotteryPanel() { lotteryPanel_.Open(); }

    // 打开任务面板
    void OpenTaskPanel() { taskPanel_.Open(); }

    // 打开商城面板
    void OpenShopPanel() { shopPanel_.Open(); }

    // 打开内存修改器面板
    void OpenMemoryModifierPanel() { memoryModifierPanel_.Open(); }

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    bool CreateDeviceD3D(HWND hWnd);
    void CleanupDeviceD3D();
    void CreateRenderTarget();
    void CleanupRenderTarget();
    void RenderFrame();

    // Win32
    HWND hwnd_ = nullptr;
    WNDCLASSEXW wc_ = {};

    // DirectX 11
    ID3D11Device*           d3dDevice_      = nullptr;
    ID3D11DeviceContext*    d3dDeviceCtx_   = nullptr;
    IDXGISwapChain*         swapChain_      = nullptr;
    ID3D11RenderTargetView* mainRTV_        = nullptr;
    bool                    swapChainOccluded_ = false;
    UINT                    resizeWidth_    = 0;
    UINT                    resizeHeight_   = 0;

    // UI Panels — Steam layout
    LoginPanel      loginPanel_;
    HeaderBar       header_;
    GameListSidebar sidebar_;
    LibraryContent  libraryContent_;
    StorePage       storePage_;
    CommunityPage   communityPage_;
    StatusBar       statusBar_;
    ProfilePanel        profilePanel_;
    VipPanel            vipPanel_;
    GamePropertiesPanel gamePropsPanel_;
    GameVerifyPanel     gameVerifyPanel_;
    GameInstallPanel    gameInstallPanel_;
    GameUninstallPanel  gameUninstallPanel_;
    CheckInPanel        checkInPanel_;
    LotteryPanel        lotteryPanel_;
    TaskPanel           taskPanel_;
    ShopPanel           shopPanel_;
    ToolsPanel          toolsPanel_;
    MemoryModifierPanel memoryModifierPanel_;

    // Deferred logout flag (processed at start of next frame)
    bool pendingLogout_ = false;

    static Application* s_instance;
};

} // namespace sf
