// The cinematic skip's prompt as a game box (include/port_skip_prompt.h).
#include "port_skip_prompt.h"

#include "port_overlay.h"

namespace PortSkipPrompt {
namespace {
// Never destroyed: a box deleted while the program exits waits on a renderer already gone (its
// models are freed through the render thread). The OS takes it back with everything else.
PortGameUi::Box* sBox = nullptr;
PortGameUi::ScreenRect sRect;
// Draw ran since the last DrawLate.
bool sDrawn = false;
} // namespace

void Draw() {
  sDrawn = true;
  const float alpha = PortOverlay::SkipPromptBox() ? PortOverlay::SkipConfirmAlpha() : 0.f;
  if (alpha <= 0.f || Unavailable()) {
    sRect = PortGameUi::ScreenRect{};
    return;
  }
  if (sBox == nullptr) {
    // The scan box's line, no fill, at the bottom: over the cutscene's letterbox, its button just
    // above the screen's edge.
    sBox = new PortGameUi::Box(PortGameUi::BoxSpec{
        .style = PortGameUi::Style::Scan,
        .fill = PortGameUi::Fill::None,
        .text = "Skip cutscene?",
        .scale = 0.75f,
        .placement = {PortGameUi::Anchor::Bottom, 0.f, 0.012f},
    });
  }
  sBox->SetAlpha(alpha);
  sBox->Draw();
  sRect = sBox->LastRect();
}

void DrawLate() {
  if (!sDrawn) {
    Draw();
  }
  sDrawn = false;
}

PortGameUi::ScreenRect LastRect() { return sRect; }

bool Unavailable() { return sBox != nullptr && sBox->Failed(); }
} // namespace PortSkipPrompt
