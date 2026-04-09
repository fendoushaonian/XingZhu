#include "ui/panels/checkin.h"
#include "core/auth.h"
#include "ui/toast.h"
#include "ui/iconfonts.h"
#include <ctime>
#include <cmath>

namespace sf {

extern ImFont* g_mainFont;
extern ImFont* g_mainFontSmall;
extern ImFont* g_mainFontLarge;
extern ImFont* g_iconFont;

// 获取某月的天数
static int GetDaysInMonth(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
        return 29;
    return days[month - 1];
}

// 获取某月第一天是星期几 (0=周日)
static int GetFirstDayOfWeek(int year, int month) {
    std::tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = 1;
    std::mktime(&tm);
    return tm.tm_wday;
}

void CheckInPanel::Open() {
    isOpen_ = true;
    RefreshCalendarData();
}

void CheckInPanel::Close() {
    isOpen_ = false;
}

void CheckInPanel::RefreshCalendarData() {
    // 获取当前日期
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);
    currentYear_ = local->tm_year + 1900;
    currentMonth_ = local->tm_mon + 1;
    daysInMonth_ = GetDaysInMonth(currentYear_, currentMonth_);
    firstDayOfWeek_ = GetFirstDayOfWeek(currentYear_, currentMonth_);

    // 初始化签到数据
    checkedDays_.clear();
    checkedDays_.resize(daysInMonth_, false);

    // 从用户数据获取签到记录
    if (IsLoggedIn()) {
        const auto& user = GetCurrentUser();
        // 解析 lastCheckIn 日期
        if (!user.lastCheckIn.empty()) {
            int y, m, d;
            if (sscanf(user.lastCheckIn.c_str(), "%d-%d-%d", &y, &m, &d) == 3) {
                if (y == currentYear_ && m == currentMonth_ && d >= 1 && d <= daysInMonth_) {
                    // 标记连续签到的日期
                    for (int i = 0; i < user.consecutiveDays && (d - i) >= 1; i++) {
                        checkedDays_[d - i - 1] = true;
                    }
                }
            }
        }
    }
}

void CheckInPanel::DoCheckIn() {
    if (!IsLoggedIn()) {
        ShowToast(u8"请先登录", ToastType::Warning);
        return;
    }

    int coinsEarned = 0;
    std::string error;
    if (AuthCheckIn(coinsEarned, error)) {
        char msg[128];
        snprintf(msg, sizeof(msg), u8"签到成功！获得 %d 星铸币", coinsEarned);
        ShowToast(msg, ToastType::Success);
        RefreshCalendarData();
    } else {
        ShowToast(error, ToastType::Error);
    }
}

void CheckInPanel::Render(float winW, float winH) {
    if (!isOpen_) {
        animProgress_ = 0.0f;
        return;
    }

    float dt = ImGui::GetIO().DeltaTime;
    animProgress_ += (1.0f - animProgress_) * dt * 10.0f;
    if (animProgress_ > 0.99f) animProgress_ = 1.0f;

    // 弹窗尺寸
    float popupW = 480.0f;
    float popupH = 520.0f;
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
    ImGui::Begin("##checkin_overlay", nullptr,
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
    ImGui::SetNextWindowFocus();  // 确保弹窗获得焦点
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(25, 30, 40, (int)(250 * alpha)));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(60, 80, 120, (int)(200 * alpha)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

    ImGui::Begin(u8"每日签到##checkin_popup", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
    ImFont* smallFont = g_mainFontSmall ? g_mainFontSmall : font;
    ImFont* largeFont = g_mainFontLarge ? g_mainFontLarge : font;

    // 获取用户数据
    int consecutiveDays = 0;
    int coins = 0;
    bool checkedToday = HasCheckedInToday();
    if (IsLoggedIn()) {
        const auto& user = GetCurrentUser();
        consecutiveDays = user.consecutiveDays;
        coins = user.coins;
    }

    // ═══════════════════════════════════════════════════════════════
    //  标题区域
    // ═══════════════════════════════════════════════════════════════
    float contentX = winPos.x + 24;
    float contentY = winPos.y + 50;

    // 月份标题
    char monthTitle[64];
    snprintf(monthTitle, sizeof(monthTitle), u8"%d年%d月", currentYear_, currentMonth_);
    dl->AddText(largeFont, largeFont->FontSize, ImVec2(contentX, contentY),
                IM_COL32(255, 255, 255, (int)(255 * alpha)), monthTitle);

    // 连续签到天数
    char streakText[64];
    snprintf(streakText, sizeof(streakText), u8"连续签到 %d 天", consecutiveDays);
    ImVec2 streakSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, streakText);
    dl->AddText(font, font->FontSize, ImVec2(contentX + popupW - 48 - streakSize.x, contentY + 4),
                IM_COL32(255, 200, 50, (int)(220 * alpha)), streakText);

    contentY += 40;

    // ═══════════════════════════════════════════════════════════════
    //  奖励提示
    // ═══════════════════════════════════════════════════════════════
    float rewardBoxY = contentY;
    float rewardBoxH = 60.0f;
    dl->AddRectFilled(ImVec2(contentX, rewardBoxY),
                      ImVec2(contentX + popupW - 48, rewardBoxY + rewardBoxH),
                      IM_COL32(35, 45, 60, (int)(200 * alpha)), 8.0f);

    // 奖励说明
    float rewardTextY = rewardBoxY + 10;
    dl->AddText(smallFont, smallFont->FontSize, ImVec2(contentX + 12, rewardTextY),
                IM_COL32(180, 195, 210, (int)(200 * alpha)), u8"每日签到: +10 星铸币");
    rewardTextY += 18;
    dl->AddText(smallFont, smallFont->FontSize, ImVec2(contentX + 12, rewardTextY),
                IM_COL32(255, 200, 50, (int)(200 * alpha)), u8"连续7天: +100 星铸币  |  连续15天: +200 星铸币");

    contentY += rewardBoxH + 16;

    // ═══════════════════════════════════════════════════════════════
    //  日历网格
    // ═══════════════════════════════════════════════════════════════
    float calendarX = contentX;
    float calendarY = contentY;
    float cellW = (popupW - 48) / 7.0f;
    float cellH = 44.0f;

    // 星期标题
    const char* weekDays[] = {u8"日", u8"一", u8"二", u8"三", u8"四", u8"五", u8"六"};
    for (int i = 0; i < 7; i++) {
        ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, weekDays[i]);
        float tx = calendarX + i * cellW + (cellW - textSize.x) * 0.5f;
        dl->AddText(font, font->FontSize, ImVec2(tx, calendarY),
                    IM_COL32(120, 140, 160, (int)(200 * alpha)), weekDays[i]);
    }
    calendarY += 28;

    // 获取今天的日期
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);
    int today = local->tm_mday;

    // 日期格子
    int row = 0;
    int col = firstDayOfWeek_;
    for (int day = 1; day <= daysInMonth_; day++) {
        float cx = calendarX + col * cellW;
        float cy = calendarY + row * cellH;

        bool isToday = (day == today);
        bool isChecked = (day < (int)checkedDays_.size() + 1) && checkedDays_[day - 1];
        bool isPast = (day < today);

        // 格子背景
        ImU32 bgColor = IM_COL32(35, 45, 60, (int)(100 * alpha));
        if (isToday) {
            bgColor = IM_COL32(66, 133, 244, (int)(150 * alpha));
        } else if (isChecked) {
            bgColor = IM_COL32(87, 203, 100, (int)(120 * alpha));
        }
        dl->AddRectFilled(ImVec2(cx + 2, cy + 2), ImVec2(cx + cellW - 2, cy + cellH - 4),
                          bgColor, 6.0f);

        // 日期数字
        char dayStr[8];
        snprintf(dayStr, sizeof(dayStr), "%d", day);
        ImVec2 daySize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, dayStr);
        float tx = cx + (cellW - daySize.x) * 0.5f;
        float ty = cy + (cellH - daySize.y) * 0.5f - 4;

        ImU32 textColor = IM_COL32(200, 215, 230, (int)(220 * alpha));
        if (isChecked) {
            textColor = IM_COL32(255, 255, 255, (int)(255 * alpha));
        } else if (isPast && !isChecked) {
            textColor = IM_COL32(100, 115, 130, (int)(150 * alpha));
        }
        dl->AddText(font, font->FontSize, ImVec2(tx, ty), textColor, dayStr);

        // 已签到标记
        if (isChecked) {
            if (g_iconFont) {
                DrawIcon(dl, icon::CHECK, ImVec2(cx + cellW - 18, cy + 4),
                         IM_COL32(255, 255, 255, (int)(200 * alpha)));
            }
        }

        col++;
        if (col >= 7) {
            col = 0;
            row++;
        }
    }

    contentY = calendarY + (row + 1) * cellH + 16;

    // ═══════════════════════════════════════════════════════════════
    //  签到按钮
    // ═══════════════════════════════════════════════════════════════
    float btnW = 200.0f;
    float btnH = 48.0f;
    float btnX = contentX + (popupW - 48 - btnW) * 0.5f;
    float btnY = contentY;

    ImGui::SetCursorScreenPos(ImVec2(btnX, btnY));
    ImGui::PushID("checkin_btn");

    if (checkedToday) {
        // 已签到状态
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(60, 70, 85, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(60, 70, 85, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(60, 70, 85, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

        ImGui::Button(u8"今日已签到", ImVec2(btnW, btnH));

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    } else {
        // 可签到状态
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(66, 133, 244, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(86, 153, 255, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(46, 113, 224, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

        if (ImGui::Button(u8"立即签到", ImVec2(btnW, btnH))) {
            DoCheckIn();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }

    ImGui::PopID();

    // ═══════════════════════════════════════════════════════════════
    //  当前余额
    // ═══════════════════════════════════════════════════════════════
    contentY = btnY + btnH + 16;
    char balanceText[64];
    snprintf(balanceText, sizeof(balanceText), u8"当前余额: %d 星铸币", coins);
    ImVec2 balanceSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, balanceText);
    float balanceX = contentX + (popupW - 48 - balanceSize.x) * 0.5f;
    dl->AddText(font, font->FontSize, ImVec2(balanceX, contentY),
                IM_COL32(255, 200, 50, (int)(200 * alpha)), balanceText);

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace sf
