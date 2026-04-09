#pragma once
#include "imgui.h"
#include <string>

namespace sf {

enum class MainTab { Store, Library, Community, Tools };

class HeaderBar {
public:
    void Render(float width, float height = 48.0f);
    MainTab GetActiveTab() const { return activeTab_; }
    void SetActiveTab(MainTab tab) { activeTab_ = tab; }
    bool WasLogoutClicked();
    bool WasProfileClicked();
    bool WasVipClicked();

private:
    MainTab activeTab_ = MainTab::Library;
    float tabAnim_[4] = {};
    bool userDropdownOpen_ = false;
    float dropdownAnim_ = 0.0f;
    bool logoutClicked_ = false;
    bool profileClicked_ = false;
    bool vipClicked_ = false;
};

class StatusBar {
public:
    void Render(float y, float width, float height = 28.0f);
    void RenderCdkPopup();  // CDK弹窗渲染

private:
    bool cdkPopupOpen_ = false;
    char cdkInput_[128] = {};
};

} // namespace sf
