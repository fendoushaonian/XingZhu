#pragma once
#include "imgui.h"
#include <string>
#include <vector>

namespace sf {

class LoginPanel {
public:
    void Render(float winW, float winH);
    bool IsAuthenticated() const { return authenticated_; }
    void SetAuthenticated(bool v) {
        authenticated_ = v;
        if (!v) {
            // Reset OAuth state on logout
            oauthWaiting_ = false;
            oauthType_.clear();
            oauthTimeout_ = 0.0f;
        }
    }

private:
    bool authenticated_ = false;
    bool showRegister_ = false;

    // Login fields
    char loginUser_[64] = {};
    char loginPass_[64] = {};

    // Register fields
    char regUser_[64] = {};
    char regPass_[64] = {};
    char regPass2_[64] = {};
    char regNick_[64] = {};
    char regEmail_[128] = {};
    char regCode_[16] = {};
    bool codeSent_ = false;

    // OAuth state
    bool oauthWaiting_ = false;
    std::string oauthType_; // "steam" or "github"
    float oauthTimeout_ = 0.0f; // Timeout counter for OAuth

    // Status
    std::string errorMsg_;
    std::string successMsg_;
    float msgTimer_ = 0;

    // Animation
    float formAlpha_ = 0.0f;
    float cardSlide_ = 30.0f;

    // Focus tracking
    bool focusUser_ = false;
    bool focusPass_ = false;
    bool focusPass2_ = false;
    bool focusNick_ = false;
    bool focusEmail_ = false;
    bool focusCode_ = false;

    // Background game images
    bool bgImagesRequested_ = false;
    float bgScrollX_ = 0;

    // Helpers
    void RenderBackground(ImDrawList* bg, float winW, float winH, float t);
    void RenderCard(ImDrawList* bg, float cardX, float cardY, float cardW, float cardH, float t);
    void RenderLogo(ImDrawList* bg, float cx, float cy, float t);
    bool RenderInputField(const char* id, const char* label, const char* iconGlyph,
                          char* buf, int bufSize, float x, float y, float w,
                          bool isPassword = false, bool enterReturns = false);
    bool RenderButton(const char* label, float x, float y, float w, float h,
                      ImU32 color, ImU32 hoverColor, bool isPrimary = true);
    bool RenderIconButton(const char* id, float x, float y, float size,
                          ImU32 bgColor, ImU32 hoverColor, const char* iconGlyph);
    void HandleOAuthCallback();
};

} // namespace sf
