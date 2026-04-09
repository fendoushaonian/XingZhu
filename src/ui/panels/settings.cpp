#include "ui/panels/settings.h"
#include "app/config.h"
#include "ui/widgets/modern.h"
#include "ui/icons.h"

namespace sf {

void SettingsPanel::Render(float x, float y, float width, float height) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(23, 26, 33, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

    if (ImGui::Begin("##Settings", nullptr, flags)) {
        ui::SectionHeader("SETTINGS");
        ImGui::Spacing();

        ImGui::BeginChild("##settingsScroll", ImVec2(width - 48, height - 60), false);

        float inputW = 350.0f;

        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(30, 36, 44, 255));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(42, 52, 64, 255));
        ImGui::PushStyleColor(ImGuiCol_CheckMark, IM_COL32(26, 159, 255, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));

        // Steam API
        ui::SectionHeader("STEAM API CONFIGURATION", IM_COL32(26, 159, 255, 255));
        ImGui::Spacing();

        ImGui::Text("Steam Web API Key");
        ImGui::SetNextItemWidth(inputW);
        ImGui::InputText("##apikey", apiKey_, sizeof(apiKey_), ImGuiInputTextFlags_Password);
        ImGui::SameLine(0, 12);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(143, 152, 160, 140));
        ImGui::Text("Get from steamcommunity.com/dev/apikey");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Text("Steam ID (64-bit)");
        ImGui::SetNextItemWidth(inputW);
        ImGui::InputText("##steamid", steamId_, sizeof(steamId_));

        ImGui::Spacing();
        ImGui::Spacing();
        ui::Divider(width - 48);
        ImGui::Spacing();

        // General
        ui::SectionHeader("GENERAL", IM_COL32(87, 203, 100, 255));
        ImGui::Spacing();

        ImGui::Checkbox("Start with Windows", &autoStart_);
        ImGui::Checkbox("Minimize to system tray", &minimizeToTray_);
        ImGui::Checkbox("Check for updates on startup", &checkUpdates_);

        ImGui::Spacing();
        ui::Divider(width - 48);
        ImGui::Spacing();

        // Card Farmer
        ui::SectionHeader("CARD FARMER", IM_COL32(76, 107, 34, 255));
        ImGui::Spacing();

        const char* farmMethods[] = { "Normal (one game at a time)", "Fast (all games at once)", "Smart (2hr refund bypass)" };
        ImGui::Text("Farming Method");
        ImGui::SetNextItemWidth(inputW);
        ImGui::Combo("##farmmethod", &farmMethod_, farmMethods, IM_ARRAYSIZE(farmMethods));

        ImGui::Checkbox("Start farming on application launch", &farmOnStartup_);

        ImGui::Spacing();
        ui::Divider(width - 48);
        ImGui::Spacing();

        // Network
        ui::SectionHeader("NETWORK", IM_COL32(220, 80, 60, 255));
        ImGui::Spacing();

        ImGui::Checkbox("Use proxy", &useProxy_);
        if (useProxy_) {
            ImGui::Spacing();
            ImGui::Text("Proxy Host");
            ImGui::SetNextItemWidth(inputW);
            ImGui::InputText("##proxyhost", proxyHost_, sizeof(proxyHost_));

            ImGui::Spacing();
            ImGui::Text("Proxy Port");
            ImGui::SetNextItemWidth(120);
            ImGui::InputInt("##proxyport", &proxyPort_);
        }

        ImGui::Spacing();
        ui::Divider(width - 48);
        ImGui::Spacing();

        // Appearance
        ui::SectionHeader("APPEARANCE", IM_COL32(100, 65, 165, 255));
        ImGui::Spacing();

        const char* scales[] = { "Small", "Medium", "Large" };
        ImGui::Text("UI Scale");
        ImGui::SetNextItemWidth(200);
        ImGui::Combo("##scale", &uiScale_, scales, IM_ARRAYSIZE(scales));

        ImGui::Checkbox("Enable animations", &showAnimations_);

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::Spacing();

        ui::ButtonSuccess("  Save Settings", ImVec2(180, 42), icons::DrawCheck);

        ImGui::EndChild();
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ==================== AboutPanel ====================

void AboutPanel::Render(float x, float y, float width, float height) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, height));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(23, 26, 33, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

    if (ImGui::Begin("##About", nullptr, flags)) {
        ImGui::Spacing();
        ImGui::Spacing();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        float cx = x + width * 0.5f;
        ImVec2 winPos = ImGui::GetWindowPos();

        // Large logo
        float logoR = 40.0f;
        float logoY = winPos.y + 40 + logoR;
        dl->AddCircleFilled(ImVec2(cx, logoY), logoR + 6, IM_COL32(26, 159, 255, 10));
        dl->AddCircleFilled(ImVec2(cx, logoY), logoR, IM_COL32(26, 159, 255, 25));
        dl->AddCircle(ImVec2(cx, logoY), logoR, IM_COL32(26, 159, 255, 150), 0, 2.0f);

        // SF letters
        float lx = cx - 12, ly = logoY - 14;
        dl->AddBezierCubic(
            ImVec2(lx + 18, ly), ImVec2(lx + 22, ly + 5), ImVec2(lx - 4, ly + 12), ImVec2(lx + 2, ly + 16),
            IM_COL32(26, 159, 255, 255), 3.5f);
        dl->AddBezierCubic(
            ImVec2(lx + 2, ly + 16), ImVec2(lx + 8, ly + 20), ImVec2(lx + 24, ly + 18), ImVec2(lx + 2, ly + 28),
            IM_COL32(26, 159, 255, 255), 3.5f);
        float fx = cx + 5;
        dl->AddLine(ImVec2(fx, ly), ImVec2(fx, ly + 28), IM_COL32(102, 192, 244, 255), 3.5f);
        dl->AddLine(ImVec2(fx, ly), ImVec2(fx + 15, ly), IM_COL32(102, 192, 244, 255), 2.5f);
        dl->AddLine(ImVec2(fx, ly + 12), ImVec2(fx + 10, ly + 12), IM_COL32(102, 192, 244, 255), 2.0f);

        ImGui::SetCursorPosY(100 + logoR);

        // Title
        const char* titleText = "STEAM FORGE";
        ImVec2 titleSize = ImGui::CalcTextSize(titleText);
        ImGui::SetCursorPosX((width - titleSize.x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(26, 159, 255, 255));
        ImGui::Text("%s", titleText);
        ImGui::PopStyleColor();

        const char* verText = "Version 1.0.0  |  The Ultimate Steam Power Tool";
        ImVec2 verSize = ImGui::CalcTextSize(verText);
        ImGui::SetCursorPosX((width - verSize.x) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(143, 152, 160, 180));
        ImGui::Text("%s", verText);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Spacing();
        ui::Divider(width - 48);
        ImGui::Spacing();

        ui::SectionHeader("FEATURES");
        ImGui::Spacing();

        const char* features[] = {
            "Game library management with Grid/List views",
            "Steam store browser with special offers",
            "Auto-discover and claim free games & DLC",
            "Trading card auto-farmer (3 modes)",
            "Achievement manager with progress tracking",
            "Cloud save backup and conflict resolution",
            "Steam Guard 2FA authenticator",
            "Steam Market price watcher",
            "Network optimization for downloads",
            "Full Chinese language support",
        };

        for (auto feat : features) {
            ImGui::SetCursorPosX(24);
            icons::DrawCheck(dl,
                ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + 2),
                14, IM_COL32(87, 203, 100, 200));
            ImGui::SetCursorPosX(44);
            ImGui::Text("%s", feat);
        }

        ImGui::Spacing();
        ImGui::Spacing();
        ui::Divider(width - 48);
        ImGui::Spacing();

        ui::SectionHeader("TECH STACK");
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(143, 152, 160, 200));
        ImGui::BulletText("C++17 with modern idioms");
        ImGui::BulletText("Dear ImGui + DirectX 11 (GPU accelerated)");
        ImGui::BulletText("Custom SVG-style vector icon system");
        ImGui::BulletText("Steam Web API integration (WinHTTP native)");
        ImGui::BulletText("nlohmann/json for configuration");
        ImGui::PopStyleColor();
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

} // namespace sf
