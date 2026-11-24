// The boot's resume prompt as the game's Yes/No box (include/port_resume_prompt.h).
#include "port_resume_prompt.h"

#include "port_savestate.h"

#include <atomic>
#include <ctime>
#include <mutex>
#include <string>

namespace PortResumePrompt {
namespace {
// Not a unique_ptr (see port_game_ui.h: one deleted while the program exits hangs it); freed once
// the prompt is answered.
PortGameUi::Question* sQuestion = nullptr;
std::atomic< bool > sUnavailable{false};
// Read by the overlay's pointer handling.
std::mutex sRectMutex;
PortGameUi::ScreenRect sRects[2];

// The question, then where and when it was saved, dimmer.
std::string Title(const PortSaveState::ResumeInfo& info) {
  std::string title = info.crashedAgain       ? "The game closed unexpectedly again\nafter resuming here. Resume anyway?"
                      : info.crashedAfterThis ? "The game closed unexpectedly.\nResume where you left off?"
                                              : "Resume where you left off?";
  char when[32] = "";
  const std::time_t t = static_cast< std::time_t >(info.savedAt);
  std::tm local{};
#if defined(_WIN32)
  if (localtime_s(&local, &t) == 0) {
#else
  if (localtime_r(&t, &local) != nullptr) {
#endif
    std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &local);
  }
  title += "\n&push;&main-color=#A0A0A0FF;" + info.room;
  if (when[0] != '\0') {
    title += "\nSaved " + std::string(when);
  }
  return title + "&pop;";
}

void SetRects(const PortGameUi::ScreenRect& yes, const PortGameUi::ScreenRect& no) {
  std::lock_guard< std::mutex > lock(sRectMutex);
  sRects[0] = yes;
  sRects[1] = no;
}
} // namespace

void Draw() {
  PortSaveState::ResumeInfo info;
  bool yes = true;
  if (!PortSaveState::ResumePromptShown(info, yes)) {
    SetRects({}, {});
    delete sQuestion;
    sQuestion = nullptr;
    return;
  }
  if (sQuestion == nullptr) {
    sQuestion = new PortGameUi::Question(PortGameUi::QuestionSpec{
        .title = Title(info),
        .selected = yes ? 0 : 1,
        .scale = 0.85f,
    });
    sQuestion->SetAlpha(1.f);
  }
  if (sQuestion->Failed()) {
    sUnavailable.store(true);
    SetRects({}, {});
    return;
  }
  sQuestion->SetSelected(yes ? 0 : 1);
  sQuestion->Draw();
  SetRects(sQuestion->ChoiceRect(0), sQuestion->ChoiceRect(1));
}

PortGameUi::ScreenRect ChoiceRect(int choice) {
  std::lock_guard< std::mutex > lock(sRectMutex);
  return sRects[choice == 0 ? 0 : 1];
}

bool Unavailable() { return sUnavailable.load(); }
} // namespace PortResumePrompt
