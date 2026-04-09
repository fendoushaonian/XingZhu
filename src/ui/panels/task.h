#pragma once
#include "imgui.h"
#include <string>
#include <vector>

namespace sf {

// 任务类型
enum class TaskType {
    Daily,      // 每日任务
    Weekly,     // 每周任务
    Achievement // 成就任务
};

// 任务状态
enum class TaskStatus {
    Locked,     // 未解锁
    InProgress, // 进行中
    Claimable,  // 可领取
    Completed   // 已完成
};

struct TaskItem {
    int id;
    TaskType type;
    const char* name;
    const char* description;
    int rewardCoins;
    int currentProgress;
    int targetProgress;
    TaskStatus status;
};

class TaskPanel {
public:
    void Open();
    void Close();
    void Render(float winW, float winH);
    bool IsOpen() const { return isOpen_; }

private:
    bool isOpen_ = false;
    float animProgress_ = 0.0f;
    int selectedTab_ = 0;  // 0=每日, 1=每周, 2=成就

    void RefreshTasks();
    void ClaimReward(int taskId);
    void RenderTaskList(const std::vector<TaskItem>& tasks, float startY, float width);

    std::vector<TaskItem> dailyTasks_;
    std::vector<TaskItem> weeklyTasks_;
    std::vector<TaskItem> achievementTasks_;
};

} // namespace sf
