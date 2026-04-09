#include "ui/widgets/search_bar.h"
#include "app/config.h"

namespace sf {

bool SearchBar::Render(float width) {
    bool changed = false;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14, 8));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ColorU32(colors::BG_DARKEST));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ColorU32(colors::BG_HOVER));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ColorU32(colors::BG_SECONDARY));
    ImGui::PushStyleColor(ImGuiCol_Text, ColorU32(colors::TEXT_PRIMARY));

    ImGui::SetNextItemWidth(width);
    if (ImGui::InputTextWithHint("##search", "Search games...", buf_, sizeof(buf_))) {
        text_ = buf_;
        changed = true;
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    return changed;
}

} // namespace sf
