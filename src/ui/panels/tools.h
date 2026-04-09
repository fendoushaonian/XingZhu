#pragma once
#include "imgui.h"
#include <vector>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace sf {

// 工具面板导航
enum class ToolPanelType {
    Hub = 0,
    MemoryModifier = 1,
};

// 全局工具面板状态
inline ToolPanelType& GetCurrentToolPanel() {
    static ToolPanelType panel = ToolPanelType::Hub;
    return panel;
}

class ToolsPanel {
public:
    void Render(float x, float y, float width, float height);
};

class CardFarmerPanel {
public:
    CardFarmerPanel();
    void Render(float x, float y, float width, float height);

private:
    struct FarmGame {
        std::string name;
        std::string appId;
        int cardsRemaining;
        int cardsTotal;
        float hoursPlayed;
        bool isFarming;
    };
    std::vector<FarmGame> farmGames_;
    bool isRunning_ = false;
    int totalCardsDropped_ = 0;
};

class AchievementsPanel {
public:
    AchievementsPanel();
    void Render(float x, float y, float width, float height);

private:
    struct AchGame {
        std::string name;
        std::string appId;
        int unlocked;
        int total;
    };
    std::vector<AchGame> games_;
};

class CloudSavesPanel {
public:
    CloudSavesPanel();
    void Render(float x, float y, float width, float height);

private:
    struct SaveEntry {
        std::string gameName;
        std::string appId;
        std::string lastSync;
        float sizeMB;
        bool hasConflict;
    };
    std::vector<SaveEntry> saves_;
};

// 内存修改器面板 - 独立窗口，现代化界面
class MemoryModifierPanel {
public:
    MemoryModifierPanel();
    void Render();  // 独立窗口渲染

    void Open() { isOpen_ = true; }
    void Close() { isOpen_ = false; }
    bool IsOpen() const { return isOpen_; }

private:
    void RenderTitleBar();
    void RenderSidebar();
    void RenderMainContent();
    void RenderProcessSelector();
    void RenderSearchPanel();
    void RenderResultsPanel();
    void RenderLockedPanel();
    void RenderTutorial();
    void RenderOnlineWarning();
    void RenderStatusBar();

    // 窗口状态
    bool isOpen_ = false;
    bool isDragging_ = false;
    ImVec2 dragOffset_;
    ImVec2 windowPos_ = ImVec2(100, 100);
    ImVec2 windowSize_ = ImVec2(1100, 700);

    // 侧边栏选项
    int sidebarTab_ = 0;  // 0=搜索, 1=锁定, 2=教程

    // 状态
    bool showTutorial_ = true;
    bool showOnlineWarning_ = false;
    std::string warningMessage_;

    // 进程选择
    std::vector<std::pair<DWORD, std::string>> processList_;
    int selectedProcess_ = -1;
    char processFilter_[128] = {};
    DWORD lastProcessRefresh_ = 0;

    // 搜索
    int valueType_ = 2;  // 默认4字节整数
    int scanType_ = 0;   // 默认精确值
    char searchValue_[64] = {};
    char searchValue2_[64] = {};

    // 结果显示
    int resultPage_ = 0;
    static constexpr int RESULTS_PER_PAGE = 50;

    // 编辑
    char editValue_[64] = {};
    char lockName_[64] = {};
};

} // namespace sf
