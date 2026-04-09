#pragma once
#include "imgui.h"
#include <string>
#include <vector>

namespace sf {

// 奖品类型
enum class PrizeType {
    Coins10,      // 10 星铸币
    Coins50,      // 50 星铸币
    Coins100,     // 100 星铸币
    Coins500,     // 500 星铸币
    VipDay1,      // 1天VIP
    VipDay3,      // 3天VIP
    VipDay7,      // 7天VIP
    Thanks        // 谢谢参与
};

struct Prize {
    PrizeType type;
    const char* name;
    ImU32 color;
    int weight;  // 权重（概率）
};

class LotteryPanel {
public:
    void Open();
    void Close();
    void Render(float winW, float winH);
    bool IsOpen() const { return isOpen_; }

private:
    bool isOpen_ = false;
    float animProgress_ = 0.0f;

    // 转盘状态
    bool isSpinning_ = false;
    float spinAngle_ = 0.0f;
    float spinSpeed_ = 0.0f;
    float targetAngle_ = 0.0f;
    int resultIndex_ = -1;
    bool showResult_ = false;

    // 抽奖消耗
    static constexpr int LOTTERY_COST = 50;  // 每次抽奖消耗50星铸币

    void DoLottery();
    void UpdateSpin(float dt);
    int SelectPrize();
    void ApplyPrize(int prizeIndex);
};

} // namespace sf
