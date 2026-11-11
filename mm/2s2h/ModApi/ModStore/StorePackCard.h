#pragma once

#include <algorithm>
#include <cfloat>
#include <string>
#include <imgui.h>

// Presentation only: activation uses ImGui's selectable so mouse and keyboard behave alike.
namespace StorePackCard {

constexpr int TitleLines = 3;
constexpr int CaptionLines = 2;

struct Content {
    std::string name;
    std::string author;
    std::string version;
    ImTextureID icon = 0;
};

struct Layout {
    float base = 0.0f;
    float titleSize = 0.0f;
    float captionSize = 0.0f;
    float titleLine = 0.0f;
    float captionLine = 0.0f;
    ImVec2 start;
    ImVec2 end;
    ImVec2 imageMin;
    ImVec2 imageMax;
    float textX = 0.0f;
    float textWidth = 0.0f;
};

inline std::string SingleLine(std::string value) {
    for (char& c : value) {
        if (c == '\n' || c == '\r' || c == '\t') {
            c = ' ';
        }
    }
    return value;
}

inline bool IsUtf8Continuation(char c) {
    return (static_cast<unsigned char>(c) & 0xc0) == 0x80;
}

inline float MeasureText(const std::string& text, float size) {
    return ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0, text.c_str()).x;
}

inline std::string FitLine(const std::string& text, float size, float width) {
    std::string fitted = SingleLine(text);
    if (MeasureText(fitted, size) <= width) {
        return fitted;
    }
    while (!fitted.empty()) {
        size_t last = fitted.size() - 1;
        while (last > 0 && IsUtf8Continuation(fitted[last])) {
            --last;
        }
        fitted.erase(last);
        if (MeasureText(fitted + "...", size) <= width) {
            return fitted + "...";
        }
    }
    return "";
}

inline Layout MeasureLayout(float width) {
    Layout layout;
    layout.base = ImGui::GetFontSize();
    layout.titleSize = layout.base * 1.4f;
    layout.captionSize = layout.base * 0.85f;
    layout.titleLine = layout.titleSize * 1.1f;
    layout.captionLine = layout.captionSize * 1.4f;
    const float padding = layout.base * 0.65f;
    const float iconSize = TitleLines * layout.titleLine + CaptionLines * layout.captionLine;
    layout.start = ImGui::GetCursorScreenPos();
    layout.end = ImVec2(layout.start.x + width, layout.start.y + iconSize + 2 * padding);
    layout.imageMin = ImVec2(layout.start.x + padding, layout.start.y + padding);
    layout.imageMax = ImVec2(layout.imageMin.x + iconSize, layout.imageMin.y + iconSize);
    layout.textX = layout.imageMax.x + padding;
    layout.textWidth = std::max(layout.base, layout.end.x - padding - layout.textX);
    return layout;
}

inline void DrawFrame(const Layout& layout, bool expanded, bool hovered, bool highlighted) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float rounding = layout.base * 0.3f;
    ImGuiCol background = ImGuiCol_FrameBg;
    if (expanded) {
        background = ImGuiCol_FrameBgActive;
    } else if (hovered) {
        background = ImGuiCol_HeaderHovered;
    }
    draw->AddRectFilled(layout.start, layout.end, ImGui::GetColorU32(background), rounding);
    draw->AddRect(layout.start, layout.end, ImGui::GetColorU32(highlighted ? ImGuiCol_CheckMark : ImGuiCol_Border),
                  rounding, 0, highlighted ? 2.0f : 1.0f);
}

inline void DrawIcon(const Layout& layout, ImTextureID icon) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (icon) {
        draw->AddImage(icon, layout.imageMin, layout.imageMax);
        return;
    }
    draw->AddRectFilled(layout.imageMin, layout.imageMax, ImGui::GetColorU32(ImGuiCol_WindowBg), layout.base * 0.2f);
    const float letterSize = layout.base * 2.5f;
    const ImVec2 letter = ImGui::GetFont()->CalcTextSizeA(letterSize, FLT_MAX, 0, "P");
    const float iconSize = layout.imageMax.x - layout.imageMin.x;
    const float letterX = layout.imageMin.x + (iconSize - letter.x) / 2;
    const float letterY = layout.imageMin.y + (iconSize - letter.y) / 2;
    draw->AddText(ImGui::GetFont(), letterSize, ImVec2(letterX, letterY), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                  "P");
}

inline void DrawTextLine(const Layout& layout, float y, float size, ImU32 color, const std::string& text) {
    const ImVec4 clip(layout.textX, layout.imageMin.y, layout.textX + layout.textWidth, layout.imageMax.y);
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), size, ImVec2(layout.textX, y), color, text.c_str(), nullptr,
                                        0, &clip);
}

inline const char* FindLineEnd(const Layout& layout, const char* line, const char* textEnd) {
    ImFont* font = ImGui::GetFont();
    const char* wrap = font->CalcWordWrapPositionA(layout.titleSize / font->FontSize, line, textEnd, layout.textWidth);
    if (wrap > line) {
        return wrap;
    }
    // A single glyph wider than the card would never advance: take one whole character anyway.
    wrap = line + 1;
    while (IsUtf8Continuation(*wrap)) {
        ++wrap;
    }
    return wrap;
}

inline void DrawTitle(const Layout& layout, const std::string& name) {
    const std::string title = SingleLine(name);
    const char* remaining = title.c_str();
    const char* titleEnd = remaining + title.size();
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text);
    for (int line = 0; line < TitleLines && *remaining; ++line) {
        const char* lineEnd = FindLineEnd(layout, remaining, titleEnd);
        const bool lastLine = line == TitleLines - 1;
        const std::string text =
            lastLine ? FitLine(remaining, layout.titleSize, layout.textWidth) : std::string(remaining, lineEnd);
        DrawTextLine(layout, layout.imageMin.y + line * layout.titleLine, layout.titleSize, color, text);
        remaining = lineEnd;
        while (*remaining == ' ') {
            ++remaining;
        }
    }
}

inline void DrawCaptions(const Layout& layout, const Content& content) {
    ImVec4 dimmed = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    dimmed.w *= 0.78f;
    const ImU32 color = ImGui::GetColorU32(dimmed);
    const float y = layout.imageMin.y + TitleLines * layout.titleLine;
    const std::string author = FitLine("By " + content.author, layout.captionSize, layout.textWidth);
    const std::string version = FitLine(content.version, layout.captionSize, layout.textWidth);
    DrawTextLine(layout, y, layout.captionSize, color, author);
    DrawTextLine(layout, y + layout.captionLine, layout.captionSize, color, version);
}

inline void DrawHoverTooltip(const Content& content, bool expanded) {
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(content.name.c_str());
    ImGui::TextDisabled("By %s", content.author.c_str());
    ImGui::TextUnformatted(content.version.c_str());
    ImGui::TextDisabled("%s", expanded ? "Click to collapse" : "Click for description and options");
    ImGui::EndTooltip();
}

// Returns true on the frame the card is activated.
inline bool Draw(const Content& content, bool expanded, float width) {
    // Resets the previous table cell's text baseline before placing the card.
    ImGui::Dummy(ImVec2(0, 0));
    const Layout layout = MeasureLayout(width);
    const ImVec2 size(width, layout.end.y - layout.start.y);
    const bool activated = ImGui::Selectable("##pack-card", expanded, ImGuiSelectableFlags_None, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool highlighted = expanded || ImGui::IsItemFocused();
    DrawFrame(layout, expanded, hovered, highlighted);
    DrawIcon(layout, content.icon);
    DrawTitle(layout, content.name);
    DrawCaptions(layout, content);
    if (hovered) {
        DrawHoverTooltip(content, expanded);
    }
    return activated;
}

} // namespace StorePackCard
