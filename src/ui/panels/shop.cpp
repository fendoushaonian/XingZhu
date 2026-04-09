#include "ui/panels/shop.h"
#include "core/auth.h"
#include "ui/toast.h"
#include "ui/iconfonts.h"
#include <cmath>

namespace sf {

extern ImFont* g_mainFont;
extern ImFont* g_mainFontSmall;
extern ImFont* g_mainFontLarge;
extern ImFont* g_iconFont;

void ShopPanel::Open() {
    isOpen_ = true;
    animProgress_ = 0.0f;
    RefreshShopItems();
}

void ShopPanel::Close() {
    isOpen_ = false;
}

void ShopPanel::RefreshShopItems() {
    shopItems_.clear();

    // VIP会员
    shopItems_.push_back({
        1, ShopItemType::Vip,
        u8"7天会员",
        u8"享受7天VIP特权\n畅玩所有游戏",
        500, 7,
        IM_COL32(102, 192, 244, 255),
        IM_COL32(30, 60, 90, 255)
    });
    shopItems_.push_back({
        2, ShopItemType::Vip,
        u8"15天会员",
        u8"享受15天VIP特权\n畅玩所有游戏",
        900, 15,
        IM_COL32(87, 203, 100, 255),
        IM_COL32(30, 70, 50, 255)
    });
    shopItems_.push_back({
        3, ShopItemType::Vip,
        u8"30天会员",
        u8"享受30天VIP特权\n畅玩所有游戏，超值",
        1500, 30,
        IM_COL32(255, 200, 50, 255),
        IM_COL32(70, 60, 30, 255)
    });

    // 游戏盲盒
    shopItems_.push_back({
        101, ShopItemType::BlindBox,
        u8"游戏盲盒(30元)",
        u8"随机获得一款\n价值30元左右的游戏",
        300, 30,
        IM_COL32(255, 130, 180, 255),
        IM_COL32(70, 40, 60, 255)
    });
    shopItems_.push_back({
        102, ShopItemType::BlindBox,
        u8"游戏盲盒(100元)",
        u8"随机获得一款\n价值100元左右的游戏",
        800, 100,
        IM_COL32(220, 130, 255, 255),
        IM_COL32(60, 40, 70, 255)
    });

    // CDK
    shopItems_.push_back({
        201, ShopItemType::CDK,
        u8"随机正版CDK",
        u8"随机获得一个\n正版游戏激活码",
        1000, 0,
        IM_COL32(255, 100, 100, 255),
        IM_COL32(70, 35, 35, 255)
    });
}

void ShopPanel::PurchaseItem(int itemId) {
    if (!IsLoggedIn()) {
        ShowToast(u8"请先登录", ToastType::Warning);
        return;
    }

    // 查找商品
    ShopItem* item = nullptr;
    for (auto& i : shopItems_) {
        if (i.id == itemId) { item = &i; break; }
    }

    if (!item) {
        ShowToast(u8"商品不存在", ToastType::Error);
        return;
    }

    // 检查余额
    const auto& user = GetCurrentUser();
    if (user.coins < item->price) {
        ShowToast(u8"星铸币不足", ToastType::Warning);
        return;
    }

    // 扣除星铸币
    std::string error;
    if (!DeductCoins(item->price, error)) {
        ShowToast(error, ToastType::Error);
        return;
    }

    // 发放商品
    bool success = false;
    switch (item->type) {
        case ShopItemType::Vip:
            success = AddVipDays(item->value, error);
            if (success) {
                char msg[128];
                snprintf(msg, sizeof(msg), u8"购买成功！获得%d天VIP", item->value);
                ShowToast(msg, ToastType::Success);
            }
            break;
        case ShopItemType::BlindBox:
            // 盲盒暂时模拟
            success = true;
            ShowToast(u8"购买成功！请到邮箱查收游戏信息", ToastType::Success);
            break;
        case ShopItemType::CDK:
            // CDK暂时模拟
            success = true;
            ShowToast(u8"购买成功！CDK已发送到您的邮箱", ToastType::Success);
            break;
    }

    if (!success) {
        // 购买失败，退还星铸币
        AddCoins(item->price, error);
        ShowToast(u8"购买失败，已退还星铸币", ToastType::Error);
    }
}

void ShopPanel::Render(float winW, float winH) {
    if (!isOpen_) {
        animProgress_ = 0.0f;
        return;
    }

    float dt = ImGui::GetIO().DeltaTime;
    animProgress_ += (1.0f - animProgress_) * dt * 10.0f;
    if (animProgress_ > 0.99f) animProgress_ = 1.0f;

    // 弹窗尺寸（加大）
    float popupW = 680.0f;
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
    ImGui::Begin("##shop_overlay", nullptr,
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
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(20, 25, 35, (int)(250 * alpha)));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(60, 80, 120, (int)(200 * alpha)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

    ImGui::Begin(u8"星铸商城##shop_popup", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoSavedSettings);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 winPos = ImGui::GetWindowPos();
    ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
    ImFont* smallFont = g_mainFontSmall ? g_mainFontSmall : font;
    ImFont* largeFont = g_mainFontLarge ? g_mainFontLarge : font;

    float contentX = winPos.x + 24;
    float contentY = winPos.y + 50;

    // ═══════════════════════════════════════════════════════════════
    //  标题
    // ═══════════════════════════════════════════════════════════════
    dl->AddText(largeFont, largeFont->FontSize, ImVec2(contentX, contentY),
                IM_COL32(255, 255, 255, (int)(255 * alpha)), u8"星铸商城");

    // 当前余额
    if (IsLoggedIn()) {
        char balanceText[64];
        snprintf(balanceText, sizeof(balanceText), u8"余额: %d 星铸币", GetCurrentUser().coins);
        ImVec2 balanceSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, balanceText);
        dl->AddText(font, font->FontSize, ImVec2(contentX + popupW - 48 - balanceSize.x, contentY + 4),
                    IM_COL32(255, 200, 50, (int)(220 * alpha)), balanceText);
    }

    contentY += 50;

    // ═══════════════════════════════════════════════════════════════
    //  商品卡片网格 (3列2行)
    // ═══════════════════════════════════════════════════════════════
    float cardW = (popupW - 48 - 24) / 3.0f;  // 3列，间距12*2
    float cardH = 180.0f;
    float gapX = 12.0f;
    float gapY = 16.0f;

    for (size_t i = 0; i < shopItems_.size(); i++) {
        const ShopItem& item = shopItems_[i];
        int col = i % 3;
        int row = (int)i / 3;

        float cardX = contentX + col * (cardW + gapX);
        float cardY = contentY + row * (cardH + gapY);

        // 卡片背景（渐变效果）
        dl->AddRectFilled(ImVec2(cardX, cardY), ImVec2(cardX + cardW, cardY + cardH),
                          item.bgColor, 12.0f);

        // 顶部装饰条
        dl->AddRectFilled(ImVec2(cardX, cardY), ImVec2(cardX + cardW, cardY + 4),
                          item.color, 12.0f, ImDrawFlags_RoundCornersTop);

        // 商品名称
        ImVec2 nameSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, item.name);
        float nameX = cardX + (cardW - nameSize.x) * 0.5f;
        dl->AddText(font, font->FontSize, ImVec2(nameX, cardY + 20), item.color, item.name);

        // 商品描述（多行）
        float descY = cardY + 50;
        const char* desc = item.description;
        const char* lineStart = desc;
        while (*desc) {
            if (*desc == '\n' || *(desc + 1) == '\0') {
                int len = (int)(desc - lineStart + (*desc != '\n' ? 1 : 0));
                char line[128];
                if (len > 0 && len < 127) {
                    strncpy(line, lineStart, len);
                    line[len] = '\0';
                    ImVec2 lineSize = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, line);
                    float lineX = cardX + (cardW - lineSize.x) * 0.5f;
                    dl->AddText(smallFont, smallFont->FontSize, ImVec2(lineX, descY),
                                IM_COL32(180, 190, 200, 220), line);
                    descY += 18;
                }
                lineStart = desc + 1;
            }
            desc++;
        }

        // 价格
        char priceText[32];
        snprintf(priceText, sizeof(priceText), "%d", item.price);

        // 星铸币图标和价格
        float priceY = cardY + cardH - 70;
        ImVec2 priceSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, priceText);
        float totalPriceW = 20 + priceSize.x + 8;  // 图标 + 间距 + 文字
        float priceX = cardX + (cardW - totalPriceW) * 0.5f;

        // 金币图标
        dl->AddCircleFilled(ImVec2(priceX + 10, priceY + 10), 10, IM_COL32(255, 200, 50, 255), 16);
        dl->AddText(smallFont, smallFont->FontSize, ImVec2(priceX + 4, priceY + 3),
                    IM_COL32(80, 60, 20, 255), u8"币");

        // 价格数字
        dl->AddText(font, font->FontSize, ImVec2(priceX + 24, priceY + 2),
                    IM_COL32(255, 200, 50, 255), priceText);

        // 购买按钮
        float btnW = cardW - 24;
        float btnH = 32.0f;
        float btnX = cardX + 12;
        float btnY = cardY + cardH - 44;

        ImGui::SetCursorScreenPos(ImVec2(btnX, btnY));
        ImGui::PushID((int)(3000 + i));

        ImGui::PushStyleColor(ImGuiCol_Button, item.color);
        ImU32 hoverColor = IM_COL32(
            std::min(255, (int)((item.color >> 0) & 0xFF) + 30),
            std::min(255, (int)((item.color >> 8) & 0xFF) + 30),
            std::min(255, (int)((item.color >> 16) & 0xFF) + 30),
            255
        );
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, item.color);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

        if (ImGui::Button(u8"立即购买", ImVec2(btnW, btnH))) {
            PurchaseItem(item.id);
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        ImGui::PopID();
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace sf
