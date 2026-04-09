#pragma once
#include "imgui.h"
#include <string>
#include <functional>

namespace sf {

class SearchBar {
public:
    // Render search bar, returns true if text changed
    bool Render(float width);

    const std::string& GetText() const { return text_; }
    void Clear() { text_.clear(); buf_[0] = '\0'; }

private:
    char buf_[256] = {};
    std::string text_;
};

} // namespace sf
