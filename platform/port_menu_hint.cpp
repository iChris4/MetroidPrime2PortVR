// The launch hint as game text (include/port_menu_hint.h).
#include "port_menu_hint.h"

#include "port_game_ui.h"
#include "port_input_remap_ui.h"
#include "port_overlay.h"

#include <algorithm>
#include <chrono>
#include <string>

namespace PortMenuHint {
namespace {
constexpr double kShowSeconds = 8.0;
constexpr double kFadeSeconds = 1.0;

// Not a unique_ptr (see port_game_ui.h: a text deleted while the program exits hangs it); freed
// once the hint is done.
PortGameUi::Text* sText = nullptr;
bool sDone = false;
bool sShowing = false;
std::chrono::steady_clock::time_point sShownAt;

void Finish() {
  sDone = true;
  delete sText;
  sText = nullptr;
}

std::string HintText() {
  const std::string keys = PortInputRemap::BindingsText(PortInput::Action::PortMenu);
  return keys.empty() ? "Open settings from the Port menu binding in Remap" : "Press " + keys + " for settings";
}
} // namespace

void Draw() {
  if (sDone) {
    return;
  }
  if (!PortOverlay::ShowMenuHint() || PortOverlay::Visible()) {
    Finish();
    return;
  }
  if (sText == nullptr) {
    // Where the overlay's toast was: the top left corner.
    sText = new PortGameUi::Text(PortGameUi::TextSpec{
        .text = HintText(),
        .scale = 0.55f,
        .placement = {PortGameUi::Anchor::TopLeft, 0.025f, 0.025f},
    });
  }
  // The 8 seconds start when it can first be seen (its frame and font take a frame or two to load).
  double age = 0.0;
  if (sShowing) {
    age = std::chrono::duration< double >(std::chrono::steady_clock::now() - sShownAt).count();
    if (age > kShowSeconds) {
      Finish();
      return;
    }
  }
  sText->SetAlpha(static_cast< float >(std::clamp((kShowSeconds - age) / kFadeSeconds, 0.0, 1.0)));
  sText->Draw();
  if (!sShowing && sText->Ready()) {
    sShowing = true;
    sShownAt = std::chrono::steady_clock::now();
  }
}

PortGameUi::ScreenRect LastRect() {
  return sText != nullptr && sShowing ? sText->LastRect() : PortGameUi::ScreenRect{};
}
} // namespace PortMenuHint
