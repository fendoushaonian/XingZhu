#pragma once
#include "imgui.h"
#include <string>
#include <vector>

namespace sf {

// 商品类型
enum class ShopItemType {
    Vip,        // VIP会员
    BlindBox,   // 游戏盲盒
    CDK         // 正版游戏CDK
};

struct ShopItem {
    int id;
    ShopItemType type;
    const char* name;
    const char* description;
    int price;          // 星铸币价格
    int value;          // 数值（VIP天数/盲盒价值等）
    ImU32 color;
    ImU32 bgColor;
};

class ShopPanel {
public:
    void Open();
    void Close();
    void Render(float winW, float winH);
    bool IsOpen() const { return isOpen_; }

private:
    bool isOpen_ = false;
    float animProgress_ = 0.0f;

    void RefreshShopItems();
    void PurchaseItem(int itemId);

    std::vector<ShopItem> shopItems_;
};

} // namespace sf
