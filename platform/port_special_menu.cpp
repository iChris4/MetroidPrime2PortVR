// The special menu, pages drawn by the game's GUI (include/port_special_menu.h).
#include "port_special_menu.h"

#include "port_cheats.h"
#include "port_game_ui_page.h"
#include "port_input_remap_ui.h"
#include "port_note_toast.h"
#include "port_overlay.h"
#include "port_play_options.h"
#include "port_save.h"
#include "port_savestate.h"
#include "port_warp.h"

#include "MetroidPrime/PortSkipFrontEnd.hpp"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace PortSpecialMenu {
namespace {
using PortGameUi::Anchor;
using PortGameUi::Button;

constexpr int kSlots = PortSaveState::kSlotCount;
constexpr int kCardSlots = 3;
constexpr const char* kDim = "&main-color=#8A8A8AFF;";
// The quick slot's number, in the HUD's blue.
constexpr const char* kQuick = "&main-color=#5FC8FFFF;";
// Frames a save may take to answer (it is run by stepping the held game, one tick a frame).
constexpr int kSaveWait = 600;
// The list, in screen heights: no wider than this however wide the screen (a row's time stays near
// its name), and its text's size.
constexpr float kListMaxWidth = 1.6f;
constexpr float kListScale = 0.7f;
// The pages of rows (Game, Cheats, Items, Warp): a name and its value, so narrower; and how many
// rows show at once (a longer page scrolls), as many as fit at 16:10 above the info line.
constexpr float kRowListWidth = 1.2f;
constexpr int kRowsShown = 9;
// The info line's middle above the content's bottom, and its size.
constexpr float kInfoUp = 0.03f;
constexpr float kInfoScale = 0.55f;

// The pages, in the order left and right turn through them: the port's save states, the memory
// card's three saves (load only), then the pages of rows: the play options, the cheats, the items
// (to give or take) and the warps.
enum Tab { kStates, kCard, kGame, kCheats, kItems, kWarp, kTabs };
const char* const kTabNames[kTabs] = {"Save states", "Memory card", "Game", "Cheats", "Items", "Warp"};
bool RowPage(int tab) { return tab >= kGame; }

enum ActionId { kLoad, kSave, kQuickSlot, kRowAction, kBack };

// A row of a page of rows: a switch (get/set: its value On or Off at the row's right) or an action
// (run). Made from port_menu_entry.h's entries, or for an item, from its type.
struct Row {
  std::string name, help;
  std::function< bool() > get;
  std::function< void(bool) > set;
  const char* onVerb = "Turn on";
  const char* offVerb = "Turn off";
  std::function< bool() > run;
  const char* verb = "";
  const char* done = nullptr;
  const char* confirm = nullptr;
  bool closes = false;
  // Held off (a switch shown Off, dim), and why.
  std::function< bool() > blocked;
  const char* blockedWhy = "Off while Original experience is on (F1 > Game)";
  // Not to be used now (shown dim), and why.
  std::function< bool() > available;
  const char* unavailableWhy = "Only while playing";

  bool IsSwitch() const { return static_cast< bool >(get); }
  bool Blocked() const { return blocked && blocked(); }
  bool Available() const { return !available || available(); }
};

Row SwitchRow(const PortMenuEntry::Switch& entry) {
  Row row{.name = entry.name, .help = entry.help, .get = entry.get, .set = entry.set};
  row.blocked = [&entry] { return PortMenuEntry::Blocked(entry); };
  return row;
}

Row ActionRow(const PortMenuEntry::Action& entry) {
  Row row{.name = entry.name,
          .help = entry.help,
          .run = entry.run,
          .verb = entry.verb,
          .done = entry.done,
          .confirm = entry.confirm,
          .closes = entry.closes};
  row.available = [&entry] { return PortMenuEntry::Available(entry); };
  row.unavailableWhy = entry.unavailable;
  return row;
}

// An upgrade or key, on while Samus has it: on gives it, off takes it (as F1's checkboxes do).
Row ItemRow(const PortCheats::Item& item) {
  const CPlayerState::EItemType type = item.type;
  Row row{.name = item.name, .help = "On while you have it: give it or take it away"};
  row.get = [type] {
    const CPlayerState* ps = PortCheats::PlayerInGame();
    return ps != nullptr && ps->HasPowerUp(type);
  };
  row.set = [type](bool on) {
    if (CPlayerState* ps = PortCheats::PlayerInGame()) {
      PortCheats::GrantItem(*ps, type, on ? 1 : 0, on ? 1 : 0);
    }
  };
  row.onVerb = "Give";
  row.offVerb = "Take";
  row.available = [] { return PortCheats::PlayerInGame() != nullptr; };
  return row;
}

// The Cheats page's switches: the ones worth flipping mid-game (F1 has them all).
constexpr PortCheats::Cheat kMenuCheats[] = {
    PortCheats::Cheat::GodMode,  PortCheats::Cheat::NoDarkAetherDamage, PortCheats::Cheat::InfiniteAmmo,
    PortCheats::Cheat::MoonJump, PortCheats::Cheat::UnlockBeamDoors,    PortCheats::Cheat::BoostAnywhere,
    PortCheats::Cheat::PhazonShot,
};

std::vector< Row > MakeRows(Tab tab) {
  std::vector< Row > rows;
  switch (tab) {
  case kGame:
    for (const PortMenuEntry::Switch& entry : PortPlayOptions::Switches()) {
      rows.push_back(SwitchRow(entry));
    }
    for (const PortMenuEntry::Action& entry : PortPlayOptions::Actions()) {
      rows.push_back(ActionRow(entry));
    }
    break;
  case kCheats:
    for (const PortCheats::Cheat cheat : kMenuCheats) {
      rows.push_back(SwitchRow(PortCheats::Get(cheat)));
    }
    for (const PortMenuEntry::Action& entry : PortCheats::Actions()) {
      rows.push_back(ActionRow(entry));
    }
    break;
  case kItems:
    for (const PortCheats::Item& item : PortCheats::Abilities()) {
      rows.push_back(ItemRow(item));
    }
    for (const PortCheats::Item& item : PortCheats::Keys()) {
      rows.push_back(ItemRow(item));
    }
    break;
  case kWarp:
    for (const PortMenuEntry::Action& entry : PortCheats::Warps()) {
      rows.push_back(ActionRow(entry));
    }
    break;
  default:
    break;
  }
  return rows;
}

// What the question on screen will do when answered Yes.
enum Asking { kAskNone, kAskOverwrite, kAskCardLoad, kAskRow };

std::string Where(const PortSaveState::Info& info) {
  const std::string room = PortOverlay::RoomName(info.worldId, info.areaIndex);
  return (room.empty() ? info.room : room) + (info.morphed ? " (ball)" : "");
}

std::string Clock(double seconds) {
  const int total = static_cast< int >(seconds);
  char text[32];
  std::snprintf(text, sizeof(text), "%d:%02d:%02d", total / 3600, total / 60 % 60, total % 60);
  return text;
}

std::string PlayTime(const PortSaveState::Info& info) { return info.exists ? Clock(info.playTime) : "--:--"; }

class Menu : public PortGameUi::Page {
public:
  // The binding's press (any thread); Frame acts on it.
  void RequestToggle(bool inGame) {
    std::lock_guard< std::mutex > lock(mMutex);
    mToggle = true;
    mToggleInGame = inGame;
  }

  void Frame() {
    bool toggle, inGame;
    {
      std::lock_guard< std::mutex > lock(mMutex);
      toggle = mToggle;
      inGame = mToggleInGame;
      mToggle = false;
    }
    if (toggle) {
      if (IsOpen()) {
        Close();
      } else if (inGame) {
        Open();
      }
    }
    WatchDimension();
    Draw();
  }

  std::string Describe() const {
    if (!IsOpen()) {
      return "closed";
    }
    if (RowPage(mTab)) {
      const Row& row = mRows[mTab][Selected()];
      std::string text = std::string(kTabNames[mTab]) + ", " + row.name;
      if (row.IsSwitch()) {
        text += row.get() ? ": on" : ": off";
      }
      if (row.Blocked()) {
        text += " (Original experience)";
      } else if (!row.Available()) {
        text += " (unavailable)";
      }
      if (mQuestion != nullptr) {
        text += ", confirm? choice " + std::to_string(mQuestion->Selected());
      }
      return mStatus.empty() ? text : text + ", status: " + mStatus;
    }
    std::string text = std::string(kTabNames[mTab]) + ", slot " + std::to_string(Selected() + 1);
    if (mTab == kStates) {
      text += ", quick slot " + std::to_string(PortSaveState::SelectedSlot());
    }
    if (mQuestion != nullptr) {
      text += std::string(mAsking == kAskOverwrite ? ", overwrite?" : ", load card save?") + " choice " +
              std::to_string(mQuestion->Selected());
    }
    if (!mStatus.empty()) {
      text += ", status: " + mStatus;
    }
    return text;
  }

protected:
  void OnOpen() override {
    if (mRows[kGame].empty()) {
      for (int tab = 0; tab < kTabs; ++tab) {
        mRows[tab] = MakeRows(static_cast< Tab >(tab));
      }
    }
    Refresh();
    mStatus.clear();
    mLastMessage = PortSaveState::LastMessage();
    mSavePending = 0;
    mSettling = false;
    MakeElements();
    mList[kStates]->SetSelected(std::clamp(PortSaveState::SelectedSlot() - 1, 0, kSlots - 1));
  }

  void OnClose() override {
    // A page of rows opens again on the row it was left on (the page itself is kept as is, mTab).
    for (int tab = kGame; tab < kTabs; ++tab) {
      if (mList[tab] != nullptr) {
        mRowSelected[tab] = mList[tab]->Selected();
      }
    }
    // Freed: made again when it next opens.
    delete mFrame;
    delete mInfo;
    for (PortGameUi::List*& list : mList) {
      delete list;
      list = nullptr;
    }
    delete mQuestion;
    mFrame = nullptr;
    mInfo = nullptr;
    mQuestion = nullptr;
    mAsking = kAskNone;
  }

  void Update(const PortGameUi::PageInput& input) override {
    StepSave();
    StepSettle();
    if (mQuestion != nullptr) {
      const int answer = mAsk.Handle(*mQuestion, input);
      if (answer >= 0) {
        const Asking asking = mAsking;
        delete mQuestion;
        mQuestion = nullptr;
        mAsking = kAskNone;
        if (answer == 0) {
          if (asking == kAskOverwrite) {
            Save();
          } else if (asking == kAskCardLoad) {
            LoadCard();
          } else {
            DoRow(mAskRow, true);
          }
        }
      }
      return;
    }
    // Left and right turn through the pages; a click on a page's tab turns to it.
    const int clickedTab = mFrame->TabClicked(input);
    if (input.horizontal != 0 || clickedTab >= 0) {
      mTab = clickedTab >= 0 ? static_cast< Tab >(clickedTab)
                             : static_cast< Tab >((mTab + (input.horizontal < 0 ? kTabs - 1 : 1)) % kTabs);
      SetTab();
      return;
    }
    const int before = Selected();
    const int picked = mList[mTab]->Handle(input);
    if (RowPage(mTab)) {
      if (Selected() != before) {
        mStatus.clear();
      }
      // A click on a row does it, as A does.
      if (picked >= 0) {
        DoRow(picked);
        return;
      }
    }
    UpdateActions();
    const int action = mFrame->Handle(input);
    if (action >= 0) {
      Act(mActionIds[action]);
    } else if (input.back || input.Pressed(Button::Start)) {
      Close();
    }
  }

  void Render(float alpha) override {
    if (mQuestion != nullptr) {
      mQuestion->SetAlpha(alpha);
      mQuestion->Draw();
      return;
    }
    UpdateActions();
    mFrame->Draw(alpha);
    if (mTab == kStates) {
      for (int i = 0; i < kSlots; ++i) {
        const std::string number = std::to_string(i + 1);
        mList[kStates]->SetCell(i, 0, i + 1 == PortSaveState::SelectedSlot() ? std::string(kQuick) + number : number);
        mList[kStates]->SetCell(i, 1, mSlot[i].where);
        mList[kStates]->SetCell(i, 2, PlayTime(mSlot[i].info));
      }
    } else if (RowPage(mTab)) {
      const std::vector< Row >& rows = mRows[mTab];
      for (int i = 0; i < static_cast< int >(rows.size()); ++i) {
        const Row& row = rows[i];
        const bool usable = !row.Blocked() && row.Available();
        mList[mTab]->SetCell(i, 0, usable ? row.name : std::string(kDim) + row.name);
        std::string value;
        if (row.IsSwitch()) {
          const bool on = !row.Blocked() && row.get();
          value = std::string(!usable ? kDim : on ? kQuick : "") + (on ? "On" : "Off");
        }
        mList[mTab]->SetCell(i, 1, value);
      }
    } else {
      for (int i = 0; i < kCardSlots; ++i) {
        mList[kCard]->SetCell(i, 0, std::to_string(i + 1));
        mList[kCard]->SetCell(i, 1, mCard[i].where);
        mList[kCard]->SetCell(i, 2, mCard[i].percent);
        mList[kCard]->SetCell(i, 3, mCard[i].time);
      }
    }
    mList[mTab]->Draw(alpha);
    mInfo->SetText(InfoLine());
    mInfo->SetAlpha(alpha);
    mInfo->Draw();
  }

private:
  struct Slot {
    PortSaveState::Info info;
    std::string where;
  };
  struct CardSlot {
    bool present = false;
    std::string where, percent, time;
  };

  int Selected() const { return mList[mTab] != nullptr ? mList[mTab]->Selected() : 0; }

  // The page shown: its name in the title and lit among the tabs.
  void SetTab() {
    mFrame->SetTitle(kTabNames[mTab]);
    mFrame->SetTabs(std::vector< std::string >(kTabNames, kTabNames + kTabs), mTab);
    mStatus.clear();
  }

  void MakeElements() {
    // The kit's full-screen page: the map's frame, the title in its label, the actions in its bar.
    mFrame = new PortGameUi::PageFrame(kTabNames[mTab]);
    SetTab();
    const PortGameUi::PageArea area = mFrame->Content();
    const float width = std::min(area.Width(), kListMaxWidth);
    // The rows in the middle of the content, above the info line.
    const float listY = 0.5f * (area.top + area.bottom - 2.f * kInfoUp);
    mList[kStates] = new PortGameUi::List(PortGameUi::ListSpec{
        .rows = kSlots,
        .columns = {{Anchor::Left, 0.07f}, {Anchor::Left, 0.17f}, {Anchor::Right, 0.07f}},
        .width = width,
        .centreY = listY,
        .scale = kListScale,
    });
    mList[kCard] = new PortGameUi::List(PortGameUi::ListSpec{
        .rows = kCardSlots,
        .columns = {{Anchor::Left, 0.07f}, {Anchor::Left, 0.17f}, {Anchor::Right, 0.36f}, {Anchor::Right, 0.07f}},
        .width = width,
        .centreY = listY,
        .scale = kListScale,
    });
    for (int tab = kGame; tab < kTabs; ++tab) {
      mList[tab] = new PortGameUi::List(PortGameUi::ListSpec{
          .rows = static_cast< int >(mRows[tab].size()),
          .columns = {{Anchor::Left, 0.07f}, {Anchor::Right, 0.07f}},
          .width = std::min(width, kRowListWidth),
          .centreY = listY,
          .scale = kListScale,
          .visibleRows = kRowsShown,
      });
      mList[tab]->SetSelected(mRowSelected[tab]);
    }
    mList[kCard]->SetSelected(mCardIdx >= 0 && mCardIdx < kCardSlots ? mCardIdx : 0);
    mInfo = new PortGameUi::Text(
        PortGameUi::TextSpec{.scale = kInfoScale, .placement = {Anchor::Centre, 0.f, area.bottom - kInfoUp}});
  }

  void Refresh() {
    for (int i = 0; i < kSlots; ++i) {
      mSlot[i].info = PortSaveState::SlotInfo(i + 1);
      mSlot[i].where = mSlot[i].info.exists ? Where(mSlot[i].info) : "Empty";
    }
    // The card's file, read from disk as the file select would show it (the card is not mounted).
    mCardFile = false;
    mCardIdx = -1;
    for (CardSlot& slot : mCard) {
      slot = CardSlot{false, "Empty", "", "--:--"};
    }
    const std::vector< std::string > files = PortSave::Files();
    PortSave::File file;
    std::string error;
    if (files.empty() || !PortSave::Inspect(files.front(), file, error)) {
      return;
    }
    mCardFile = true;
    mCardIdx = file.saveIdx;
    for (int i = 0; i < kCardSlots; ++i) {
      const PortSave::Slot& s = file.slots[i];
      if (!s.present) {
        continue;
      }
      const std::string world = PortOverlay::WorldName(s.worldId);
      mCard[i].present = true;
      mCard[i].where = (world.empty() ? "Unknown world" : world) + (s.hardMode ? " (Hard)" : "");
      mCard[i].percent = std::to_string(s.itemPercent) + "%";
      mCard[i].time = Clock(s.playTime);
    }
  }

  void UpdateActions() {
    std::vector< PortGameUi::PageAction > actions;
    mActionIds.clear();
    const auto add = [&](ActionId id, Button button, const char* label, bool enabled) {
      actions.push_back({button, label, enabled});
      mActionIds.push_back(id);
    };
    if (mTab == kStates) {
      add(kLoad, Button::A, "Load", mSlot[Selected()].info.exists && !mSettling);
      add(kSave, Button::Y, "Save", !mSettling);
      add(kQuickSlot, Button::X, "Quick slot", true);
    } else if (mTab == kCard) {
      add(kLoad, Button::A, "Load", mCard[Selected()].present && !mSettling);
    } else {
      const Row& row = mRows[mTab][Selected()];
      const char* label = !row.IsSwitch() ? row.verb : row.get() ? row.offVerb : row.onVerb;
      add(kRowAction, Button::A, label, !row.Blocked() && row.Available());
    }
    add(kBack, Button::B, "Back", true);
    mFrame->SetActions(actions);
  }

  // Above the actions: the last result, else what the page's rows are.
  std::string InfoLine() const {
    if (!mStatus.empty()) {
      return std::string(kDim) + mStatus;
    }
    std::string line = kDim;
    if (RowPage(mTab)) {
      const Row& row = mRows[mTab][Selected()];
      line += row.Blocked() ? row.blockedWhy : !row.Available() ? row.unavailableWhy : row.help;
    } else if (mTab == kCard) {
      line += mCardFile ? "Loading quits to the title and starts that save" : "The memory card has no save yet";
    } else {
      const std::string save = PortInputRemap::BindingsText(PortInput::Action::SaveState, " / ");
      const std::string load = PortInputRemap::BindingsText(PortInput::Action::LoadState, " / ");
      line += "Quick slot " + std::string(kQuick) + std::to_string(PortSaveState::SelectedSlot()) + kDim;
      if (!save.empty() || !load.empty()) {
        line += ": " + (save.empty() ? std::string() : save + " saves") + (!save.empty() && !load.empty() ? ", " : "") +
                (load.empty() ? std::string() : load + " loads");
      }
    }
    return line;
  }

  void Act(ActionId action) {
    const int slot = Selected() + 1;
    switch (action) {
    case kLoad:
      if (mSettling) {
        return;
      }
      if (mTab == kCard) {
        // Quitting loses the progress since the last save: ask first, No highlighted.
        Ask(kAskCardLoad, "Load memory card save " + std::to_string(slot) + "?\n&push;" + kDim + mCard[Selected()].where +
                              "  " + mCard[Selected()].time + "\nProgress since your last save is lost&pop;");
        return;
      }
      PortSaveState::RequestLoad(slot, true);
      Close();
      return;
    case kSave:
      if (mSettling) {
        return;
      }
      if (!mSlot[Selected()].info.exists) {
        Save();
        return;
      }
      // Overwriting a state asks first, No highlighted.
      Ask(kAskOverwrite, "Overwrite slot " + std::to_string(slot) + "?\n&push;" + kDim + mSlot[Selected()].where +
                             "  " + PlayTime(mSlot[Selected()].info) + "&pop;");
      return;
    case kQuickSlot:
      PortSaveState::SetSelectedSlot(slot);
      mStatus.clear();
      return;
    case kRowAction:
      DoRow(Selected());
      return;
    default:
      Close();
      return;
    }
  }

  // A row's switch flipped, or its action done (after its question, if it asks one): what it did
  // is said under the list, or the menu closes for the game to do it.
  void DoRow(int index, bool confirmed = false) {
    const Row& row = mRows[mTab][index];
    if (row.Blocked() || !row.Available()) {
      return;
    }
    if (row.IsSwitch()) {
      const bool on = !row.get();
      row.set(on);
      mStatus.clear();
      std::fprintf(stderr, "[specialmenu] %s %s\n", row.name.c_str(), on ? "on" : "off");
      return;
    }
    if (row.confirm != nullptr && !confirmed) {
      mAskRow = index;
      Ask(kAskRow, row.confirm);
      return;
    }
    const bool done = row.run();
    std::fprintf(stderr, "[specialmenu] %s%s\n", row.name.c_str(), done ? "" : ": not now");
    if (done && row.closes) {
      // A warp to the room's twin says a frame or so later whether there was one.
      mDimensionWait = mTab == kWarp ? kDimensionWait : 0;
      Close();
      return;
    }
    mStatus = done ? (row.done != nullptr ? row.done : "") : row.unavailableWhy;
  }

  // After a warp from the Warp page: no twin room in the other dimension is said in a note.
  void WatchDimension() {
    if (mDimensionWait <= 0) {
      return;
    }
    --mDimensionWait;
    if (const int result = PortWarp::TakeDimensionResult(); result != 0) {
      mDimensionWait = 0;
      if (result < 0) {
        PortNoteToast::Show("No twin room in the other dimension");
      }
    }
  }

  void Ask(Asking asking, const std::string& title) {
    mAsking = asking;
    mQuestion = new PortGameUi::Question(PortGameUi::QuestionSpec{.title = title, .selected = 1, .scale = 0.8f});
  }

  void Save() {
    const int slot = Selected() + 1;
    PortSaveState::RequestSave(slot);
    mStatus = "Saving to slot " + std::to_string(slot) + "...";
    mSavePending = kSaveWait;
  }

  // The card's save, as the file select starts one: the game quits to the title, and on the way
  // back the front-end skip reads the slot and starts it (armed as the game quits:
  // PortSkipFrontEnd::ArmQueuedCardLoad).
  void LoadCard() {
    const int fileIdx = Selected();
    PortSkipFrontEnd::QueueCardLoad(fileIdx);
    PortOverlay::RequestReturnToTitle();
    std::fprintf(stderr, "[specialmenu] loading memory card save %d\n", fileIdx + 1);
    Close();
  }

  // A save runs in the game's tick, which the page holds: step it until the result comes back.
  void StepSave() {
    if (mSavePending <= 0) {
      return;
    }
    const std::string message = PortSaveState::LastMessage();
    if (message != mLastMessage) {
      mLastMessage = message;
      mStatus = message;
      mSavePending = 0;
      Refresh();
      return;
    }
    if (--mSavePending == 0) {
      mStatus = "No answer from the save";
      return;
    }
    StepGame(1);
  }

  // A load (or warp) still settling when the page opened would wait for the game, which the page
  // holds, and refuse a save or load meanwhile: let the game run under the dark until it is done.
  void StepSettle() {
    if (mSavePending > 0) {
      return;
    }
    if (PortSaveState::Busy()) {
      if (!mSettling) {
        mSettling = true;
        mStatus = "Waiting for the last load to finish...";
      }
      StepGame(1);
    } else if (mSettling) {
      mSettling = false;
      mStatus.clear();
      mLastMessage = PortSaveState::LastMessage();
      Refresh();
    }
  }

  std::mutex mMutex;
  bool mToggle = false;
  bool mToggleInGame = false;
  // The page shown, kept from one opening to the next for the session (a new one starts on the
  // save states), and each page of rows' selected row.
  Tab mTab = kStates;
  int mRowSelected[kTabs] = {};
  Slot mSlot[kSlots];
  CardSlot mCard[kCardSlots];
  bool mCardFile = false;
  int mCardIdx = -1; // the card slot the file select opens on
  std::string mStatus;
  std::string mLastMessage;
  int mSavePending = 0;
  bool mSettling = false;
  std::vector< ActionId > mActionIds;
  Asking mAsking = kAskNone;
  int mAskRow = 0; // the row whose question is up (kAskRow)
  std::vector< Row > mRows[kTabs]; // the pages of rows' (made at the first open)
  // Frames left to hear whether a warp found a twin room.
  static constexpr int kDimensionWait = 120;
  int mDimensionWait = 0;
  // Not unique_ptrs (see port_game_ui.h: one deleted while the program exits hangs it).
  PortGameUi::PageFrame* mFrame = nullptr;
  PortGameUi::Text* mInfo = nullptr;
  PortGameUi::List* mList[kTabs] = {};
  PortGameUi::Question* mQuestion = nullptr;
  PortGameUi::QuestionInput mAsk;
};

// Never deleted, as the kit's elements it holds must not be at exit.
Menu& TheMenu() {
  static Menu* sMenu = new Menu();
  return *sMenu;
}
} // namespace

void Toggle(bool inGame) { TheMenu().RequestToggle(inGame); }

bool Open() { return TheMenu().IsOpen(); }

void Draw() { TheMenu().Frame(); }

bool Press(const std::string& what) {
  if (what == "open" || what == "close") {
    if ((what == "open") != Open()) {
      Toggle(true);
    }
    return true;
  }
  return PortGameUi::ScriptPagePress(what);
}

std::string Describe() { return TheMenu().Describe(); }
} // namespace PortSpecialMenu
