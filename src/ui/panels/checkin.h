#pragma once
#include "imgui.h"
#include <string>
#include <vector>

namespace sf {

class CheckInPanel {
public:
    void Open();
    void Close();
    void Render(float winW, float winH);
    bool IsOpen() const { return isOpen_; }

private:
    bool isOpen_ = false;
    float animProgress_ = 0.0f;

    // 当前月份数据
    int currentYear_ = 0;
    int currentMonth_ = 0;
    int daysInMonth_ = 0;
    int firstDayOfWeek_ = 0;  // 0=周日, 1=周一...
    std::vector<bool> checkedDays_;  // 本月已签到的日期

    void RefreshCalendarData();
    void DoCheckIn();
};

} // namespace sf
