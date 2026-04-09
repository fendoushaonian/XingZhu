#include "ui/panels/task.h"
#include "core/auth.h"
#include "ui/toast.h"
#include "ui/iconfonts.h"
#include <cmath>

namespace sf {

extern ImFont* g_mainFont;
extern ImFont* g_mainFontSmall;
extern ImFont* g_mainFontLarge;
extern ImFont* g_iconFont;

void TaskPanel::Open() {
    isOpen_ = true;
    animProgress_ = 0.0f;
    RefreshTasks();
}

void TaskPanel::Close() {
    isOpen_ = false;
}

void TaskPanel::RefreshTasks() {
    // 每日任务
    dailyTasks_.clear();

    bool loggedIn = IsLoggedIn();
    bool checkedIn = loggedIn && HasCheckedInToday();

    dailyTasks_.push_back({
        1, TaskType::Daily,
        u8"每日签到",
        u8"完成每日签到",
        10,
        checkedIn ? 1 : 0, 1,
        checkedIn ? TaskStatus::Completed : TaskStatus::InProgress
    });

    dailyTasks_.push_back({
        2, TaskType::Daily,
        u8"启动游戏",
        u8"启动任意一款游戏",
        20,
        0, 1,
        TaskStatus::InProgress
    });

    dailyTasks_.push_back({
        3, TaskType::Daily,
        u8"游戏时长",
        u8"累计游戏30分钟",
        30,
        0, 30,
        TaskStatus::InProgress
    });

    // 每周任务
    weeklyTasks_.clear();

    int consecutiveDays = loggedIn ? GetCurrentUser().consecutiveDays : 0;

    weeklyTasks_.push_back({
        101, TaskType::Weekly,
        u8"连续签到3天",
        u8"连续签到3天",
        50,
        std::min(consecutiveDays, 3), 3,
        consecutiveDays >= 3 ? TaskStatus::Claimable : TaskStatus::InProgress
    });

    weeklyTasks_.push_back({
        102, TaskType::Weekly,
        u8"连续签到7天",
        u8"连续签到7天",
        150,
        std::min(consecutiveDays, 7), 7,
        consecutiveDays >= 7 ? TaskStatus::Claimable : TaskStatus::InProgress
    });

    weeklyTasks_.push_back({
        103, TaskType::Weekly,
        u8"游戏达人",
        u8"本周游戏时长达到5小时",
        100,
        0, 300,
        TaskStatus::InProgress
    });

    // 成就任务
    achievementTasks_.clear();

    achievementTasks_.push_back({
        201, TaskType::Achievement,
        u8"初来乍到",
        u8"完成首次签到",
        50,
        checkedIn ? 1 : 0, 1,
        checkedIn ? TaskStatus::Claimable : TaskStatus::InProgress
    });

    achievementTasks_.push_back({
        202, TaskType::Achievement,
        u8"签到达人",
        u8"累计签到30天",
        200,
        loggedIn ? GetCurrentUser().monthlyCheckIns : 0, 30,
        TaskStatus::InProgress
    });

    achievementTasks_.push_back({
        203, TaskType::Achievement,
        u8"游戏收藏家",
        u8"安装10款游戏",
        300,
        0, 10,
        TaskStatus::InProgress
    });

    achievementTasks_.push_back({
        204, TaskType::Achievement,
        u8"VIP会员",
        u8"成为VIP会员",
        100,
        IsVipActive() ? 1 : 0, 1,
        IsVipActive() ? TaskStatus::Claimable : TaskStatus::InProgress
    });
}

void TaskPanel::ClaimReward(int taskId) {
    if (!IsLoggedIn()) {
        ShowToast(u8"请先登录", ToastType::Warning);
        return;
    }

    // 查找任务
    TaskItem* task = nullptr;
    for (auto& t : dailyTasks_) {
        if (t.id == taskId) { task = &t; break; }
    }
    if (!task) {
        for (auto& t : weeklyTasks_) {
            if (t.id == taskId) { task = &t; break; }
        }
    }
    if (!task) {
        for (auto& t : achievementTasks_) {
            if (t.id == taskId) { task = &t; break; }
        }
    }

    if (!task || task->status != TaskStatus::Claimable) {
        ShowToast(u8"无法领取奖励", ToastType::Warning);
        return;
    }

    // 发放奖励
    std::string error;
    if (AddCoins(task->rewardCoins, error)) {
        task->status = TaskStatus::Completed;
        char msg[128];
        snprintf(msg, sizeof(msg), u8"领取成功！获得 %d 星铸币", task->rewardCoins);
        ShowToast(msg, ToastType::Success);
    } else {
        ShowToast(error, ToastType::Error);
    }
}

void TaskPanel::RenderTaskList(const std::vector<TaskItem>& tasks, float startY, float width) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
    ImFont* smallFont = g_mainFontSmall ? g_mainFontSmall : font;

    float itemH = 80.0f;
    float padding = 12.0f;
    float y = startY;

    // 按钮宽度
    float btnW = 70.0f;
    float btnH = 28.0f;

    for (size_t i = 0; i < tasks.size(); i++) {
        const TaskItem& task = tasks[i];
        float x = winPos.x + 12;  // 减少左边距
        float itemW = width - 24;  // 调整宽度

        // 任务卡片背景
        ImU32 bgColor = IM_COL32(35, 45, 60, 220);
        if (task.status == TaskStatus::Claimable) {
            bgColor = IM_COL32(45, 60, 45, 220);
        } else if (task.status == TaskStatus::Completed) {
            bgColor = IM_COL32(40, 50, 65, 180);
        }
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + itemW, y + itemH), bgColor, 8.0f);

        // 任务名称
        ImU32 nameColor = task.status == TaskStatus::Completed
            ? IM_COL32(120, 140, 160, 200)
            : IM_COL32(230, 240, 250, 255);
        dl->AddText(font, font->FontSize, ImVec2(x + padding, y + padding), nameColor, task.name);

        // 奖励显示（放在名称右边）
        char rewardText[32];
        snprintf(rewardText, sizeof(rewardText), "+%d", task.rewardCoins);
        ImVec2 rewardSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, rewardText);
        float rewardX = x + itemW - padding - btnW - 16 - rewardSize.x;
        dl->AddText(font, font->FontSize, ImVec2(rewardX, y + padding),
                    IM_COL32(255, 200, 50, 220), rewardText);

        // 任务描述
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(x + padding, y + padding + 24),
                    IM_COL32(140, 160, 180, 200), task.description);

        // 进度条（缩短宽度，给右边按钮留空间）
        float progressBarW = itemW - padding * 2 - btnW - 24;
        float progressBarH = 6.0f;
        float progressBarX = x + padding;
        float progressBarY = y + itemH - padding - progressBarH - 4;

        // 进度条背景
        dl->AddRectFilled(ImVec2(progressBarX, progressBarY),
                          ImVec2(progressBarX + progressBarW, progressBarY + progressBarH),
                          IM_COL32(25, 30, 40, 200), 3.0f);

        // 进度条填充
        float progress = task.targetProgress > 0
            ? (float)task.currentProgress / task.targetProgress
            : 0.0f;
        progress = std::min(progress, 1.0f);

        ImU32 progressColor = IM_COL32(66, 133, 244, 255);
        if (task.status == TaskStatus::Claimable) {
            progressColor = IM_COL32(87, 203, 100, 255);
        } else if (task.status == TaskStatus::Completed) {
            progressColor = IM_COL32(100, 120, 140, 200);
        }

        if (progress > 0) {
            dl->AddRectFilled(ImVec2(progressBarX, progressBarY),
                              ImVec2(progressBarX + progressBarW * progress, progressBarY + progressBarH),
                              progressColor, 3.0f);
        }

        // 进度文字（放在进度条右端）
        char progressText[32];
        snprintf(progressText, sizeof(progressText), "%d/%d", task.currentProgress, task.targetProgress);
        ImVec2 progressTextSize = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, progressText);
        dl->AddText(smallFont, smallFont->FontSize,
                    ImVec2(progressBarX + progressBarW - progressTextSize.x, progressBarY - 14),
                    IM_COL32(160, 180, 200, 200), progressText);

        // 领取按钮（固定在右侧）
        float btnX = x + itemW - padding - btnW;
        float btnY = y + (itemH - btnH) * 0.5f;

        ImGui::SetCursorScreenPos(ImVec2(btnX, btnY));
        ImGui::PushID((int)(1000 + i));

        if (task.status == TaskStatus::Claimable) {
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(87, 203, 100, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(107, 223, 120, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(67, 183, 80, 255));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

            if (ImGui::Button(u8"领取", ImVec2(btnW, btnH))) {
                const_cast<TaskPanel*>(this)->ClaimReward(task.id);
            }

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        } else if (task.status == TaskStatus::Completed) {
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(60, 70, 85, 200));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(60, 70, 85, 200));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(60, 70, 85, 200));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

            ImGui::Button(u8"已完成", ImVec2(btnW, btnH));

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(50, 60, 75, 200));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(50, 60, 75, 200));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(50, 60, 75, 200));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

            ImGui::Button(u8"进行中", ImVec2(btnW, btnH));

            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
        }

        ImGui::PopID();

        y += itemH + 8;
    }
}

void TaskPanel::Render(float winW, float winH) {
    if (!isOpen_) {
        animProgress_ = 0.0f;
        return;
    }

    float dt = ImGui::GetIO().DeltaTime;
    animProgress_ += (1.0f - animProgress_) * dt * 10.0f;
    if (animProgress_ > 0.99f) animProgress_ = 1.0f;

    // 弹窗尺寸
    float popupW = 520.0f;
    float popupH = 560.0f;
    float popupX = (winW - popupW) * 0.5f;
    float popupY = (winH - popupH) * 0.5f;

    // 应用动画
    float scale = 0.9f + 0.1f * animProgress_;
    float alpha = animProgress_;
    popupW *= scale;
    popupH *= scale;
    popupX = (winW - popupW) * 0.5f;
    popupY = (winH - popupH) * 0.5f;

    // 半透明背景遮罩
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(winW, winH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, (int)(150 * alpha)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##task_overlay", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings);

    // 点击遮罩关闭
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
        ImVec2 mousePos = ImGui::GetMousePos();
        if (mousePos.x < popupX || mousePos.x > popupX + popupW ||
            mousePos.y < popupY || mousePos.y > popupY + popupH) {
            Close();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // 弹窗主体
    ImGui::SetNextWindowPos(ImVec2(popupX, popupY));
    ImGui::SetNextWindowSize(ImVec2(popupW, popupH));
    ImGui::SetNextWindowFocus();
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(25, 30, 40, (int)(250 * alpha)));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(60, 80, 120, (int)(200 * alpha)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

    ImGui::Begin(u8"任务中心##task_popup", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
    ImFont* largeFont = g_mainFontLarge ? g_mainFontLarge : font;

    float contentX = winPos.x + 24;
    float contentY = winPos.y + 50;

    // ═══════════════════════════════════════════════════════════════
    //  标题
    // ═══════════════════════════════════════════════════════════════
    dl->AddText(largeFont, largeFont->FontSize, ImVec2(contentX, contentY),
                IM_COL32(255, 255, 255, (int)(255 * alpha)), u8"任务中心");

    // 当前余额
    if (IsLoggedIn()) {
        char balanceText[64];
        snprintf(balanceText, sizeof(balanceText), u8"余额: %d 星铸币", GetCurrentUser().coins);
        ImVec2 balanceSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, balanceText);
        dl->AddText(font, font->FontSize, ImVec2(contentX + popupW - 48 - balanceSize.x, contentY + 4),
                    IM_COL32(255, 200, 50, (int)(220 * alpha)), balanceText);
    }

    contentY += 45;

    // ═══════════════════════════════════════════════════════════════
    //  标签页
    // ═══════════════════════════════════════════════════════════════
    const char* tabs[] = {u8"每日任务", u8"每周任务", u8"成就"};
    float tabW = (popupW - 48) / 3.0f;
    float tabH = 36.0f;

    for (int i = 0; i < 3; i++) {
        float tabX = contentX + i * tabW;

        ImGui::SetCursorScreenPos(ImVec2(tabX, contentY));
        ImGui::PushID(i + 100);

        bool isSelected = (selectedTab_ == i);
        ImU32 tabBg = isSelected ? IM_COL32(66, 133, 244, 255) : IM_COL32(40, 50, 65, 200);
        ImU32 tabHover = isSelected ? IM_COL32(86, 153, 255, 255) : IM_COL32(50, 60, 75, 220);

        ImGui::PushStyleColor(ImGuiCol_Button, tabBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, tabHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, tabBg);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

        if (ImGui::Button(tabs[i], ImVec2(tabW - 4, tabH))) {
            selectedTab_ = i;
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        ImGui::PopID();
    }

    contentY += tabH + 16;

    // ═══════════════════════════════════════════════════════════════
    //  任务列表（带滚动）
    // ═══��══════════════════════════════════════════════════════════��
    float listH = popupH - (contentY - winPos.y) - 30;

    ImGui::SetCursorScreenPos(ImVec2(contentX, contentY));
    ImGui::BeginChild("##task_list", ImVec2(popupW - 48, listH), false,
                      ImGuiWindowFlags_NoBackground);

    ImVec2 childPos = ImGui::GetWindowPos();
    float listY = childPos.y;

    switch (selectedTab_) {
        case 0:
            RenderTaskList(dailyTasks_, listY, popupW);
            break;
        case 1:
            RenderTaskList(weeklyTasks_, listY, popupW);
            break;
        case 2:
            RenderTaskList(achievementTasks_, listY, popupW);
            break;
    }

    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace sf
