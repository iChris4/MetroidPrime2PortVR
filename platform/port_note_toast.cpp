// A short note in the game's text (include/port_note_toast.h).
#include "port_note_toast.h"

#include "port_game_ui.h"
#include "port_menu_hint.h"
#include "port_photo.h"
#include "port_shader_toast.h"
#include "port_update_toast.h"

#include <algorithm>
#include <chrono>
#include <mutex>

namespace PortNoteToast {
namespace {
using Clock = std::chrono::steady_clock;
constexpr float kShowSecs = 2.5f;
constexpr float kFadeSecs = 0.4f;
// A note whose text never loads is dropped after this long unseen.
constexpr float kLoadSecs = 10.f;

std::mutex sMutex;
std::string sNext;
bool sNew = false;
// Not a unique_ptr (see port_game_ui.h: one deleted while the program exits hangs it); freed once
// the note is done.
PortGameUi::Text* sText = nullptr;
// Timed from the first frame the note can be seen (its frame and font take a frame or two to
// load); until then from when it was taken, for kLoadSecs.
bool sShowing = false;
Clock::time_point sShownAt;
} // namespace

void Show(const std::string& text) {
  // The game's text reads "&...;" as a tag and "&&" as the character: a note is plain words (a
  // custom action's own, a room's name).
  std::string plain;
  plain.reserve(text.size());
  for (const char c : text) {
    plain += c;
    if (c == '&') {
      plain += '&';
    }
  }
  std::lock_guard< std::mutex > lock(sMutex);
  sNext = std::move(plain);
  sNew = true;
}

void Draw() {
  const Clock::time_point now = Clock::now();
  {
    std::lock_guard< std::mutex > lock(sMutex);
    if (sNew) {
      sNew = false;
      sShowing = false;
      sShownAt = now;
      if (sNext.empty()) {
        delete sText;
        sText = nullptr;
      } else if (sText == nullptr) {
        sText = new PortGameUi::Text(PortGameUi::TextSpec{.text = sNext, .scale = 0.55f});
      } else {
        sText->SetText(sNext);
      }
    }
  }
  if (sText == nullptr) {
    return;
  }
  const float age = std::chrono::duration< float >(now - sShownAt).count();
  if (age > (sShowing ? kShowSecs : kLoadSecs)) {
    delete sText;
    sText = nullptr;
    return;
  }
  // Below the shader compile's count and the update toast while they are up, and below the launch
  // hint where the two would meet (a narrow screen).
  float top = std::max(0.025f, PortShaderToast::Bottom() + 0.01f);
  if (const PortGameUi::ScreenRect update = PortUpdateToast::LastRect(); PortUpdateToast::Showing() && update.valid) {
    top = std::max(top, update.y1 + 0.01f);
  }
  const PortGameUi::ScreenRect hint = PortMenuHint::LastRect(), self = sText->LastRect();
  if (hint.valid && self.valid && hint.x1 + 0.01f > self.x0) {
    top = std::max(top, hint.y1 + 0.01f);
  }
  sText->SetPlacement({PortGameUi::Anchor::Top, 0.f, top});
  // Not in a photo (the overlay's toasts hide for one too); its time still runs.
  const float fade = sShowing ? std::clamp((kShowSecs - age) / kFadeSecs, 0.f, 1.f) : 1.f;
  sText->SetAlpha(PortPhoto::Capturing() ? 0.f : fade);
  sText->Draw();
  if (!sShowing && sText->Ready()) {
    sShowing = true;
    sShownAt = Clock::now();
  }
}

float Bottom() {
  if (sText == nullptr || !sShowing) {
    return 0.f;
  }
  const PortGameUi::ScreenRect rect = sText->LastRect();
  return rect.valid ? rect.y1 : 0.f;
}
} // namespace PortNoteToast
