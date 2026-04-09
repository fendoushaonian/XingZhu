#include "ui/panels/profile.h"
#include "ui/iconfonts.h"
#include "core/auth.h"
#include "core/texture_manager.h"
#include "core/steam_library.h"
#include <cstring>
#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <filesystem>

namespace sf {

extern ImFont* g_mainFontLarge;
extern ImFont* g_mainFontSmall;
extern ImFont* g_mainFont;
extern ImFont* g_iconFont;
extern ImFont* g_iconFontLarge;

// Copy avatar to app data folder, return new path
static std::string CopyAvatarToLocal(const std::string& srcPath) {
    wchar_t appData[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appData)))
        return srcPath;

    std::filesystem::path dir = std::filesystem::path(appData) / L"SteamForge" / L"avatars";
    std::filesystem::create_directories(dir);

    std::filesystem::path src(srcPath);
    std::string ext = src.extension().string();
    std::string filename = "avatar_" + std::to_string(GetCurrentUser().id) + ext;
    std::filesystem::path dst = dir / filename;

    try {
        std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing);
    } catch (...) {
        return srcPath;
    }

    return dst.string();
}

static std::string OpenFileDialog() {
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = "Select Avatar";

    if (GetOpenFileNameA(&ofn))
        return std::string(filename);
    return "";
}

void ProfilePanel::Open() {
    open_ = true;
    anim_ = 0.0f;
    saveMsg_.clear();
    saveMsgTimer_ = 0.0f;
    dirty_ = false;

    if (IsLoggedIn()) {
        const auto& u = GetCurrentUser();
        strncpy(editNick_, u.nickname.c_str(), sizeof(editNick_) - 1);
        editNick_[sizeof(editNick_) - 1] = '\0';

        strncpy(editSteamId_, u.steamId.c_str(), sizeof(editSteamId_) - 1);
        editSteamId_[sizeof(editSteamId_) - 1] = '\0';
        steamIdDirty_ = false;
        steamIdMsg_.clear();
        steamIdMsgTimer_ = 0.0f;

        avatarPath_ = u.avatarUrl;
        avatarTex_ = (ImTextureID)0;
        avatarLoaded_ = false;
        if (!avatarPath_.empty()) {
            avatarTex_ = LoadTextureFromLocalFile("user_avatar", avatarPath_);
            avatarLoaded_ = (avatarTex_ != (ImTextureID)0);
        }
    }
}

void ProfilePanel::Render(float winW, float winH) {
    if (!open_ && anim_ < 0.01f) return;

    float dt = ImGui::GetIO().DeltaTime;

    float target = open_ ? 1.0f : 0.0f;
    anim_ += (target - anim_) * dt * 14.0f;
    anim_ = std::clamp(anim_, 0.0f, 1.0f);
    if (!open_ && anim_ < 0.01f) { anim_ = 0.0f; return; }

    int overlayAlpha = (int)(120 * anim_);

    // Dark overlay
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(winW, winH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, overlayAlpha));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGuiWindowFlags overlayFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    if (ImGui::Begin("##ProfileOverlay", nullptr, overlayFlags)) {
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) && ImGui::IsMouseClicked(0))
            open_ = false;
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    // Panel
    float pw = 420, ph = 540;
    float px = (winW - pw) * 0.5f;
    float py = (winH - ph) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(px, py));
    ImGui::SetNextWindowSize(ImVec2(pw, ph));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(22, 28, 40, (int)(255 * anim_)));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(50, 70, 100, (int)(80 * anim_)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 16));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("##ProfilePanel", nullptr, panelFlags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* font = g_mainFont ? g_mainFont : ImGui::GetFont();
        ImFont* smallFont = g_mainFontSmall ? g_mainFontSmall : font;
        ImFont* largeFont = g_mainFontLarge ? g_mainFontLarge : font;
        ImVec2 wp = ImGui::GetWindowPos();
        float fieldW = pw - 48;
        float leftX = wp.x + 24;
        float rightX = wp.x + pw - 24;

        const auto& user = GetCurrentUser();
        ImVec2 mp = ImGui::GetMousePos();

        // ── Title + Close ──
        ImGui::PushFont(largeFont);
        ImGui::TextColored(ImVec4(0.86f, 0.92f, 0.98f, anim_), u8"\u8D26\u6237\u4FE1\u606F");
        ImGui::PopFont();

        float closeX = wp.x + pw - 40;
        float closeY = wp.y + 14;
        bool closeHov = (mp.x >= closeX && mp.x <= closeX + 24 && mp.y >= closeY && mp.y <= closeY + 24);
        if (closeHov)
            dl->AddRectFilled(ImVec2(closeX - 2, closeY - 2), ImVec2(closeX + 26, closeY + 26),
                              IM_COL32(255, 255, 255, 15), 4.0f);
        if (g_iconFont)
            DrawIcon(dl, icon::CLOSE, ImVec2(closeX + 4, closeY + 4),
                     IM_COL32(160, 175, 190, closeHov ? 255 : 150));
        if (closeHov && ImGui::IsMouseClicked(0))
            open_ = false;

        ImGui::Spacing();
        float sepY = ImGui::GetCursorScreenPos().y;
        dl->AddLine(ImVec2(leftX - 8, sepY), ImVec2(rightX + 8, sepY),
                    IM_COL32(50, 70, 100, (int)(60 * anim_)));
        ImGui::Dummy(ImVec2(0, 16));

        // ── Avatar (centered) + click to change ──
        float avatarR = 40.0f;
        float avatarCX = leftX + fieldW * 0.5f;
        float avatarCY = ImGui::GetCursorScreenPos().y + avatarR;

        // Hover detection (circular)
        float ddx = mp.x - avatarCX, ddy = mp.y - avatarCY;
        bool avatarHov = (ddx * ddx + ddy * ddy) <= (avatarR + 4) * (avatarR + 4);

        // Ring
        dl->AddCircleFilled(ImVec2(avatarCX, avatarCY), avatarR + 3,
                            IM_COL32(40, 60, 100, avatarHov ? 200 : 160));
        dl->AddCircleFilled(ImVec2(avatarCX, avatarCY), avatarR,
                            IM_COL32(30, 50, 80, 255));

        if (avatarLoaded_ && avatarTex_) {
            // Draw avatar image clipped to circle
            ImVec2 imgTL(avatarCX - avatarR, avatarCY - avatarR);
            ImVec2 imgBR(avatarCX + avatarR, avatarCY + avatarR);
            dl->AddImageRounded(avatarTex_, imgTL, imgBR,
                                ImVec2(0, 0), ImVec2(1, 1),
                                IM_COL32(255, 255, 255, 255), avatarR);
        } else {
            // Default user icon — properly centered
            ImFont* iconF = g_iconFontLarge ? g_iconFontLarge : g_iconFont;
            if (iconF) {
                ImVec2 iconSize = iconF->CalcTextSizeA(iconF->FontSize, FLT_MAX, 0, icon::USER);
                dl->AddText(iconF, iconF->FontSize,
                            ImVec2(avatarCX - iconSize.x * 0.5f, avatarCY - iconSize.y * 0.5f),
                            IM_COL32(130, 175, 215, 255), icon::USER);
            }
        }

        // Border
        dl->AddCircle(ImVec2(avatarCX, avatarCY), avatarR,
                      IM_COL32(102, 192, 244, avatarHov ? 160 : 60), 0, 2.0f);

        // Hover overlay
        if (avatarHov) {
            dl->AddCircleFilled(ImVec2(avatarCX, avatarCY), avatarR, IM_COL32(0, 0, 0, 120));
            const char* editText = u8"\u7F16\u8F91";
            ImVec2 ts = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, editText);
            dl->AddText(smallFont, smallFont->FontSize,
                        ImVec2(avatarCX - ts.x * 0.5f, avatarCY - ts.y * 0.5f),
                        IM_COL32(255, 255, 255, 220), editText);
        }

        // Click to open file picker
        if (avatarHov && ImGui::IsMouseClicked(0)) {
            std::string path = OpenFileDialog();
            if (!path.empty()) {
                std::string localPath = CopyAvatarToLocal(path);
                std::string texKey = "user_avatar_" + std::to_string(user.id);
                avatarTex_ = LoadTextureFromLocalFile(texKey, localPath);
                if (avatarTex_) {
                    avatarLoaded_ = true;
                    avatarPath_ = localPath;
                    AuthUpdateAvatar(localPath);
                    saveMsg_ = u8"\u5934\u50CF\u5DF2\u66F4\u65B0";
                    saveMsgTimer_ = 2.0f;
                }
            }
        }

        // Online dot
        dl->AddCircleFilled(ImVec2(avatarCX + avatarR * 0.7f, avatarCY + avatarR * 0.7f),
                            6.0f, IM_COL32(22, 28, 40, 255));
        dl->AddCircleFilled(ImVec2(avatarCX + avatarR * 0.7f, avatarCY + avatarR * 0.7f),
                            4.0f, IM_COL32(87, 203, 100, 255));

        ImGui::Dummy(ImVec2(0, avatarR * 2 + 8));

        // ── Username (centered) ──
        float cy = ImGui::GetCursorScreenPos().y;
        ImVec2 nameSize = largeFont->CalcTextSizeA(largeFont->FontSize, FLT_MAX, 0, user.username.c_str());
        dl->AddText(largeFont, largeFont->FontSize,
                    ImVec2(avatarCX - nameSize.x * 0.5f, cy),
                    IM_COL32(220, 235, 250, 255), user.username.c_str());

        ImVec2 statusSize = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0, u8"\u5728\u7EBF");
        dl->AddText(smallFont, smallFont->FontSize,
                    ImVec2(avatarCX - statusSize.x * 0.5f, cy + largeFont->FontSize + 2),
                    IM_COL32(87, 203, 100, 180), u8"\u5728\u7EBF");

        ImGui::Dummy(ImVec2(0, largeFont->FontSize + smallFont->FontSize + 14));

        // ── Nickname ──
        ImGui::TextColored(ImVec4(0.55f, 0.61f, 0.69f, 0.8f), u8"\u6635\u79F0");
        ImGui::Dummy(ImVec2(0, 2));

        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(10, 16, 28, 255));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(16, 24, 40, 255));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(16, 24, 40, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(40, 60, 90, 100));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 230, 245, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::SetNextItemWidth(fieldW);
        if (ImGui::InputText("##editNick", editNick_, sizeof(editNick_)))
            dirty_ = true;
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(5);

        ImGui::Dummy(ImVec2(0, 8));

        // ── Registration date ──
        ImGui::TextColored(ImVec4(0.55f, 0.61f, 0.69f, 0.8f), u8"\u6CE8\u518C\u65F6\u95F4");
        ImGui::Dummy(ImVec2(0, 2));

        float fy = ImGui::GetCursorScreenPos().y;
        dl->AddRectFilled(ImVec2(leftX, fy), ImVec2(rightX, fy + 40),
                          IM_COL32(10, 16, 28, 200), 6.0f);
        dl->AddRect(ImVec2(leftX, fy), ImVec2(rightX, fy + 40),
                    IM_COL32(40, 60, 90, 60), 6.0f, 0, 1.0f);
        const char* dateStr = user.createdAt.empty() ? "-" : user.createdAt.c_str();
        dl->AddText(font, font->FontSize,
                    ImVec2(leftX + 12, fy + 10),
                    IM_COL32(120, 140, 165, 180), dateStr);
        ImGui::Dummy(ImVec2(0, 52));

        // ── Steam ID ──
        ImGui::TextColored(ImVec4(0.55f, 0.61f, 0.69f, 0.8f), "Steam ID");
        ImGui::Dummy(ImVec2(0, 2));

        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(10, 16, 28, 255));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(16, 24, 40, 255));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(16, 24, 40, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(40, 60, 90, 100));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 230, 245, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        float steamFieldW = fieldW - 80;
        ImGui::SetNextItemWidth(steamFieldW);
        if (ImGui::InputTextWithHint("##editSteamId", u8"Steam ID \u6216 \u81EA\u5B9A\u4E49URL",
                                      editSteamId_, sizeof(editSteamId_)))
            steamIdDirty_ = true;

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(5);

        // Bind button on same line
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(26, 120, 220, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(40, 150, 255, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(20, 100, 200, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        if (ImGui::Button(u8"\u7ED1\u5B9A##steam", ImVec2(68, 0))) {
            if (editSteamId_[0] != '\0') {
                std::string sid(editSteamId_);
                // Save to DB
                if (AuthUpdateSteamId(sid)) {
                    steamIdMsg_ = u8"Steam ID \u5DF2\u4FDD\u5B58\uFF0C\u6B63\u5728\u52A0\u8F7D\u6E38\u620F\u5E93..."; // Steam ID 已保存，正在加载游戏库...
                    // Start async fetch of Steam game library
                    RequestSteamBind(sid);
                } else {
                    steamIdMsg_ = u8"\u4FDD\u5B58\u5931\u8D25"; // 保存失败
                }
                steamIdMsgTimer_ = 3.0f;
                steamIdDirty_ = false;
            }
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        // Status message
        if (steamIdMsgTimer_ > 0) {
            steamIdMsgTimer_ -= dt;
            float a = steamIdMsgTimer_ < 0.5f ? steamIdMsgTimer_ * 2.0f : 1.0f;
            ImGui::TextColored(ImVec4(0.39f, 0.86f, 0.47f, a), "%s", steamIdMsg_.c_str());
        } else {
            ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeight()));
        }

        ImGui::Dummy(ImVec2(0, 4));

        // ── Save button ──
        if (dirty_) {
            float btnW = 100, btnH = 36;
            float btnX = rightX - btnW;
            fy = ImGui::GetCursorScreenPos().y;

            bool btnHov = (mp.x >= btnX && mp.x <= btnX + btnW && mp.y >= fy && mp.y <= fy + btnH);
            ImU32 btnCol = btnHov ? IM_COL32(40, 150, 255, 255) : IM_COL32(26, 120, 220, 255);

            dl->AddRectFilled(ImVec2(btnX, fy), ImVec2(btnX + btnW, fy + btnH), btnCol, 6.0f);
            const char* saveText = u8"\u4FDD\u5B58";
            ImVec2 ts = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, saveText);
            dl->AddText(font, font->FontSize,
                        ImVec2(btnX + (btnW - ts.x) * 0.5f, fy + (btnH - ts.y) * 0.5f),
                        IM_COL32(255, 255, 255, 255), saveText);

            if (btnHov && ImGui::IsMouseClicked(0)) {
                if (AuthUpdateNickname(editNick_)) {
                    saveMsg_ = u8"\u4FDD\u5B58\u6210\u529F";
                    dirty_ = false;
                } else {
                    saveMsg_ = u8"\u4FDD\u5B58\u5931\u8D25";
                }
                saveMsgTimer_ = 2.0f;
            }
        }

        // Status message
        if (saveMsgTimer_ > 0) {
            saveMsgTimer_ -= dt;
            float a = saveMsgTimer_ < 0.5f ? saveMsgTimer_ * 2.0f : 1.0f;
            ImGui::TextColored(ImVec4(0.39f, 0.86f, 0.47f, a), "%s", saveMsg_.c_str());
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

} // namespace sf
