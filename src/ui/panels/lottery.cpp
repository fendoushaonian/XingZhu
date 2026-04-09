#include "ui/panels/lottery.h"
#include "core/auth.h"
#include "ui/toast.h"
#include "ui/iconfonts.h"
#include <cmath>
#include <cstdlib>
#include <ctime>

namespace sf {

extern ImFont* g_mainFont;
extern ImFont* g_mainFontSmall;
extern ImFont* g_mainFontLarge;
extern ImFont* g_iconFont;

// 奖品配置
static Prize g_prizes[] = {
    {PrizeType::Thanks,    u8"谢谢参与",    IM_COL32(100, 110, 130, 255), 30},
    {PrizeType::Coins10,   u8"10 星铸币",   IM_COL32(102, 192, 244, 255), 25},
    {PrizeType::Coins50,   u8"50 星铸币",   IM_COL32(87, 203, 100, 255),  20},
    {PrizeType::Coins100,  u8"100 星铸币",  IM_COL32(255, 200, 50, 255),  12},
    {PrizeType::VipDay1,   u8"1天VIP",      IM_COL32(220, 130, 255, 255), 6},
    {PrizeType::Coins500,  u8"500 星铸币",  IM_COL32(255, 100, 100, 255), 4},
    {PrizeType::VipDay3,   u8"3天VIP",      IM_COL32(255, 150, 200, 255), 2},
    {PrizeType::VipDay7,   u8"7天VIP",      IM_COL32(255, 215, 0, 255),   1},
};
static constexpr int PRIZE_COUNT = sizeof(g_prizes) / sizeof(g_prizes[0]);

void LotteryPanel::Open() {
    isOpen_ = true;
    animProgress_ = 0.0f;
    showResult_ = false;
}

void LotteryPanel::Close() {
    isOpen_ = false;
    isSpinning_ = false;
    showResult_ = false;
}

int LotteryPanel::SelectPrize() {
    // 计算总权重
    int totalWeight = 0;
    for (int i = 0; i < PRIZE_COUNT; i++) {
        totalWeight += g_prizes[i].weight;
    }

    // 随机选择
    int roll = rand() % totalWeight;
    int cumulative = 0;
    for (int i = 0; i < PRIZE_COUNT; i++) {
        cumulative += g_prizes[i].weight;
        if (roll < cumulative) {
            return i;
        }
    }
    return 0;  // 默认返回第一个
}

void LotteryPanel::DoLottery() {
    if (!IsLoggedIn()) {
        ShowToast(u8"请先登录", ToastType::Warning);
        return;
    }

    if (isSpinning_) {
        return;
    }

    // 检查余额
    const auto& user = GetCurrentUser();
    if (user.coins < LOTTERY_COST) {
        ShowToast(u8"星铸币不足，需要50星铸币", ToastType::Warning);
        return;
    }

    // 扣除星铸币
    std::string error;
    if (!DeductCoins(LOTTERY_COST, error)) {
        ShowToast(error, ToastType::Error);
        return;
    }

    // 开始转动
    isSpinning_ = true;
    showResult_ = false;
    spinSpeed_ = 15.0f + (rand() % 10) * 0.5f;  // 初始速度

    // 预先决定结果
    resultIndex_ = SelectPrize();

    // 计算目标角度（至少转3圈 + 停在目标位置）
    float sectorAngle = 360.0f / PRIZE_COUNT;
    float prizeAngle = resultIndex_ * sectorAngle + sectorAngle * 0.5f;
    targetAngle_ = spinAngle_ + 360.0f * 5 + (360.0f - prizeAngle);  // 5圈 + 目标位置
}

void LotteryPanel::UpdateSpin(float dt) {
    if (!isSpinning_) return;

    // 计算剩余角度
    float remaining = targetAngle_ - spinAngle_;

    if (remaining > 0) {
        // 缓动减速
        float progress = 1.0f - (remaining / (360.0f * 5));
        float easedSpeed = spinSpeed_ * (1.0f - progress * progress);
        easedSpeed = std::max(easedSpeed, 0.5f);

        spinAngle_ += easedSpeed * dt * 60.0f;

        if (spinAngle_ >= targetAngle_) {
            spinAngle_ = targetAngle_;
            isSpinning_ = false;
            showResult_ = true;
            ApplyPrize(resultIndex_);
        }
    }

    // 保持角度在合理范围
    while (spinAngle_ >= 360.0f) {
        spinAngle_ -= 360.0f;
        targetAngle_ -= 360.0f;
    }
}

void LotteryPanel::ApplyPrize(int prizeIndex) {
    if (prizeIndex < 0 || prizeIndex >= PRIZE_COUNT) return;

    const Prize& prize = g_prizes[prizeIndex];
    std::string error;
    int coinsToAdd = 0;
    int vipDays = 0;

    switch (prize.type) {
        case PrizeType::Coins10:  coinsToAdd = 10; break;
        case PrizeType::Coins50:  coinsToAdd = 50; break;
        case PrizeType::Coins100: coinsToAdd = 100; break;
        case PrizeType::Coins500: coinsToAdd = 500; break;
        case PrizeType::VipDay1:  vipDays = 1; break;
        case PrizeType::VipDay3:  vipDays = 3; break;
        case PrizeType::VipDay7:  vipDays = 7; break;
        case PrizeType::Thanks:   break;  // 谢谢参与，无奖励
    }

    if (coinsToAdd > 0) {
        AddCoins(coinsToAdd, error);
    }
    if (vipDays > 0) {
        AddVipDays(vipDays, error);
    }
}

void LotteryPanel::Render(float winW, float winH) {
    if (!isOpen_) {
        animProgress_ = 0.0f;
        return;
    }

    float dt = ImGui::GetIO().DeltaTime;
    animProgress_ += (1.0f - animProgress_) * dt * 10.0f;
    if (animProgress_ > 0.99f) animProgress_ = 1.0f;

    UpdateSpin(dt);

    // 弹窗尺寸
    float popupW = 500.0f;
    float popupH = 580.0f;
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
    ImGui::Begin("##lottery_overlay", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings);

    // 点击遮罩关闭（转动时不允许关闭）
    if (!isSpinning_ && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
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

    ImGui::Begin(u8"幸运抽奖##lottery_popup", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
    ImFont* smallFont = g_mainFontSmall ? g_mainFontSmall : font;
    ImFont* largeFont = g_mainFontLarge ? g_mainFontLarge : font;

    // 获取用户数据
    int coins = 0;
    if (IsLoggedIn()) {
        const auto& user = GetCurrentUser();
        coins = user.coins;
    }

    float contentX = winPos.x + 24;
    float contentY = winPos.y + 50;

    // ═══════════════════════════════════════════════════════════════
    //  标题和余额
    // ═══════════════════════════════════════════════════════════════
    dl->AddText(largeFont, largeFont->FontSize, ImVec2(contentX, contentY),
                IM_COL32(255, 255, 255, (int)(255 * alpha)), u8"幸运大转盘");

    char balanceText[64];
    snprintf(balanceText, sizeof(balanceText), u8"余额: %d 星铸币", coins);
    ImVec2 balanceSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, balanceText);
    dl->AddText(font, font->FontSize, ImVec2(contentX + popupW - 48 - balanceSize.x, contentY + 4),
                IM_COL32(255, 200, 50, (int)(220 * alpha)), balanceText);

    contentY += 45;

    // ═══════════════════════════════════════════════════════════════
    //  转盘
    // ═══════════════════════════════════════════════════════════════
    float wheelRadius = 160.0f * scale;
    float wheelCx = winPos.x + popupW * 0.5f;
    float wheelCy = contentY + wheelRadius + 10;

    // 转盘外圈装饰
    dl->AddCircleFilled(ImVec2(wheelCx, wheelCy), wheelRadius + 15,
                        IM_COL32(50, 60, 80, (int)(255 * alpha)), 64);
    dl->AddCircle(ImVec2(wheelCx, wheelCy), wheelRadius + 15,
                  IM_COL32(255, 200, 50, (int)(200 * alpha)), 64, 3.0f);

    // 绘制转盘扇区
    float sectorAngle = 360.0f / PRIZE_COUNT;
    for (int i = 0; i < PRIZE_COUNT; i++) {
        float startAngle = (i * sectorAngle + spinAngle_ - 90) * 3.14159f / 180.0f;
        float endAngle = ((i + 1) * sectorAngle + spinAngle_ - 90) * 3.14159f / 180.0f;

        // 扇区颜色（交替深浅）
        ImU32 sectorColor = (i % 2 == 0)
            ? IM_COL32(35, 45, 65, (int)(255 * alpha))
            : IM_COL32(45, 55, 75, (int)(255 * alpha));

        // 绘制扇区
        ImVec2 center(wheelCx, wheelCy);
        int segments = 32;
        dl->PathLineTo(center);
        for (int s = 0; s <= segments; s++) {
            float angle = startAngle + (endAngle - startAngle) * s / segments;
            dl->PathLineTo(ImVec2(wheelCx + cosf(angle) * wheelRadius,
                                   wheelCy + sinf(angle) * wheelRadius));
        }
        dl->PathFillConvex(sectorColor);

        // 扇区边框
        dl->AddLine(ImVec2(wheelCx, wheelCy),
                    ImVec2(wheelCx + cosf(startAngle) * wheelRadius,
                           wheelCy + sinf(startAngle) * wheelRadius),
                    IM_COL32(60, 70, 90, (int)(200 * alpha)), 1.0f);

        // 奖品文字
        float midAngle = (startAngle + endAngle) * 0.5f;
        float textR = wheelRadius * 0.65f;
        float textX = wheelCx + cosf(midAngle) * textR;
        float textY = wheelCy + sinf(midAngle) * textR;

        ImVec2 textSize = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, g_prizes[i].name);
        dl->AddText(smallFont, smallFont->FontSize,
                    ImVec2(textX - textSize.x * 0.5f, textY - textSize.y * 0.5f),
                    g_prizes[i].color, g_prizes[i].name);
    }

    // 中心圆
    dl->AddCircleFilled(ImVec2(wheelCx, wheelCy), 35 * scale,
                        IM_COL32(255, 200, 50, (int)(255 * alpha)), 32);
    dl->AddCircle(ImVec2(wheelCx, wheelCy), 35 * scale,
                  IM_COL32(255, 255, 255, (int)(200 * alpha)), 32, 2.0f);

    // 指针（顶部）
    float pointerY = wheelCy - wheelRadius - 5;
    ImVec2 p1(wheelCx, pointerY + 25);
    ImVec2 p2(wheelCx - 12, pointerY);
    ImVec2 p3(wheelCx + 12, pointerY);
    dl->AddTriangleFilled(p1, p2, p3, IM_COL32(255, 100, 100, (int)(255 * alpha)));
    dl->AddTriangle(p1, p2, p3, IM_COL32(255, 255, 255, (int)(200 * alpha)), 2.0f);

    contentY = wheelCy + wheelRadius + 30;

    // ═══════════════════════════════════════════════════════════════
    //  抽奖按钮
    // ═══════════════════════════════════════════════════════════════
    float btnW = 200.0f;
    float btnH = 48.0f;
    float btnX = winPos.x + (popupW - btnW) * 0.5f;
    float btnY = contentY;

    ImGui::SetCursorScreenPos(ImVec2(btnX, btnY));
    ImGui::PushID("lottery_btn");

    if (isSpinning_) {
        // 转动中
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(60, 70, 85, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(60, 70, 85, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(60, 70, 85, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::Button(u8"抽奖中...", ImVec2(btnW, btnH));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    } else {
        // 可抽奖
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(255, 180, 50, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 200, 80, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(235, 160, 30, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

        char btnText[64];
        snprintf(btnText, sizeof(btnText), u8"抽奖 (消耗%d星铸币)", LOTTERY_COST);
        if (ImGui::Button(btnText, ImVec2(btnW, btnH))) {
            DoLottery();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
    }

    ImGui::PopID();

    // ═══════════════════════════════════════════════════════════════
    //  中奖结果
    // ═══════════════════════════════════════════════════════════════
    if (showResult_ && resultIndex_ >= 0 && resultIndex_ < PRIZE_COUNT) {
        contentY = btnY + btnH + 20;

        const Prize& prize = g_prizes[resultIndex_];
        char resultText[128];
        if (prize.type == PrizeType::Thanks) {
            snprintf(resultText, sizeof(resultText), u8"很遗憾，谢谢参与~");
        } else {
            snprintf(resultText, sizeof(resultText), u8"恭喜获得: %s", prize.name);
        }

        ImVec2 resultSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, resultText);
        float resultX = winPos.x + (popupW - resultSize.x) * 0.5f;

        // 结果背景
        float bgPad = 12.0f;
        dl->AddRectFilled(ImVec2(resultX - bgPad, contentY - 4),
                          ImVec2(resultX + resultSize.x + bgPad, contentY + resultSize.y + 8),
                          IM_COL32(35, 45, 60, (int)(220 * alpha)), 6.0f);

        dl->AddText(font, font->FontSize, ImVec2(resultX, contentY),
                    prize.color, resultText);
    }

    // ═══════════════════════════════════════════════════════════════
    //  奖品说明
    // ═══════════════════════════════════════════════════════════════
    contentY = btnY + btnH + 60;
    dl->AddText(smallFont, smallFont->FontSize, ImVec2(contentX, contentY),
                IM_COL32(120, 140, 160, (int)(180 * alpha)),
                u8"奖品: 星铸币 / VIP天数 / 谢谢参与");

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace sf
