#include "ui/panels/tools.h"
#include "core/memory_editor.h"
#include "core/online_detector.h"
#include "ui/widgets/modern.h"
#include "ui/icons.h"
#include <cstdio>
#include <algorithm>

namespace sf {

MemoryModifierPanel::MemoryModifierPanel() {
    processList_ = MemoryEditor::GetProcessList();
}

void MemoryModifierPanel::Render() {
    if (!isOpen_) return;

    // 窗口居中显示（首次打开时）
    static bool firstOpen = true;
    if (firstOpen) {
        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        windowPos_ = ImVec2((displaySize.x - windowSize_.x) * 0.5f,
                            (displaySize.y - windowSize_.y) * 0.5f);
        firstOpen = false;
    }

    ImGui::SetNextWindowPos(windowPos_);
    ImGui::SetNextWindowSize(windowSize_);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(18, 20, 28, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(60, 65, 80, 255));

    if (ImGui::Begin(u8"##MemoryModifierWindow", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winPos = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();

        // 渲染标题栏
        RenderTitleBar();

        // 主内容区域
        float titleBarH = 48.0f;
        float statusBarH = 32.0f;
        float contentH = winSize.y - titleBarH - statusBarH;

        ImGui::SetCursorPos(ImVec2(0, titleBarH));

        // 左侧边栏
        float sidebarW = 220.0f;
        ImGui::BeginChild("##Sidebar", ImVec2(sidebarW, contentH), false);
        RenderSidebar();
        ImGui::EndChild();

        // 分隔线
        dl->AddLine(ImVec2(winPos.x + sidebarW, winPos.y + titleBarH),
                    ImVec2(winPos.x + sidebarW, winPos.y + titleBarH + contentH),
                    IM_COL32(50, 55, 70, 255), 1.0f);

        // 右侧主内容
        ImGui::SetCursorPos(ImVec2(sidebarW + 1, titleBarH));
        ImGui::BeginChild("##MainContent", ImVec2(winSize.x - sidebarW - 1, contentH), false);
        RenderMainContent();
        ImGui::EndChild();

        // 状态栏
        ImGui::SetCursorPos(ImVec2(0, titleBarH + contentH));
        RenderStatusBar();

        // 联机警告弹窗
        if (showOnlineWarning_) {
            RenderOnlineWarning();
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);

    // 更新锁定值
    if (isOpen_) {
        GetMemoryEditor().UpdateLocks();
    }
}

void MemoryModifierPanel::RenderTitleBar() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    float titleBarH = 48.0f;

    // 标题栏背景渐变
    dl->AddRectFilledMultiColor(
        winPos,
        ImVec2(winPos.x + winSize.x, winPos.y + titleBarH),
        IM_COL32(35, 40, 55, 255), IM_COL32(35, 40, 55, 255),
        IM_COL32(25, 28, 38, 255), IM_COL32(25, 28, 38, 255));

    // 底部分隔线
    dl->AddLine(ImVec2(winPos.x, winPos.y + titleBarH),
                ImVec2(winPos.x + winSize.x, winPos.y + titleBarH),
                IM_COL32(60, 65, 80, 255), 1.0f);

    // 图标 - 内存芯片
    float iconX = winPos.x + 20;
    float iconY = winPos.y + 12;
    icons::DrawMemory(dl, ImVec2(iconX, iconY), 24, IM_COL32(255, 100, 100, 255));

    // 标题文字
    ImFont* font = ImGui::GetFont();
    dl->AddText(font, font->FontSize * 1.2f,
                ImVec2(winPos.x + 54, winPos.y + 14),
                IM_COL32(255, 255, 255, 245),
                u8"\u661F\u94F8\u6E38\u620F\u4FEE\u6539\u5668");  // 星铸游戏修改器

    // 版本号
    dl->AddText(font, font->FontSize * 0.85f,
                ImVec2(winPos.x + 200, winPos.y + 18),
                IM_COL32(120, 130, 150, 200),
                "v1.0.1");

    // 拖动区域
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::InvisibleButton("##TitleDrag", ImVec2(winSize.x - 120, titleBarH));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        windowPos_.x += ImGui::GetIO().MouseDelta.x;
        windowPos_.y += ImGui::GetIO().MouseDelta.y;
    }

    // 关闭按钮
    ImGui::SetCursorPos(ImVec2(winSize.x - 48, 8));
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(200, 60, 60, 200));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(180, 40, 40, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

    if (ImGui::Button(u8"\u2715", ImVec2(32, 32))) {  // ✕
        isOpen_ = false;
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
}

void MemoryModifierPanel::RenderSidebar() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    float height = ImGui::GetContentRegionAvail().y;

    // 侧边栏背景 - 深色渐变
    dl->AddRectFilledMultiColor(pos, ImVec2(pos.x + width, pos.y + height),
        IM_COL32(22, 24, 35, 255), IM_COL32(22, 24, 35, 255),
        IM_COL32(16, 18, 28, 255), IM_COL32(16, 18, 28, 255));

    // 导航标签
    struct SideTab {
        const char* icon;
        const char* label;
        ImU32 accentColor;
    };
    SideTab tabs[] = {
        {u8"\xEF\x80\x82", u8"\u641C\u7D22\u5185\u5B58", IM_COL32(80, 160, 255, 255)},   // 搜索内存
        {u8"\xEF\x80\xA3", u8"\u9501\u5B9A\u5217\u8868", IM_COL32(255, 160, 80, 255)},   // 锁定列表
        {u8"\xEF\x81\x99", u8"\u4F7F\u7528\u6559\u7A0B", IM_COL32(80, 200, 140, 255)},   // 使用教程
    };

    float btnH = 52;
    float btnMargin = 10;
    float startY = 12;

    for (int i = 0; i < 3; i++) {
        float btnY = startY + i * (btnH + 6);
        ImVec2 btnPos = ImVec2(pos.x + btnMargin, pos.y + btnY);
        float btnW = width - btnMargin * 2;

        ImGui::SetCursorPos(ImVec2(btnMargin, btnY));
        ImGui::InvisibleButton(tabs[i].label, ImVec2(btnW, btnH));

        bool hovered = ImGui::IsItemHovered();
        bool isActive = (sidebarTab_ == i);
        if (ImGui::IsItemClicked()) sidebarTab_ = i;

        // 按钮背景
        if (isActive) {
            // 激活状态 - 渐变背景
            ImU32 col1 = IM_COL32((tabs[i].accentColor >> 0) & 0xFF,
                                   (tabs[i].accentColor >> 8) & 0xFF,
                                   (tabs[i].accentColor >> 16) & 0xFF, 40);
            ImU32 col2 = IM_COL32((tabs[i].accentColor >> 0) & 0xFF,
                                   (tabs[i].accentColor >> 8) & 0xFF,
                                   (tabs[i].accentColor >> 16) & 0xFF, 15);
            dl->AddRectFilledMultiColor(btnPos, ImVec2(btnPos.x + btnW, btnPos.y + btnH),
                col1, col2, col2, col1);
            dl->AddRect(btnPos, ImVec2(btnPos.x + btnW, btnPos.y + btnH),
                IM_COL32((tabs[i].accentColor >> 0) & 0xFF,
                         (tabs[i].accentColor >> 8) & 0xFF,
                         (tabs[i].accentColor >> 16) & 0xFF, 100), 10.0f, 0, 1.5f);
            // 左侧高亮条
            dl->AddRectFilled(ImVec2(btnPos.x, btnPos.y + 8),
                ImVec2(btnPos.x + 4, btnPos.y + btnH - 8), tabs[i].accentColor, 2.0f);
        } else if (hovered) {
            dl->AddRectFilled(btnPos, ImVec2(btnPos.x + btnW, btnPos.y + btnH),
                IM_COL32(255, 255, 255, 15), 10.0f);
        }

        // 图标背景圆
        float iconCenterX = btnPos.x + 28;
        float iconCenterY = btnPos.y + btnH / 2;
        ImU32 iconBgCol = isActive ? IM_COL32((tabs[i].accentColor >> 0) & 0xFF,
                                              (tabs[i].accentColor >> 8) & 0xFF,
                                              (tabs[i].accentColor >> 16) & 0xFF, 50)
                                   : IM_COL32(50, 55, 70, 150);
        dl->AddCircleFilled(ImVec2(iconCenterX, iconCenterY), 16, iconBgCol);

        // 图标符号
        ImU32 iconCol = isActive ? tabs[i].accentColor : IM_COL32(140, 150, 170, 220);
        const char* iconSymbol = (i == 0) ? u8"\u25CE" : (i == 1) ? u8"\u25A0" : u8"?";
        ImVec2 iconTextSize = ImGui::CalcTextSize(iconSymbol);
        dl->AddText(ImVec2(iconCenterX - iconTextSize.x/2, iconCenterY - iconTextSize.y/2),
            iconCol, iconSymbol);

        // 标签文字
        ImU32 textCol = isActive ? IM_COL32(255, 255, 255, 255) : IM_COL32(170, 180, 200, 220);
        dl->AddText(ImGui::GetFont(), 15.0f, ImVec2(btnPos.x + 52, btnPos.y + btnH/2 - 7),
            textCol, tabs[i].label);
    }

    // 分隔线
    float sepY = pos.y + startY + 3 * (btnH + 6) + 8;
    dl->AddLine(ImVec2(pos.x + 20, sepY), ImVec2(pos.x + width - 20, sepY),
        IM_COL32(60, 65, 85, 150), 1.0f);

    // 进程选择区域
    float procY = sepY - pos.y + 16;
    dl->AddText(ImGui::GetFont(), 12.0f, ImVec2(pos.x + 18, pos.y + procY),
        IM_COL32(100, 110, 130, 255), u8"\u76EE\u6807\u8FDB\u7A0B");

    ImGui::SetCursorPos(ImVec2(12, procY + 22));
    RenderProcessSelector();
}

void MemoryModifierPanel::RenderMainContent() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    float height = ImGui::GetContentRegionAvail().y;

    // 主内容区背景
    dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(18, 20, 28, 255));

    // 内边距
    float padding = 20.0f;
    ImGui::SetCursorScreenPos(ImVec2(pos.x + padding, pos.y + padding));

    switch (sidebarTab_) {
        case 0:  // 搜索
            RenderSearchPanel();
            RenderResultsPanel();
            break;
        case 1:  // 锁定列表
            RenderLockedPanel();
            break;
        case 2:  // 教程
            RenderTutorial();
            break;
    }
}

void MemoryModifierPanel::RenderStatusBar() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    float statusBarH = 32.0f;
    float y = winPos.y + winSize.y - statusBarH;

    // 背景
    dl->AddRectFilled(ImVec2(winPos.x, y),
                      ImVec2(winPos.x + winSize.x, winPos.y + winSize.y),
                      IM_COL32(22, 25, 35, 255));

    // 顶部分隔线
    dl->AddLine(ImVec2(winPos.x, y), ImVec2(winPos.x + winSize.x, y),
                IM_COL32(50, 55, 70, 255), 1.0f);

    auto& editor = GetMemoryEditor();
    ImFont* font = ImGui::GetFont();

    // 进程状态
    if (editor.IsAttached()) {
        dl->AddCircleFilled(ImVec2(winPos.x + 16, y + 16), 5, IM_COL32(100, 220, 100, 255));
        char statusBuf[256];
        snprintf(statusBuf, sizeof(statusBuf), u8"\u5DF2\u9644\u52A0: %s (PID: %lu)",
                 editor.GetProcessName().c_str(), editor.GetAttachedPid());
        dl->AddText(ImVec2(winPos.x + 28, y + 8), IM_COL32(180, 200, 180, 255), statusBuf);
    } else {
        dl->AddCircleFilled(ImVec2(winPos.x + 16, y + 16), 5, IM_COL32(150, 150, 150, 200));
        dl->AddText(ImVec2(winPos.x + 28, y + 8), IM_COL32(150, 160, 170, 200),
                    u8"\u672A\u9644\u52A0\u8FDB\u7A0B");  // 未附加进程
    }

    // 结果数量
    size_t resultCount = editor.GetResultCount();
    if (resultCount > 0) {
        char resultBuf[64];
        snprintf(resultBuf, sizeof(resultBuf), u8"\u627E\u5230 %zu \u4E2A\u7ED3\u679C", resultCount);
        ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, resultBuf);
        dl->AddText(ImVec2(winPos.x + winSize.x - textSize.x - 20, y + 8),
                    IM_COL32(100, 180, 255, 255), resultBuf);
    }

    // 锁定数量
    size_t lockCount = editor.GetLockedAddresses().size();
    if (lockCount > 0) {
        char lockBuf[64];
        snprintf(lockBuf, sizeof(lockBuf), u8"\u9501\u5B9A: %zu", lockCount);
        ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, lockBuf);
        dl->AddText(ImVec2(winPos.x + winSize.x - textSize.x - 150, y + 8),
                    IM_COL32(255, 180, 100, 255), lockBuf);
    }
}

void MemoryModifierPanel::RenderProcessSelector() {
    float width = 196.0f;

    // 刷新进程列表
    DWORD now = GetTickCount();
    if (now - lastProcessRefresh_ > 3000) {
        processList_ = MemoryEditor::GetProcessList();
        lastProcessRefresh_ = now;
    }

    auto& editor = GetMemoryEditor();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 当前附加状态卡片
    if (editor.IsAttached()) {
        ImVec2 pos = ImGui::GetCursorScreenPos();

        // 渐变背景
        dl->AddRectFilledMultiColor(pos, ImVec2(pos.x + width, pos.y + 64),
            IM_COL32(35, 65, 50, 255), IM_COL32(30, 55, 45, 255),
            IM_COL32(30, 55, 45, 255), IM_COL32(35, 65, 50, 255));
        dl->AddRect(pos, ImVec2(pos.x + width, pos.y + 64),
            IM_COL32(70, 160, 100, 150), 10.0f);

        // 状态指示灯 - 带光晕
        dl->AddCircleFilled(ImVec2(pos.x + 16, pos.y + 22), 8, IM_COL32(80, 220, 100, 40));
        dl->AddCircleFilled(ImVec2(pos.x + 16, pos.y + 22), 5, IM_COL32(80, 220, 100, 255));

        // 进程名
        dl->AddText(ImGui::GetFont(), 13.0f, ImVec2(pos.x + 30, pos.y + 14), IM_COL32(200, 255, 210, 255),
            editor.GetProcessName().c_str());

        // PID
        char pidBuf[32];
        snprintf(pidBuf, sizeof(pidBuf), "PID: %lu", editor.GetAttachedPid());
        dl->AddText(ImGui::GetFont(), 11.0f, ImVec2(pos.x + 30, pos.y + 36), IM_COL32(130, 175, 145, 200), pidBuf);

        // 分离按钮
        ImGui::SetCursorScreenPos(ImVec2(pos.x + width - 52, pos.y + 18));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(150, 55, 55, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(180, 70, 70, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(130, 45, 45, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        if (ImGui::Button(u8"\u65AD\u5F00", ImVec2(44, 26))) {
            editor.DetachProcess();
            selectedProcess_ = -1;
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + 72));
    }

    // 搜索框
    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(22, 26, 38, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(28, 32, 45, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(55, 65, 90, 200));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(210, 215, 230, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 9));
    ImGui::SetNextItemWidth(width);
    ImGui::InputTextWithHint("##processFilter", u8"\u641C\u7D22\u8FDB\u7A0B\u540D\u79F0...", processFilter_, sizeof(processFilter_));
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(4);

    ImGui::Spacing();

    // 进程列表
    float listH = editor.IsAttached() ? 135 : 205;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(18, 21, 32, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 5.0f);
    ImGui::BeginChild("##ProcessList", ImVec2(width, listH), true);

    std::string filterLower = processFilter_;
    std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    ImDrawList* dlList = ImGui::GetWindowDrawList();
    int idx = 0;
    for (const auto& [pid, name] : processList_) {
        if (!filterLower.empty()) {
            std::string nameLower = name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (nameLower.find(filterLower) == std::string::npos) {
                idx++;
                continue;
            }
        }

        ImGui::PushID(idx);
        bool isSelected = (selectedProcess_ == idx);

        ImVec2 itemPos = ImGui::GetCursorScreenPos();
        float itemW = ImGui::GetContentRegionAvail().x;
        float itemH = 28;

        ImGui::InvisibleButton("##item", ImVec2(itemW, itemH));
        bool hovered = ImGui::IsItemHovered();

        if (ImGui::IsItemClicked()) {
            selectedProcess_ = idx;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            if (OnlineDetector::Get().IsOnlineGame(name)) {
                warningMessage_ = OnlineDetector::Get().GetDetectionReason();
                showOnlineWarning_ = true;
            } else {
                editor.AttachProcess(pid);
            }
        }

        // 背景
        ImU32 bgCol = IM_COL32(0, 0, 0, 0);
        if (isSelected) {
            bgCol = IM_COL32(220, 80, 90, 60);
        } else if (hovered) {
            bgCol = IM_COL32(255, 255, 255, 18);
        }
        dlList->AddRectFilled(itemPos, ImVec2(itemPos.x + itemW, itemPos.y + itemH), bgCol, 6.0f);
        if (isSelected) {
            dlList->AddRect(itemPos, ImVec2(itemPos.x + itemW, itemPos.y + itemH),
                IM_COL32(220, 80, 90, 120), 6.0f);
        }

        // 文字
        ImU32 textCol = isSelected ? IM_COL32(255, 210, 215, 255) : IM_COL32(175, 185, 205, 220);
        dlList->AddText(ImGui::GetFont(), 12.0f, ImVec2(itemPos.x + 10, itemPos.y + 7), textCol, name.c_str());

        ImGui::PopID();
        idx++;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    // 附加按钮
    if (selectedProcess_ >= 0 && selectedProcess_ < (int)processList_.size() && !editor.IsAttached()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(220, 75, 85, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(240, 95, 105, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(200, 65, 75, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);

        if (ImGui::Button(u8"\u9644\u52A0\u8FDB\u7A0B", ImVec2(width, 38))) {
            const auto& [pid, name] = processList_[selectedProcess_];
            if (OnlineDetector::Get().IsOnlineGame(name)) {
                warningMessage_ = OnlineDetector::Get().GetDetectionReason();
                showOnlineWarning_ = true;
            } else {
                editor.AttachProcess(pid);
            }
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }
}

void MemoryModifierPanel::RenderSearchPanel() {
    auto& editor = GetMemoryEditor();
    bool canSearch = editor.IsAttached() && !editor.IsScanning();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    float cardW = ImGui::GetContentRegionAvail().x - 20;
    float cardH = 190.0f;

    // 卡片背景 - 玻璃质感
    dl->AddRectFilled(startPos, ImVec2(startPos.x + cardW, startPos.y + cardH),
        IM_COL32(28, 32, 48, 250), 12.0f);
    // 顶部高光
    dl->AddRectFilledMultiColor(startPos, ImVec2(startPos.x + cardW, startPos.y + 50),
        IM_COL32(60, 70, 100, 40), IM_COL32(60, 70, 100, 40),
        IM_COL32(60, 70, 100, 0), IM_COL32(60, 70, 100, 0));
    // 边框渐变
    dl->AddRect(startPos, ImVec2(startPos.x + cardW, startPos.y + cardH),
        IM_COL32(80, 100, 140, 100), 12.0f, 0, 1.5f);

    // 标题区域
    float titleH = 44;
    dl->AddLine(ImVec2(startPos.x + 16, startPos.y + titleH),
        ImVec2(startPos.x + cardW - 16, startPos.y + titleH), IM_COL32(70, 80, 110, 100), 1.0f);

    // 图标背景
    dl->AddCircleFilled(ImVec2(startPos.x + 26, startPos.y + titleH/2), 14, IM_COL32(80, 140, 255, 50));
    dl->AddCircleFilled(ImVec2(startPos.x + 26, startPos.y + titleH/2), 6, IM_COL32(80, 160, 255, 255));

    // 标题文字
    dl->AddText(ImGui::GetFont(), 16.0f, ImVec2(startPos.x + 48, startPos.y + titleH/2 - 8),
        IM_COL32(240, 245, 255, 255), u8"\u641C\u7D22\u8BBE\u7F6E");

    // 内容区域
    float contentY = startPos.y + 56;
    float leftX = startPos.x + 20;
    float colW = (cardW - 60) * 0.5f;

    ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(20, 24, 38, 255));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(30, 36, 55, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(70, 85, 120, 150));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(230, 235, 245, 255));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(28, 32, 48, 250));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 8));

    // 第一行：数值类型 + 扫描方式
    ImGui::SetCursorScreenPos(ImVec2(leftX, contentY));
    dl->AddText(ImGui::GetFont(), 12.0f, ImVec2(leftX, contentY), IM_COL32(130, 145, 175, 255), u8"\u6570\u503C\u7C7B\u578B");
    ImGui::SetCursorScreenPos(ImVec2(leftX, contentY + 20));
    ImGui::SetNextItemWidth(colW);
    const char* valueTypes[] = { u8"1\u5B57\u8282", u8"2\u5B57\u8282", u8"4\u5B57\u8282", u8"8\u5B57\u8282", u8"\u5355\u7CBE\u5EA6", u8"\u53CC\u7CBE\u5EA6" };
    ImGui::Combo("##valueType", &valueType_, valueTypes, IM_ARRAYSIZE(valueTypes));

    float rightX = leftX + colW + 20;
    ImGui::SetCursorScreenPos(ImVec2(rightX, contentY));
    dl->AddText(ImGui::GetFont(), 12.0f, ImVec2(rightX, contentY), IM_COL32(130, 145, 175, 255), u8"\u626B\u63CF\u65B9\u5F0F");
    ImGui::SetCursorScreenPos(ImVec2(rightX, contentY + 20));
    ImGui::SetNextItemWidth(colW);
    const char* scanTypes[] = { u8"\u7CBE\u786E\u503C", u8"\u5927\u4E8E", u8"\u5C0F\u4E8E", u8"\u8303\u56F4\u5185", u8"\u672A\u77E5\u521D\u59CB\u503C" };
    const char* nextScanTypes[] = { u8"\u7CBE\u786E\u503C", u8"\u5927\u4E8E", u8"\u5C0F\u4E8E", u8"\u589E\u52A0\u4E86", u8"\u51CF\u5C11\u4E86", u8"\u53D8\u5316\u4E86", u8"\u672A\u53D8\u5316" };
    if (editor.GetResultCount() == 0) {
        ImGui::Combo("##scanType", &scanType_, scanTypes, IM_ARRAYSIZE(scanTypes));
    } else {
        ImGui::Combo("##scanType", &scanType_, nextScanTypes, IM_ARRAYSIZE(nextScanTypes));
    }

    // 第二行：搜索数值
    float row2Y = contentY + 56;
    ImGui::SetCursorScreenPos(ImVec2(leftX, row2Y));
    dl->AddText(ImGui::GetFont(), 12.0f, ImVec2(leftX, row2Y), IM_COL32(130, 145, 175, 255), u8"\u641C\u7D22\u6570\u503C");
    ImGui::SetCursorScreenPos(ImVec2(leftX, row2Y + 20));

    if (scanType_ == 3) {  // 范围搜索
        ImGui::SetNextItemWidth(colW);
        ImGui::InputText("##searchValue", searchValue_, sizeof(searchValue_));
        ImGui::SameLine(0, 10);
        ImGui::TextColored(ImVec4(0.55f, 0.6f, 0.7f, 1.0f), u8"\u5230");
        ImGui::SameLine(0, 10);
        ImGui::SetNextItemWidth(colW - 30);
        ImGui::InputText("##searchValue2", searchValue2_, sizeof(searchValue2_));
    } else {
        ImGui::SetNextItemWidth(cardW - 32);
        ImGui::InputText("##searchValue", searchValue_, sizeof(searchValue_));
    }

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(5);

    // 按钮区域
    float btnY = contentY + 108;
    float btnW = (cardW - 70) / 3;
    float btnH = 36;

    ImGui::SetCursorScreenPos(ImVec2(leftX, btnY));

    // 扫描按钮 - 渐变效果
    if (editor.GetResultCount() == 0) {
        ImGui::PushStyleColor(ImGuiCol_Button, canSearch ? IM_COL32(220, 70, 90, 255) : IM_COL32(60, 65, 85, 200));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(240, 90, 110, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(200, 60, 80, 255));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, canSearch ? IM_COL32(60, 140, 255, 255) : IM_COL32(60, 65, 85, 200));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 160, 255, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(50, 120, 220, 255));
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    ImGui::BeginDisabled(!canSearch);
    const char* scanBtnText = editor.GetResultCount() == 0 ? u8"\u9996\u6B21\u626B\u63CF" : u8"\u518D\u6B21\u626B\u63CF";
    if (ImGui::Button(scanBtnText, ImVec2(btnW, btnH))) {
        if (editor.GetResultCount() == 0) {
            editor.StartScan((ValueType)valueType_, (ScanCondition)scanType_, searchValue_, searchValue2_);
        } else {
            ScanCondition sc;
            switch (scanType_) {
                case 0: sc = ScanCondition::Exact; break;
                case 1: sc = ScanCondition::Greater; break;
                case 2: sc = ScanCondition::Less; break;
                case 3: sc = ScanCondition::Increased; break;
                case 4: sc = ScanCondition::Decreased; break;
                case 5: sc = ScanCondition::Changed; break;
                case 6: sc = ScanCondition::Unchanged; break;
                default: sc = ScanCondition::Exact; break;
            }
            editor.NextScan(sc, searchValue_, searchValue2_);
        }
    }
    ImGui::EndDisabled();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0, 12);

    // 重置按钮 - 更柔和的颜色
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(50, 55, 75, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(65, 72, 95, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(45, 50, 68, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    if (ImGui::Button(u8"\u91CD\u7F6E\u626B\u63CF", ImVec2(btnW, btnH))) {
        editor.ResetScan();
        searchValue_[0] = '\0';
        searchValue2_[0] = '\0';
        resultPage_ = 0;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    // 扫描进度
    if (editor.IsScanning()) {
        ImGui::SameLine(0, 12);
        float progress = editor.GetScanProgress();
        char progressText[32];
        snprintf(progressText, sizeof(progressText), u8"%.0f%%", progress * 100);

        ImVec2 barPos = ImGui::GetCursorScreenPos();
        // 进度条背景
        dl->AddRectFilled(barPos, ImVec2(barPos.x + btnW, barPos.y + btnH),
            IM_COL32(30, 35, 50, 255), 8.0f);
        // 进度条填充 - 渐变
        dl->AddRectFilledMultiColor(barPos, ImVec2(barPos.x + btnW * progress, barPos.y + btnH),
            IM_COL32(60, 140, 255, 255), IM_COL32(100, 180, 255, 255),
            IM_COL32(100, 180, 255, 255), IM_COL32(60, 140, 255, 255));
        // 进度条边框
        dl->AddRect(barPos, ImVec2(barPos.x + btnW, barPos.y + btnH),
            IM_COL32(70, 85, 120, 150), 8.0f);

        ImVec2 textSize = ImGui::CalcTextSize(progressText);
        dl->AddText(ImVec2(barPos.x + (btnW - textSize.x) / 2, barPos.y + (btnH - textSize.y) / 2),
                    IM_COL32(255, 255, 255, 255), progressText);
        ImGui::Dummy(ImVec2(btnW, btnH));
    }

    // 设置下一个面板的起始位置
    ImGui::SetCursorScreenPos(ImVec2(startPos.x, startPos.y + cardH + 16));
}

void MemoryModifierPanel::RenderResultsPanel() {
    auto& editor = GetMemoryEditor();
    size_t totalResults = editor.GetResultCount();

    ImVec2 startPos = ImGui::GetCursorScreenPos();
    float cardW = ImGui::GetContentRegionAvail().x - 20;
    float cardH = ImGui::GetContentRegionAvail().y - 10;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 卡片背景 - 玻璃质感
    dl->AddRectFilled(startPos, ImVec2(startPos.x + cardW, startPos.y + cardH),
        IM_COL32(28, 32, 48, 250), 12.0f);
    // 顶部高光
    dl->AddRectFilledMultiColor(startPos, ImVec2(startPos.x + cardW, startPos.y + 50),
        IM_COL32(60, 80, 100, 35), IM_COL32(60, 80, 100, 35),
        IM_COL32(60, 80, 100, 0), IM_COL32(60, 80, 100, 0));
    // 边框
    dl->AddRect(startPos, ImVec2(startPos.x + cardW, startPos.y + cardH),
        IM_COL32(80, 100, 140, 100), 12.0f, 0, 1.5f);

    // 标题区域
    float titleH = 44;
    dl->AddLine(ImVec2(startPos.x + 16, startPos.y + titleH),
        ImVec2(startPos.x + cardW - 16, startPos.y + titleH), IM_COL32(70, 80, 110, 100), 1.0f);

    // 图标背景
    dl->AddCircleFilled(ImVec2(startPos.x + 26, startPos.y + titleH/2), 14, IM_COL32(80, 200, 140, 50));
    dl->AddCircleFilled(ImVec2(startPos.x + 26, startPos.y + titleH/2), 6, IM_COL32(80, 220, 150, 255));

    // 标题文字
    dl->AddText(ImGui::GetFont(), 16.0f, ImVec2(startPos.x + 48, startPos.y + titleH/2 - 8),
        IM_COL32(240, 245, 255, 255), u8"\u626B\u63CF\u7ED3\u679C");

    // 结果数量标签
    char countBuf[64];
    snprintf(countBuf, sizeof(countBuf), u8"%zu \u4E2A\u7ED3\u679C", totalResults);
    ImVec2 countSize = ImGui::CalcTextSize(countBuf);
    // 数量背景
    float tagW = countSize.x + 16;
    float tagH = 22;
    float tagX = startPos.x + cardW - tagW - 16;
    float tagY = startPos.y + titleH/2 - tagH/2;
    dl->AddRectFilled(ImVec2(tagX, tagY), ImVec2(tagX + tagW, tagY + tagH),
        IM_COL32(80, 200, 140, 40), 11.0f);
    dl->AddText(ImVec2(tagX + 8, tagY + 3), IM_COL32(80, 220, 150, 255), countBuf);

    // 表头
    float headerY = startPos.y + 56;
    float colAddr = 140, colValue = 120;
    dl->AddText(ImGui::GetFont(), 12.0f, ImVec2(startPos.x + 20, headerY), IM_COL32(110, 125, 155, 255), u8"\u5185\u5B58\u5730\u5740");
    dl->AddText(ImGui::GetFont(), 12.0f, ImVec2(startPos.x + 20 + colAddr, headerY), IM_COL32(110, 125, 155, 255), u8"\u5F53\u524D\u6570\u503C");
    dl->AddText(ImGui::GetFont(), 12.0f, ImVec2(startPos.x + 20 + colAddr + colValue, headerY), IM_COL32(110, 125, 155, 255), u8"\u64CD\u4F5C");

    // 分隔线
    dl->AddLine(ImVec2(startPos.x + 16, headerY + 24),
        ImVec2(startPos.x + cardW - 16, headerY + 24), IM_COL32(60, 70, 95, 150));

    // 结果列表区域
    ImGui::SetCursorScreenPos(ImVec2(startPos.x + 8, startPos.y + 86));
    ImGui::BeginChild("##results_list", ImVec2(cardW - 16, cardH - 138), false);

    if (totalResults == 0) {
        float centerY = ImGui::GetContentRegionAvail().y / 2 - 30;
        ImGui::SetCursorPosY(centerY);

        // 空状态
        float textW = ImGui::CalcTextSize(u8"\u6682\u65E0\u626B\u63CF\u7ED3\u679C").x;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - textW) / 2);
        ImGui::TextColored(ImVec4(0.4f, 0.45f, 0.55f, 1.0f), u8"\u6682\u65E0\u626B\u63CF\u7ED3\u679C");

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
        textW = ImGui::CalcTextSize(u8"\u9009\u62E9\u8FDB\u7A0B\u5E76\u8F93\u5165\u6570\u503C\u5F00\u59CB\u626B\u63CF").x;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - textW) / 2);
        ImGui::TextColored(ImVec4(0.35f, 0.4f, 0.5f, 1.0f), u8"\u9009\u62E9\u8FDB\u7A0B\u5E76\u8F93\u5165\u6570\u503C\u5F00\u59CB\u626B\u63CF");
    } else {
        // 分页显示
        size_t startIdx = resultPage_ * RESULTS_PER_PAGE;
        size_t count = (std::min)(static_cast<size_t>(RESULTS_PER_PAGE), totalResults - startIdx);
        auto results = editor.GetResults(startIdx, count);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        for (size_t i = 0; i < results.size(); i++) {
            const auto& r = results[i];
            ImGui::PushID(static_cast<int>(startIdx + i));

            ImVec2 rowPos = ImGui::GetCursorScreenPos();
            float rowH = 32;

            // 行背景（悬停效果）
            bool hovered = ImGui::IsMouseHoveringRect(rowPos, ImVec2(rowPos.x + cardW - 32, rowPos.y + rowH));
            if (hovered) {
                dl->AddRectFilled(rowPos, ImVec2(rowPos.x + cardW - 32, rowPos.y + rowH),
                                  IM_COL32(50, 60, 80, 100), 4.0f);
            }

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);

            // 地址
            char addrBuf[32];
            snprintf(addrBuf, sizeof(addrBuf), "0x%llX", (unsigned long long)r.address);
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", addrBuf);

            // 当前值
            ImGui::SameLine(colAddr);
            ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.5f, 1.0f), "%s", r.displayValue.c_str());

            // 操作按钮
            ImGui::SameLine(colAddr + colValue);

            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(50, 120, 85, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(60, 150, 100, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(45, 100, 75, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
            if (ImGui::SmallButton(u8"\u4FEE\u6539")) {
                strncpy(editValue_, r.displayValue.c_str(), sizeof(editValue_) - 1);
                editValue_[sizeof(editValue_) - 1] = '\0';
                ImGui::OpenPopup("##edit_value");
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            ImGui::SameLine(0, 8);
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(170, 120, 45, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(200, 150, 60, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(150, 100, 40, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
            if (ImGui::SmallButton(u8"\u9501\u5B9A")) {
                lockName_[0] = '\0';
                strncpy(editValue_, r.displayValue.c_str(), sizeof(editValue_) - 1);
                editValue_[sizeof(editValue_) - 1] = '\0';
                ImGui::OpenPopup("##lock_value");
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            // 修改弹窗
            if (ImGui::BeginPopup("##edit_value")) {
                ImGui::Text(u8"\u8F93\u5165\u65B0\u6570\u503C:");
                ImGui::SetNextItemWidth(140);
                ImGui::InputText("##edit_input", editValue_, sizeof(editValue_));
                ImGui::Spacing();
                if (ImGui::Button(u8"\u786E\u5B9A", ImVec2(65, 26))) {
                    ValueType vt = static_cast<ValueType>(valueType_);
                    auto data = MemoryEditor::StringToValue(editValue_, vt);
                    editor.WriteMemory(r.address, data.data(), data.size());
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button(u8"\u53D6\u6D88", ImVec2(65, 26))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            // 锁定弹窗
            if (ImGui::BeginPopup("##lock_value")) {
                ImGui::Text(u8"\u9501\u5B9A\u540D\u79F0:");
                ImGui::SetNextItemWidth(140);
                ImGui::InputText("##lock_name", lockName_, sizeof(lockName_));
                ImGui::Text(u8"\u9501\u5B9A\u6570\u503C:");
                ImGui::SetNextItemWidth(140);
                ImGui::InputText("##lock_val", editValue_, sizeof(editValue_));
                ImGui::Spacing();
                if (ImGui::Button(u8"\u786E\u5B9A", ImVec2(65, 26))) {
                    LockedAddress lock;
                    lock.address = r.address;
                    lock.type = static_cast<ValueType>(valueType_);
                    lock.value = MemoryEditor::StringToValue(editValue_, lock.type);
                    lock.name = lockName_[0] ? lockName_ : addrBuf;
                    lock.enabled = true;
                    editor.AddLockedAddress(lock);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button(u8"\u53D6\u6D88", ImVec2(65, 26))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
            ImGui::PopID();
        }

        ImGui::PopStyleVar();
    }

    ImGui::EndChild();

    // 分页控制
    if (totalResults > RESULTS_PER_PAGE) {
        int totalPages = (int)((totalResults + RESULTS_PER_PAGE - 1) / RESULTS_PER_PAGE);
        ImGui::SetCursorScreenPos(ImVec2(startPos.x + 20, startPos.y + cardH - 42));

        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(45, 52, 72, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(60, 70, 95, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(40, 46, 65, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

        ImGui::BeginDisabled(resultPage_ == 0);
        if (ImGui::Button(u8"\u4E0A\u4E00\u9875", ImVec2(75, 28))) {
            resultPage_--;
        }
        ImGui::EndDisabled();

        ImGui::SameLine(0, 16);
        ImGui::TextColored(ImVec4(0.55f, 0.6f, 0.72f, 1.0f), u8"%d / %d", resultPage_ + 1, totalPages);

        ImGui::SameLine(0, 16);
        ImGui::BeginDisabled(resultPage_ >= totalPages - 1);
        if (ImGui::Button(u8"\u4E0B\u4E00\u9875", ImVec2(75, 28))) {
            resultPage_++;
        }
        ImGui::EndDisabled();

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }
}

void MemoryModifierPanel::RenderLockedPanel() {
    auto& editor = GetMemoryEditor();
    auto& locks = editor.GetLockedAddresses();

    ImVec2 startPos = ImGui::GetCursorScreenPos();
    float cardW = ImGui::GetContentRegionAvail().x - 20;
    float cardH = ImGui::GetContentRegionAvail().y - 10;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 卡片背景 - 玻璃质感
    dl->AddRectFilled(startPos, ImVec2(startPos.x + cardW, startPos.y + cardH),
        IM_COL32(28, 32, 48, 250), 12.0f);
    // 顶部高光
    dl->AddRectFilledMultiColor(startPos, ImVec2(startPos.x + cardW, startPos.y + 50),
        IM_COL32(80, 70, 50, 35), IM_COL32(80, 70, 50, 35),
        IM_COL32(80, 70, 50, 0), IM_COL32(80, 70, 50, 0));
    // 边框
    dl->AddRect(startPos, ImVec2(startPos.x + cardW, startPos.y + cardH),
        IM_COL32(100, 90, 70, 100), 12.0f, 0, 1.5f);

    // 标题区域
    float titleH = 44;
    dl->AddLine(ImVec2(startPos.x + 16, startPos.y + titleH),
        ImVec2(startPos.x + cardW - 16, startPos.y + titleH), IM_COL32(80, 75, 60, 100), 1.0f);

    // 图标背景
    dl->AddCircleFilled(ImVec2(startPos.x + 26, startPos.y + titleH/2), 14, IM_COL32(255, 170, 80, 50));
    dl->AddCircleFilled(ImVec2(startPos.x + 26, startPos.y + titleH/2), 6, IM_COL32(255, 180, 100, 255));

    // 标题文字
    dl->AddText(ImGui::GetFont(), 16.0f, ImVec2(startPos.x + 48, startPos.y + titleH/2 - 8),
        IM_COL32(240, 245, 255, 255), u8"\u9501\u5B9A\u5217\u8868");

    // 锁定数量标签
    char countBuf[64];
    snprintf(countBuf, sizeof(countBuf), u8"%zu \u4E2A\u9501\u5B9A", locks.size());
    ImVec2 countSize = ImGui::CalcTextSize(countBuf);
    float tagW = countSize.x + 16;
    float tagH = 22;
    float tagX = startPos.x + cardW - tagW - 16;
    float tagY = startPos.y + titleH/2 - tagH/2;
    dl->AddRectFilled(ImVec2(tagX, tagY), ImVec2(tagX + tagW, tagY + tagH),
        IM_COL32(255, 170, 80, 40), 11.0f);
    dl->AddText(ImVec2(tagX + 8, tagY + 3), IM_COL32(255, 180, 100, 255), countBuf);

    // 表头
    float headerY = startPos.y + 56;
    dl->AddText(ImGui::GetFont(), 12.0f, ImVec2(startPos.x + 20, headerY), IM_COL32(110, 125, 155, 255), u8"\u540D\u79F0");
    dl->AddText(ImGui::GetFont(), 12.0f, ImVec2(startPos.x + 130, headerY), IM_COL32(110, 125, 155, 255), u8"\u5730\u5740");
    dl->AddText(ImGui::GetFont(), 12.0f, ImVec2(startPos.x + 260, headerY), IM_COL32(110, 125, 155, 255), u8"\u9501\u5B9A\u503C");
    dl->AddText(ImGui::GetFont(), 12.0f, ImVec2(startPos.x + 360, headerY), IM_COL32(110, 125, 155, 255), u8"\u72B6\u6001");
    dl->AddText(ImGui::GetFont(), 12.0f, ImVec2(startPos.x + 440, headerY), IM_COL32(110, 125, 155, 255), u8"\u64CD\u4F5C");

    dl->AddLine(ImVec2(startPos.x + 16, headerY + 24),
        ImVec2(startPos.x + cardW - 16, headerY + 24), IM_COL32(60, 70, 95, 150));

    ImGui::SetCursorScreenPos(ImVec2(startPos.x + 8, startPos.y + 86));
    ImGui::BeginChild("##locks_list", ImVec2(cardW - 16, cardH - 98), false);

    if (locks.empty()) {
        float centerY = ImGui::GetContentRegionAvail().y / 2 - 30;
        ImGui::SetCursorPosY(centerY);

        float textW = ImGui::CalcTextSize(u8"\u6682\u65E0\u9501\u5B9A\u9879").x;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - textW) / 2);
        ImGui::TextColored(ImVec4(0.4f, 0.45f, 0.55f, 1.0f), u8"\u6682\u65E0\u9501\u5B9A\u9879");

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
        textW = ImGui::CalcTextSize(u8"\u5728\u626B\u63CF\u7ED3\u679C\u4E2D\u70B9\u51FB\"\u9501\u5B9A\"\u6DFB\u52A0").x;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - textW) / 2);
        ImGui::TextColored(ImVec4(0.35f, 0.4f, 0.5f, 1.0f), u8"\u5728\u626B\u63CF\u7ED3\u679C\u4E2D\u70B9\u51FB\"\u9501\u5B9A\"\u6DFB\u52A0");
    } else {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

        for (size_t i = 0; i < locks.size(); i++) {
            const auto& lock = locks[i];
            ImGui::PushID(static_cast<int>(i));

            ImVec2 rowPos = ImGui::GetCursorScreenPos();
            float rowH = 32;

            // 行背景
            bool hovered = ImGui::IsMouseHoveringRect(rowPos, ImVec2(rowPos.x + cardW - 32, rowPos.y + rowH));
            if (hovered) {
                dl->AddRectFilled(rowPos, ImVec2(rowPos.x + cardW - 32, rowPos.y + rowH),
                                  IM_COL32(50, 60, 80, 100), 4.0f);
            }

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);

            // 名称
            ImGui::TextColored(ImVec4(0.9f, 0.92f, 0.98f, 1.0f), "%s", lock.name.c_str());

            // 地址
            ImGui::SameLine(122);
            char addrBuf[32];
            snprintf(addrBuf, sizeof(addrBuf), "0x%llX", (unsigned long long)lock.address);
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", addrBuf);

            // 锁定值
            ImGui::SameLine(252);
            std::string valueStr = MemoryEditor::ValueToString(lock.value, lock.type);
            ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.5f, 1.0f), "%s", valueStr.c_str());

            // 状态
            ImGui::SameLine(352);
            if (lock.enabled) {
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), u8"\u5DF2\u542F\u7528");
            } else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), u8"\u5DF2\u7981\u7528");
            }

            // 操作按钮
            ImGui::SameLine(432);

            if (lock.enabled) {
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(100, 100, 60, 220));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(130, 130, 80, 255));
                if (ImGui::SmallButton(u8"\u7981\u7528")) {
                    editor.SetLockedEnabled(i, false);
                }
                ImGui::PopStyleColor(2);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(60, 130, 90, 220));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 160, 110, 255));
                if (ImGui::SmallButton(u8"\u542F\u7528")) {
                    editor.SetLockedEnabled(i, true);
                }
                ImGui::PopStyleColor(2);
            }

            ImGui::SameLine(0, 6);
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(150, 60, 60, 220));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(180, 80, 80, 255));
            if (ImGui::SmallButton(u8"\u5220\u9664")) {
                editor.RemoveLockedAddress(i);
            }
            ImGui::PopStyleColor(2);

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
            ImGui::PopID();
        }

        ImGui::PopStyleVar();
    }

    ImGui::EndChild();
}

void MemoryModifierPanel::RenderTutorial() {
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    float cardW = ImGui::GetContentRegionAvail().x - 20;
    float cardH = ImGui::GetContentRegionAvail().y - 10;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 卡片背景 - 玻璃质感
    dl->AddRectFilled(startPos, ImVec2(startPos.x + cardW, startPos.y + cardH),
        IM_COL32(28, 32, 48, 250), 12.0f);
    // 顶部高光
    dl->AddRectFilledMultiColor(startPos, ImVec2(startPos.x + cardW, startPos.y + 50),
        IM_COL32(60, 90, 80, 35), IM_COL32(60, 90, 80, 35),
        IM_COL32(60, 90, 80, 0), IM_COL32(60, 90, 80, 0));
    // 边框
    dl->AddRect(startPos, ImVec2(startPos.x + cardW, startPos.y + cardH),
        IM_COL32(80, 120, 100, 100), 12.0f, 0, 1.5f);

    // 标题区域
    float titleH = 44;
    dl->AddLine(ImVec2(startPos.x + 16, startPos.y + titleH),
        ImVec2(startPos.x + cardW - 16, startPos.y + titleH), IM_COL32(70, 90, 80, 100), 1.0f);

    // 图标背景
    dl->AddCircleFilled(ImVec2(startPos.x + 26, startPos.y + titleH/2), 14, IM_COL32(80, 200, 140, 50));
    dl->AddCircleFilled(ImVec2(startPos.x + 26, startPos.y + titleH/2), 6, IM_COL32(80, 220, 150, 255));

    // 标题文字
    dl->AddText(ImGui::GetFont(), 16.0f, ImVec2(startPos.x + 48, startPos.y + titleH/2 - 8),
        IM_COL32(240, 245, 255, 255), u8"\u4F7F\u7528\u6559\u7A0B");

    ImGui::SetCursorScreenPos(ImVec2(startPos.x + 8, startPos.y + 56));
    ImGui::BeginChild("##tutorial_content", ImVec2(cardW - 16, cardH - 68), false);

    // 步骤卡片样式
    float stepW = cardW - 40;
    auto RenderStep = [&](int num, const char* title, const char* desc, ImU32 color) {
        ImVec2 stepPos = ImGui::GetCursorScreenPos();

        // 步骤背景 - 渐变
        dl->AddRectFilledMultiColor(stepPos, ImVec2(stepPos.x + stepW, stepPos.y + 64),
            IM_COL32(35, 40, 58, 255), IM_COL32(32, 36, 52, 255),
            IM_COL32(32, 36, 52, 255), IM_COL32(35, 40, 58, 255));
        dl->AddRect(stepPos, ImVec2(stepPos.x + stepW, stepPos.y + 64),
            IM_COL32(55, 65, 90, 100), 8.0f);

        // 步骤编号圆圈 - 带光晕
        dl->AddCircleFilled(ImVec2(stepPos.x + 28, stepPos.y + 32), 18, IM_COL32((color >> 0) & 0xFF, (color >> 8) & 0xFF, (color >> 16) & 0xFF, 30));
        dl->AddCircleFilled(ImVec2(stepPos.x + 28, stepPos.y + 32), 14, color);
        char numStr[4];
        snprintf(numStr, sizeof(numStr), "%d", num);
        ImVec2 numSize = ImGui::CalcTextSize(numStr);
        dl->AddText(ImGui::GetFont(), 15.0f, ImVec2(stepPos.x + 28 - numSize.x / 2, stepPos.y + 32 - 7),
            IM_COL32(255, 255, 255, 255), numStr);

        // 标题
        dl->AddText(ImGui::GetFont(), 15.0f, ImVec2(stepPos.x + 56, stepPos.y + 12), IM_COL32(240, 245, 255, 255), title);

        // 描述
        ImGui::SetCursorScreenPos(ImVec2(stepPos.x + 56, stepPos.y + 34));
        ImGui::PushTextWrapPos(stepPos.x + stepW - 16);
        ImGui::TextColored(ImVec4(0.52f, 0.58f, 0.7f, 1.0f), "%s", desc);
        ImGui::PopTextWrapPos();

        ImGui::SetCursorScreenPos(ImVec2(stepPos.x, stepPos.y + 72));
    };

    RenderStep(1, u8"\u9009\u62E9\u76EE\u6807\u8FDB\u7A0B",
               u8"\u5728\u5DE6\u4FA7\u8FDB\u7A0B\u5217\u8868\u4E2D\u627E\u5230\u6E38\u620F\uFF0C\u53CC\u51FB\u6216\u70B9\u51FB\"\u9644\u52A0\u8FDB\u7A0B\"\u8FDE\u63A5",
               IM_COL32(100, 180, 255, 255));

    RenderStep(2, u8"\u8BBE\u7F6E\u626B\u63CF\u53C2\u6570",
               u8"\u9009\u62E9\u6570\u503C\u7C7B\u578B\uFF08\u6574\u6570\u901A\u5E38\u7528 4\u5B57\u8282\uFF09\u548C\u626B\u63CF\u65B9\u5F0F",
               IM_COL32(255, 180, 100, 255));

    RenderStep(3, u8"\u9996\u6B21\u626B\u63CF",
               u8"\u8F93\u5165\u6E38\u620F\u4E2D\u7684\u5F53\u524D\u6570\u503C\uFF08\u5982\u91D1\u5E01\u6570\u91CF\uFF09\uFF0C\u70B9\u51FB\"\u9996\u6B21\u626B\u63CF\"",
               IM_COL32(100, 220, 150, 255));

    RenderStep(4, u8"\u7F29\u5C0F\u8303\u56F4",
               u8"\u56DE\u6E38\u620F\u6539\u53D8\u6570\u503C\uFF0C\u518D\u8F93\u5165\u65B0\u503C\u70B9\u51FB\"\u518D\u6B21\u626B\u63CF\"\uFF0C\u91CD\u590D\u76F4\u5230\u7ED3\u679C\u5F88\u5C11",
               IM_COL32(200, 150, 255, 255));

    RenderStep(5, u8"\u4FEE\u6539\u6216\u9501\u5B9A",
               u8"\u627E\u5230\u6B63\u786E\u5730\u5740\u540E\uFF0C\u70B9\u51FB\"\u4FEE\u6539\"\u76F4\u63A5\u6539\u53D8\uFF0C\u6216\"\u9501\u5B9A\"\u4FDD\u6301\u6570\u503C\u4E0D\u53D8",
               IM_COL32(255, 100, 130, 255));

    ImGui::Spacing();
    ImGui::Spacing();

    // 提示卡片 - 更现代的设计
    ImVec2 tipPos = ImGui::GetCursorScreenPos();
    float tipW = stepW;
    float tipH = 95;
    // 背景渐变
    dl->AddRectFilledMultiColor(tipPos, ImVec2(tipPos.x + tipW, tipPos.y + tipH),
        IM_COL32(45, 60, 90, 220), IM_COL32(40, 55, 85, 220),
        IM_COL32(40, 55, 85, 220), IM_COL32(45, 60, 90, 220));
    dl->AddRect(tipPos, ImVec2(tipPos.x + tipW, tipPos.y + tipH),
        IM_COL32(80, 130, 200, 120), 10.0f);

    // 灯泡图标 - 更精致
    float bulbX = tipPos.x + 24;
    float bulbY = tipPos.y + 24;
    dl->AddCircleFilled(ImVec2(bulbX, bulbY), 12, IM_COL32(255, 220, 100, 60));
    dl->AddCircleFilled(ImVec2(bulbX, bulbY), 9, IM_COL32(255, 220, 100, 255));
    dl->AddRectFilled(ImVec2(bulbX - 4, bulbY + 10), ImVec2(bulbX + 4, bulbY + 16),
        IM_COL32(255, 220, 100, 255), 2.0f);

    dl->AddText(ImGui::GetFont(), 14.0f, ImVec2(tipPos.x + 46, tipPos.y + 14), IM_COL32(255, 220, 100, 255), u8"\u5C0F\u8D34\u58EB");

    ImGui::SetCursorScreenPos(ImVec2(tipPos.x + 46, tipPos.y + 38));
    ImGui::PushTextWrapPos(tipPos.x + tipW - 16);
    ImGui::TextColored(ImVec4(0.68f, 0.73f, 0.85f, 1.0f),
                       u8"\u2022 \u4E0D\u786E\u5B9A\u7C7B\u578B\u65F6\u5148\u8BD5 4\u5B57\u8282\u6574\u6570\n"
                       u8"\u2022 \u5C0F\u6570\uFF08\u5750\u6807/\u901F\u5EA6\uFF09\u7528\u5355\u7CBE\u5EA6\u6216\u53CC\u7CBE\u5EA6\n"
                       u8"\u2022 \u672A\u77E5\u521D\u59CB\u503C\u9002\u5408\u4E0D\u77E5\u9053\u5177\u4F53\u6570\u503C\u7684\u60C5\u51B5");
    ImGui::PopTextWrapPos();

    ImGui::EndChild();
}

void MemoryModifierPanel::RenderOnlineWarning() {
    if (!showOnlineWarning_) return;

    ImVec2 center = ImVec2(windowPos_.x + windowSize_.x / 2, windowPos_.y + windowSize_.y / 2);
    ImVec2 popupSize = ImVec2(400, 200);
    ImVec2 popupPos = ImVec2(center.x - popupSize.x / 2, center.y - popupSize.y / 2);

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // 半透明遮罩
    dl->AddRectFilled(windowPos_, ImVec2(windowPos_.x + windowSize_.x, windowPos_.y + windowSize_.y),
                      IM_COL32(0, 0, 0, 150));

    // 弹窗背景
    dl->AddRectFilled(popupPos, ImVec2(popupPos.x + popupSize.x, popupPos.y + popupSize.y),
                      IM_COL32(45, 50, 65, 250), 12.0f);
    dl->AddRect(popupPos, ImVec2(popupPos.x + popupSize.x, popupPos.y + popupSize.y),
                IM_COL32(200, 100, 100, 200), 12.0f, 0, 2.0f);

    // 警告图标
    ImVec2 iconPos = ImVec2(popupPos.x + popupSize.x / 2, popupPos.y + 50);
    dl->AddTriangleFilled(
        ImVec2(iconPos.x, iconPos.y - 25),
        ImVec2(iconPos.x - 25, iconPos.y + 20),
        ImVec2(iconPos.x + 25, iconPos.y + 20),
        IM_COL32(255, 180, 50, 255)
    );
    dl->AddText(ImVec2(iconPos.x - 4, iconPos.y - 8), IM_COL32(50, 50, 50, 255), "!");

    // 标题
    const char* title = u8"\u8B66\u544A";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    dl->AddText(ImVec2(popupPos.x + (popupSize.x - titleSize.x) / 2, popupPos.y + 85),
                IM_COL32(255, 200, 100, 255), title);

    // 消息
    ImVec2 msgSize = ImGui::CalcTextSize(warningMessage_.c_str());
    dl->AddText(ImVec2(popupPos.x + (popupSize.x - msgSize.x) / 2, popupPos.y + 115),
                IM_COL32(220, 220, 230, 255), warningMessage_.c_str());

    // 确定按钮
    ImVec2 btnPos = ImVec2(popupPos.x + (popupSize.x - 100) / 2, popupPos.y + 155);
    ImVec2 btnSize = ImVec2(100, 32);

    bool btnHovered = ImGui::IsMouseHoveringRect(btnPos, ImVec2(btnPos.x + btnSize.x, btnPos.y + btnSize.y));
    dl->AddRectFilled(btnPos, ImVec2(btnPos.x + btnSize.x, btnPos.y + btnSize.y),
                      btnHovered ? IM_COL32(100, 140, 200, 255) : IM_COL32(80, 120, 180, 255), 6.0f);

    const char* btnText = u8"\u786E\u5B9A";
    ImVec2 btnTextSize = ImGui::CalcTextSize(btnText);
    dl->AddText(ImVec2(btnPos.x + (btnSize.x - btnTextSize.x) / 2, btnPos.y + (btnSize.y - btnTextSize.y) / 2),
                IM_COL32(255, 255, 255, 255), btnText);

    if (btnHovered && ImGui::IsMouseClicked(0)) {
        showOnlineWarning_ = false;
    }
}

} // namespace sf
