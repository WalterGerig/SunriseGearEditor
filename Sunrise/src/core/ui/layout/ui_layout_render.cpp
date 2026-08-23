#include <algorithm>
#include <array>
#include <imgui.h>
#include <string_view>

#include "../../../../resources/resource.h"
#include "../animation/transition/ui_transition_animation.h"
#include "../components/card/ui_card_component.h"
#include "../components/logo/ui_logo_component.h"
#include "../components/section/ui_section_component.h"
#include "../scaling/dpi/ui_dpi_scaling.h"
#include "navigation/ui_layout_navigation.h"
#include "ui_layout_lifecycle.h"

namespace sunrise::core::ui::layout {
namespace {

/** The wider authored size gives content-heavy modules such as Gear Editor room for two columns. */
constexpr float kPreferredWindowWidth = 1180.0F;
/** The taller authored size leaves enough room for inventory and perk browsers. */
constexpr float kPreferredWindowHeight = 660.0F;
/** A 420-pixel minimum keeps the two columns from overlapping. */
constexpr float kMinimumWindowWidth = 420.0F;
/** A 300-pixel minimum keeps the navigation list and credits footer. */
constexpr float kMinimumWindowHeight = 300.0F;
/** 24 pixels keep the centered surface away from viewport edges. */
constexpr float kViewportMargin = 24.0F;
/** Two margins hold the same space on opposite viewport edges. */
constexpr float kViewportMarginCount = 2.0F;
/** 180 pixels caps the narrow module navigation. */
constexpr float kNavigationWidth = 180.0F;
/** Zero width lets Dear ImGui fill the space left on the current row. */
constexpr float kAutomaticWidth = 0.0F;
/** A half-axis pivot centers the window on both viewport axes. */
constexpr ImVec2 kCenterPivot{0.5F, 0.5F};
/** The main surface has no title bar, but keeps Dear ImGui's resize grip enabled. */
constexpr ImGuiWindowFlags kMainWindowFlags =
    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;
/** One trailing null byte turns a descriptor name into a component label. */
constexpr std::size_t kLabelTerminatorBytes = 1;
/** Fixed animation key. Every visibility-lane user needs its own, so keep these distinct. */
constexpr ImGuiID kSurfaceAnimationId = 1;
/** Response rates for opening and closing, in the same range as the other components. */
constexpr animation::transition::Rates kVisibilityRates{16.0F, 14.0F};
/** A closed surface has finished its transition and draws nothing. */
constexpr float kClosedProgress = 0.0F;
/** 34 authored pixels give the title logo presence without crowding the title row. */
constexpr float kTitleLogoExtent = 34.0F;
/** The title is drawn at this multiple of the body text, so it holds the logo's row. */
constexpr float kTitleTextRatio = 1.5F;
/** Half a difference centers one item against a taller one. */
constexpr float kHalfExtent = 2.0F;
/** The surface names the tool with the same wordmark the HUD card carries. */
constexpr char kTitle[] = "SUNRISE";

/**
 * Copies one display name into null-terminated component storage.
 * @return Fixed label storage, always with a trailing null.
 */
[[nodiscard]] std::array<char, modules::kDisplayNameCapacity + kLabelTerminatorBytes>
component_label(const modules::Descriptor& descriptor) noexcept {
    std::array<char, modules::kDisplayNameCapacity + kLabelTerminatorBytes> label{};
    const std::string_view displayName = descriptor.display_name();
    std::copy(displayName.begin(), displayName.end(), label.begin());
    return label;
}

/**
 * Works out a centered size that fits the viewport and the authored minimums.
 * @param viewport Active Dear ImGui viewport.
 * @return Main window size, or zero axes when the viewport is not ready.
 */
[[nodiscard]] ImVec2 window_size(const ImGuiViewport& viewport) noexcept {
    if (viewport.Size.x <= 0.0F || viewport.Size.y <= 0.0F) {
        return {};
    }
    const float margin = scaling::dpi::pixels(kViewportMargin);
    const float availableWidth = viewport.Size.x - (margin * kViewportMarginCount);
    const float availableHeight = viewport.Size.y - (margin * kViewportMarginCount);
    const float minimumWidth = scaling::dpi::pixels(kMinimumWindowWidth);
    const float minimumHeight = scaling::dpi::pixels(kMinimumWindowHeight);
    if (availableWidth < minimumWidth || availableHeight < minimumHeight) {
        return {};
    }
    return {(std::min)(scaling::dpi::pixels(kPreferredWindowWidth), availableWidth),
            (std::min)(scaling::dpi::pixels(kPreferredWindowHeight), availableHeight)};
}

/**
 * Draws the wide content panel and calls only the selected module's callback.
 * @param selected Descriptor copied from one registry snapshot.
 */
void draw_content(const navigation::Selection& selected) noexcept {
    if (!selected.moduleAvailable) {
        ImGui::TextDisabled("No modules are registered.");
        return;
    }
    const auto displayName = component_label(selected.descriptor);
    components::section::header(displayName.data());
    // One spacing height below the title row, so a module's first line never sits against it.
    ImGui::Dummy({kAutomaticWidth, ImGui::GetStyle().ItemSpacing.y});
    selected.descriptor.frame_callback()();
}

/** Draws the animated logo, then the name and version, on one title row. */
void draw_title() noexcept {
    const float extent = scaling::dpi::pixels(kTitleLogoExtent);
    const bool logoDrawn = components::logo::draw(extent);
    if (logoDrawn) {
        ImGui::SameLine();
    }

    // The size is the authored one, because the style carries the display scale separately.
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * kTitleTextRatio);
    const float titleHeight = ImGui::GetTextLineHeight();
    const float rowY = ImGui::GetCursorPosY();
    // The title is shorter than the logo, so it sits lower to stay level with it.
    const float titleY =
        logoDrawn ? rowY + ((std::max)(extent - titleHeight, 0.0F) / kHalfExtent) : rowY;
    ImGui::SetCursorPosY(titleY);
    ImGui::TextUnformatted(kTitle);
    ImGui::PopFont();

    ImGui::SameLine();
    // SameLine returns to the row the logo opened, so the version is placed against the title
    // again, centered on it because it stays at body size.
    ImGui::SetCursorPosY(
        titleY + ((std::max)(titleHeight - ImGui::GetTextLineHeight(), 0.0F) / kHalfExtent));
    ImGui::TextDisabled(SUNRISE_VER_STRING);
}

} // namespace

/** Draws the centered Sunrise surface inside the caller's active Dear ImGui frame. */
bool render(bool visible) noexcept {
    if (!internal::context_is_current()) {
        return false;
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (viewport == nullptr) {
        return false;
    }
    const ImVec2 size = window_size(*viewport);
    if (size.x <= 0.0F || size.y <= 0.0F) {
        return false;
    }
    // A new lane starts closed, so the surface animates open the first time it is asked for.
    const float progress = animation::transition::update(kSurfaceAnimationId,
                                                         animation::transition::Lane::visibility,
                                                         visible,
                                                         kVisibilityRates,
                                                         kClosedProgress);
    if (progress <= kClosedProgress) {
        return false;
    }

    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, kCenterPivot);

    // Set the authored size only when the window is created. After that Dear ImGui owns the size,
    // so the bottom-right resize grip can be dragged without the next frame snapping it back.
    ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);

    // Keep manual resizing inside the active game viewport and preserve the existing layout minimums.
    const float margin = scaling::dpi::pixels(kViewportMargin);
    const ImVec2 minimumSize{scaling::dpi::pixels(kMinimumWindowWidth),
                             scaling::dpi::pixels(kMinimumWindowHeight)};
    const ImVec2 maximumSize{viewport->Size.x - (margin * kViewportMarginCount),
                             viewport->Size.y - (margin * kViewportMarginCount)};
    ImGui::SetNextWindowSizeConstraints(minimumSize, maximumSize);
    // One style alpha fades the surface and everything drawn inside it together.
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, progress);
    const bool submitContents = ImGui::Begin("Sunrise", nullptr, kMainWindowFlags);
    if (submitContents) {
        draw_title();
        ImGui::Separator();

        const StateSnapshot state = snapshot();
        navigation::Selection selected{};
        const float panelHeight = ImGui::GetContentRegionAvail().y;
        {
            const components::card::Scope navigationCard(
                "##navigation_card", ImVec2(scaling::dpi::pixels(kNavigationWidth), panelHeight));
            if (navigationCard.visible()) {
                selected = navigation::draw(state);
            }
        }
        ImGui::SameLine();
        {
            const components::card::Scope contentCard("##content_card",
                                                      ImVec2(kAutomaticWidth, panelHeight));
            if (contentCard.visible()) {
                draw_content(selected);
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    return true;
}

} // namespace sunrise::core::ui::layout
