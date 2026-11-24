// Pages over the game, and the pieces they are made of (include/port_game_ui_page.h).
#include "port_game_ui_page.h"

#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Graphics/CTexture.hpp"
#include "Kyoto/TToken.hpp"
#include "MetroidPrime/Cameras/CCameraBlurPass.hpp"
#include "MetroidPrime/Cameras/CCameraFilterPass.hpp"
#include "MetroidPrime/Tweaks/CTweakGui.hpp"
#include "MetroidPrime/Tweaks/CTweakGuiColors.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayerRes.hpp"

#include "port_game_ui_math.h"
#include "port_mouse.h"
#include "port_overlay.h"

#include <SDL3/SDL.h>
#include <dolphin/pad.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>

namespace PortGameUi {
namespace {
using Clock = std::chrono::steady_clock;

constexpr const char* kLit = "&main-color=#FFFFFFFF;";
constexpr const char* kDim = "&main-color=#8A8A8AFF;";
constexpr const char* kOff = "&main-color=#505050FF;"; // an action that can't be done now
// A scrolling list's thumb (the font has no arrows): its width and its middle in from the list's
// right edge (past the selection bar's end and a right column's text), screen heights.
constexpr float kThumbWidth = 0.03f;
constexpr float kThumbIn = 0.025f;
// The pause screen's fade (CPauseScreenBlur: half a second in); the page fades in over it.
constexpr float kDimIn = 0.5f;
constexpr float kDimOut = 0.25f;
constexpr float kPageFrom = 0.2f;
constexpr float kPageFade = 0.3f;

float Seconds(Clock::time_point from) { return std::chrono::duration< float >(Clock::now() - from).count(); }

float ScreenAspect() {
  const CViewport& vp = CGraphics::GetViewport();
  return vp.mHeight > 0 ? static_cast< float >(vp.mWidth) / static_cast< float >(vp.mHeight) : 16.f / 9.f;
}

bool Inside(const ScreenRect& r, float x, float y, float pad) {
  return r.valid && x >= r.x0 - pad && x <= r.x1 + pad && y >= r.y0 - pad && y <= r.y1 + pad;
}

template < typename T >
void Free(T*& element) {
  delete element;
  element = nullptr;
}

// --- Input -----------------------------------------------------------------------------------------

// Shared with the input threads.
std::mutex sInputMutex;
uint16_t sPadButtons = 0;
float sStickX = 0.f, sStickY = 0.f;
std::string sScripted;
std::vector< PageInput::Click > sClicks;
float sWheel = 0.f; // steps, a trackpad's fractions adding up
bool sRightDown = false; // a right button press since the last frame
std::atomic< bool > sAnyOpen{false};

// The game thread's.
uint16_t sPrevButtons = 0;
PortGameUiMath::Repeat sRepeat;
float sPrevX = -1.f, sPrevY = -1.f;

bool SDLCALL ClickWatch(void*, SDL_Event* event) {
  if (event->type == SDL_EVENT_MOUSE_WHEEL) {
    if (sAnyOpen.load()) {
      std::lock_guard< std::mutex > lock(sInputMutex);
      sWheel += event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event->wheel.y : event->wheel.y;
    }
    return true;
  }
  if (event->type != SDL_EVENT_MOUSE_BUTTON_DOWN && event->type != SDL_EVENT_MOUSE_BUTTON_UP) {
    return true;
  }
  const SDL_MouseButtonEvent& b = event->button;
  SDL_Window* window = SDL_GetWindowFromID(b.windowID);
  int w = 0, h = 0;
  if (!sAnyOpen.load() || window == nullptr || !SDL_GetWindowSize(window, &w, &h) || w <= 0 || h <= 0) {
    return true;
  }
  std::lock_guard< std::mutex > lock(sInputMutex);
  if (b.button == SDL_BUTTON_RIGHT && b.down) {
    sRightDown = true;
  } else if (b.button == SDL_BUTTON_LEFT && sClicks.size() < 32) {
    sClicks.push_back({b.down, b.x / static_cast< float >(w), b.y / static_cast< float >(h)});
  }
  return true;
}

// A new page: what is already held (the binding that opened it) is not a press.
void ResetInput() {
  static bool sWatching = false;
  if (!sWatching) {
    sWatching = SDL_AddEventWatch(ClickWatch, nullptr);
  }
  std::lock_guard< std::mutex > lock(sInputMutex);
  sClicks.clear();
  sWheel = 0.f;
  sScripted.clear();
  sRightDown = false;
  sPrevButtons = 0xffff;
  sRepeat = PortGameUiMath::Repeat{};
  sRepeat.held = 99; // a direction already held waits for its release
}

PageInput TakeInput() {
  PageInput in;
  uint16_t buttons;
  float stickX, stickY;
  std::string scripted;
  bool right;
  {
    std::lock_guard< std::mutex > lock(sInputMutex);
    buttons = sPadButtons;
    stickX = sStickX;
    stickY = sStickY;
    scripted.swap(sScripted);
    in.clicks.swap(sClicks);
    in.wheel = static_cast< int >(sWheel);
    sWheel -= static_cast< float >(in.wheel);
    right = sRightDown;
    sRightDown = false;
  }
  const uint16_t pressed = buttons & ~sPrevButtons;
  sPrevButtons = buttons;
  static constexpr uint16_t kBits[static_cast< int >(Button::Count)] = {PAD_BUTTON_A, PAD_BUTTON_B, PAD_BUTTON_X,
                                                                        PAD_BUTTON_Y, PAD_BUTTON_START};
  static constexpr const char* kNames[static_cast< int >(Button::Count)] = {"a", "b", "x", "y", "start"};
  for (int i = 0; i < static_cast< int >(Button::Count); ++i) {
    in.pressed[i] = (pressed & kBits[i]) != 0 || scripted == kNames[i];
  }
  // One direction: the D-pad or the stick; up 1, down 2, left 3, right 4.
  int dir = 0;
  if ((buttons & PAD_BUTTON_UP) || stickY > 0.5f) {
    dir = 1;
  } else if ((buttons & PAD_BUTTON_DOWN) || stickY < -0.5f) {
    dir = 2;
  } else if ((buttons & PAD_BUTTON_LEFT) || stickX < -0.5f) {
    dir = 3;
  } else if ((buttons & PAD_BUTTON_RIGHT) || stickX > 0.5f) {
    dir = 4;
  }
  const double now = std::chrono::duration< double >(Clock::now().time_since_epoch()).count();
  if (sRepeat.held == 99 && dir == 0) {
    sRepeat.held = 0;
  }
  bool step = sRepeat.held != 99 && sRepeat.Step(dir, now);
  if (scripted == "up" || scripted == "down" || scripted == "left" || scripted == "right") {
    dir = scripted == "up" ? 1 : scripted == "down" ? 2 : scripted == "left" ? 3 : 4;
    step = true;
  }
  if (step) {
    in.vertical = dir == 1 ? -1 : dir == 2 ? 1 : 0;
    in.horizontal = dir == 3 ? -1 : dir == 4 ? 1 : 0;
  }
  in.back = in.Pressed(Button::B) || right;
  // The pointer, over the window it is in.
  SDL_Window* window = SDL_GetMouseFocus();
  int w = 0, h = 0;
  if (window != nullptr && SDL_GetWindowSize(window, &w, &h) && w > 0 && h > 0) {
    float px = 0.f, py = 0.f;
    SDL_GetMouseState(&px, &py);
    in.pointerX = px / static_cast< float >(w);
    in.pointerY = py / static_cast< float >(h);
    in.pointerMoved = in.pointerX != sPrevX || in.pointerY != sPrevY;
    sPrevX = in.pointerX;
    sPrevY = in.pointerY;
  }
  return in;
}

// --- The game behind -------------------------------------------------------------------------------

Page* sCurrent = nullptr;
// Kept for the program's life: freed at exit they would hang, as the kit's elements would.
CCameraBlurPass* sBlur = nullptr;
TLockedToken< CTexture >* sMapLight = nullptr;

// The pause screen's blur (CPauseScreenBlur::Update): its high blur from one copy of the frame (the
// game stands still under it), to the tweak's pause amount as `t` goes 0 to 1.
void BlurGame(float t) {
  if (sBlur == nullptr) {
    sBlur = new CCameraBlurPass();
  }
  const float factor = gpTweakGui.get() != nullptr ? gpTweakGui->GetPauseBlurFactor() : 0.f;
  sBlur->SetBlur(CCameraBlurPass::kBT_HiBlur, t * factor, 0.f, true);
  sBlur->Draw();
}

void StopBlur() {
  if (sBlur != nullptr) {
    sBlur->SetBlur(CCameraBlurPass::kBT_NoBlur, 0.f, 0.f, true);
  }
}

// The pause screen's dark (CPauseScreenBlur::Draw): the map light texture multiplied over each
// quarter in its background colour, and every other line darker, eased in by `t`.
void DimGame(float t) {
  if (t <= 0.f) {
    return;
  }
  if (sMapLight == nullptr && gpSimplePool != nullptr) {
    sMapLight = new TLockedToken< CTexture >(gpSimplePool->GetObj("TXTR_MapLightQuarter"));
  }
  const CTexture* light = sMapLight != nullptr ? **sMapLight : nullptr;
  const float ease = 2.f * t - t * t;
  CGraphics::DisableAllLights();
  CGraphics::SetAmbientColor(CColor(0xffffffff));
  CCameraFilterPass::DrawFilter(
      CCameraFilterPass::kFT_Multiply,
      light != nullptr ? CCameraFilterPass::kFS_FullscreenQuarters : CCameraFilterPass::kFS_Fullscreen,
      CColor::Lerp(CColor::White(), gpTweakGuiColors->GetPauseScreenBGModulateColor(), ease), light, ease);
  CCameraFilterPass::DrawFilter(
      CCameraFilterPass::kFT_Multiply, CCameraFilterPass::kFS_ScanLinesEven,
      CColor::Lerp(CColor::White(), CColor(uchar(0xc0), uchar(0xc0), uchar(0xc0), uchar(0xff)), ease), nullptr,
      ease);
}
} // namespace

std::string Glyph(Button button) {
  if (gpTweakPlayerRes.get() == nullptr) {
    return "";
  }
  const CTweakPlayerRes& res = *gpTweakPlayerRes;
  const rstl::reserved_vector< CAssetId, 2 >& ids = button == Button::A   ? res.mAButton
                                                    : button == Button::B ? res.mBButton
                                                    : button == Button::X ? res.mXButton
                                                    : button == Button::Y ? res.mYButton
                                                                          : res.mStartButton;
  if (ids.size() == 0) {
    return "";
  }
  char tag[32];
  std::snprintf(tag, sizeof(tag), "&image=%8.8X;", ids[0]);
  return tag;
}

void NotePagePad(const PADStatus& pad) {
  std::lock_guard< std::mutex > lock(sInputMutex);
  sPadButtons = pad.button;
  sStickX = static_cast< float >(pad.stickX) / 72.f;
  sStickY = static_cast< float >(pad.stickY) / 72.f;
}

bool ScriptPagePress(const std::string& name) {
  static const char* const kNames[] = {"up", "down", "left", "right", "a", "b", "x", "y", "start"};
  if (std::find_if(std::begin(kNames), std::end(kNames), [&](const char* n) { return name == n; }) ==
      std::end(kNames)) {
    return false;
  }
  std::lock_guard< std::mutex > lock(sInputMutex);
  sScripted = name;
  return true;
}

bool PageOpen() { return sAnyOpen.load(); }

// --- Page ------------------------------------------------------------------------------------------

struct Page::Impl {
  bool open = false;
  bool heldTicks = false; // this page set the tick hold (the console's own is left alone)
  Clock::time_point openedAt;
  Clock::time_point closedAt;
  float dimAtClose = 0.f; // how far the dark had come when it closed: it fades out from there
};

Page::Page() : mImpl(std::make_unique< Impl >()) {}
Page::~Page() = default;

bool Page::IsOpen() const { return mImpl->open; }

void Page::Open() {
  if (mImpl->open) {
    return;
  }
  if (sCurrent != nullptr && sCurrent != this) {
    sCurrent->Close();
  }
  sCurrent = this;
  mImpl->open = true;
  mImpl->openedAt = Clock::now();
  if (!PortOverlay::TickHold()) {
    PortOverlay::SetTickHold(true);
    mImpl->heldTicks = true;
  }
  PortMouse::SetUiActive(true);
  ResetInput();
  sAnyOpen.store(true);
  OnOpen();
}

void Page::Close() {
  if (!mImpl->open) {
    return;
  }
  mImpl->open = false;
  if (sCurrent == this) {
    sCurrent = nullptr;
  }
  sAnyOpen.store(false);
  if (mImpl->heldTicks) {
    PortOverlay::SetTickHold(false);
    mImpl->heldTicks = false;
  }
  PortMouse::SetUiActive(PortOverlay::Visible());
  // The blur goes at once (its copy is of the frame the game is leaving); the dark fades out.
  StopBlur();
  mImpl->closedAt = Clock::now();
  mImpl->dimAtClose = std::min(1.f, Seconds(mImpl->openedAt) / kDimIn);
  OnClose();
}

void Page::StepGame(unsigned ticks) {
  // Also under a hold the page did not take (the console's `pause`), so a save still runs.
  if (mImpl->open && PortOverlay::TickHold()) {
    PortOverlay::StepTicks(ticks);
  }
}

void Page::Draw() {
  if (mImpl->open) {
    const PageInput input = TakeInput();
    Update(input);
  }
  if (!mImpl->open) {
    const float since = Seconds(mImpl->closedAt);
    if (mImpl->dimAtClose > 0.f && since < kDimOut) {
      DimGame(mImpl->dimAtClose * (1.f - since / kDimOut));
    }
    return;
  }
  const float since = Seconds(mImpl->openedAt);
  BlurGame(std::min(1.f, since / kDimIn));
  DimGame(std::min(1.f, since / kDimIn));
  const float alpha = std::clamp((since - kPageFrom) / kPageFade, 0.f, 1.f);
  if (alpha > 0.f) {
    Render(alpha);
  }
}

// --- List ------------------------------------------------------------------------------------------

struct List::Impl {
  ListSpec spec;
  std::vector< std::vector< std::string > > cells;
  int selected = 0;
  int pressed = -1;
  int first = 0; // the top row shown, when not all fit
  // A scrolling list's thumb: how much of it shows, and where.
  Box* thumb = nullptr;
  std::vector< Text* > columns;
  // One line and two of the same font, spacing and scale, never seen: the line's ink and the pitch
  // from one line to the next, measured rather than guessed from the column's ink.
  Text* probe1 = nullptr;
  Text* probe2 = nullptr;
  Box* bar = nullptr;
  PortGameUiMath::Lines lines;
  bool linesValid = false;

  explicit Impl(const ListSpec& s) : spec(s) {
    cells.assign(std::max(1, spec.rows), std::vector< std::string >(spec.columns.size()));
    columns.assign(spec.columns.size(), nullptr);
  }
  ~Impl() {
    for (Text*& column : columns) {
      Free(column);
    }
    Free(probe1);
    Free(probe2);
    Free(bar);
    Free(thumb);
  }

  int Rows() const { return static_cast< int >(cells.size()); }
  // How many rows show at once.
  int Shown() const { return spec.visibleRows > 0 ? std::min(spec.visibleRows, Rows()) : Rows(); }
  // Scrolls as little as keeps the selection in view.
  void Follow() {
    first = std::min(first, selected);
    first = std::max(first, selected - Shown() + 1);
    first = std::clamp(first, 0, Rows() - Shown());
  }

  std::string Spacing() const { return "&line-spacing=" + std::to_string(spec.lineSpacing) + ";"; }

  // The list's left and right, in fractions of the screen's width.
  void Span(float& left, float& right) const {
    const float aspect = ScreenAspect();
    left = 0.5f - 0.5f * spec.width / aspect;
    right = 0.5f + 0.5f * spec.width / aspect;
  }

  void Measure() {
    if (probe1 == nullptr) {
      probe1 = new Text(TextSpec{.text = Spacing() + "A", .scale = spec.scale});
      probe2 = new Text(TextSpec{.text = Spacing() + "A\nA", .scale = spec.scale});
      probe1->SetAlpha(0.f);
      probe2->SetAlpha(0.f);
    }
    probe1->Draw();
    probe2->Draw();
    const ScreenRect one = probe1->LastRect();
    const ScreenRect two = probe2->LastRect();
    const ScreenRect first = columns.empty() || columns[0] == nullptr ? ScreenRect{} : columns[0]->LastRect();
    linesValid = one.valid && two.valid && first.valid;
    if (linesValid) {
      lines.ink = one.y1 - one.y0;
      lines.pitch = (two.y1 - two.y0) - lines.ink;
      lines.top = first.y0;
    }
  }
};

List::List(const ListSpec& spec) : mImpl(std::make_unique< Impl >(spec)) {}
List::~List() = default;

void List::SetCell(int row, int column, const std::string& text) {
  if (row >= 0 && row < static_cast< int >(mImpl->cells.size()) && column >= 0 &&
      column < static_cast< int >(mImpl->spec.columns.size())) {
    mImpl->cells[row][column] = text;
  }
}

int List::Selected() const { return mImpl->selected; }
void List::SetSelected(int row) {
  mImpl->selected = std::clamp(row, 0, mImpl->Rows() - 1);
  mImpl->Follow();
}

int List::Handle(const PageInput& input) {
  Impl& m = *mImpl;
  const int rows = m.Rows();
  if (input.vertical != 0) {
    m.selected = PortGameUiMath::Wrap(m.selected, input.vertical, rows);
    m.Follow();
  }
  // The wheel scrolls the rows, and the selection along with them where it would leave the view.
  if (input.wheel != 0 && m.Shown() < rows) {
    m.first = std::clamp(m.first - input.wheel, 0, rows - m.Shown());
    m.selected = std::clamp(m.selected, m.first, m.first + m.Shown() - 1);
  }
  if (!m.linesValid) {
    return -1;
  }
  float left = 0.f, right = 0.f;
  m.Span(left, right);
  const auto rowAt = [&](float x, float y) {
    const int shown = x >= left && x <= right ? m.lines.RowAt(y, m.Shown()) : -1;
    return shown >= 0 ? m.first + shown : -1;
  };
  if (input.pointerMoved) {
    const int row = rowAt(input.pointerX, input.pointerY);
    if (row >= 0) {
      m.selected = row;
    }
  }
  int picked = -1;
  for (const PageInput::Click& c : input.clicks) {
    const int row = rowAt(c.x, c.y);
    if (c.down) {
      m.pressed = row;
      if (row >= 0) {
        m.selected = row;
      }
    } else {
      if (row >= 0 && row == m.pressed) {
        picked = row;
      }
      m.pressed = -1;
    }
  }
  return picked;
}

void List::Draw(float alpha) {
  Impl& m = *mImpl;
  const float aspect = ScreenAspect();
  const float side = 0.5f * (aspect - m.spec.width); // the screen's edge to the list's, in heights
  for (size_t c = 0; c < m.spec.columns.size(); ++c) {
    const ListColumn& col = m.spec.columns[c];
    const bool right = col.edge == Anchor::Right;
    std::string text = std::string("&just=") + (right ? "right" : "left") + ";" + m.Spacing();
    for (int r = m.first; r < m.first + m.Shown(); ++r) {
      // An empty cell holds an invisible letter: the game's text drops blank lines at a block's
      // end, and the shorter column, centred on the list, would sit its rows lower than the others'.
      const std::string& cell = m.cells[r][c];
      text += (r == m.selected ? kLit : kDim) +
              (cell.empty() ? std::string("&push;&main-color=#00000000;&outline-color=#00000000;A&pop;") : cell);
      if (r + 1 < m.first + m.Shown()) {
        text += "\n";
      }
    }
    const Placement placement{right ? Anchor::Right : Anchor::Left, side + col.inset, m.spec.centreY};
    if (m.columns[c] == nullptr) {
      m.columns[c] = new Text(TextSpec{.text = text, .scale = m.spec.scale, .placement = placement});
    }
    m.columns[c]->SetPlacement(placement);
    m.columns[c]->SetText(text);
  }
  m.Measure();
  // The bar under the selected row: the plain box, a little shorter than the pitch so it holds the
  // one line (it can't be thinner than its corners).
  if (m.linesValid) {
    if (m.bar == nullptr) {
      m.bar = new Box(BoxSpec{.style = Style::Plain,
                              .button = false,
                              .width = m.spec.width - 0.12f,
                              .height = m.lines.pitch * 0.95f,
                              .scale = 0.6f});
    }
    m.bar->SetPlacement({Anchor::Centre, 0.f, m.lines.Centre(m.selected - m.first) - 0.5f});
    m.bar->SetAlpha(alpha);
    m.bar->Draw();
    // The thumb beside the rows: as tall as the share of them shown, as far down as they are
    // scrolled. Made again when its size changes (a box is sized when made).
    if (m.Shown() < m.Rows()) {
      const float span = m.lines.pitch * static_cast< float >(m.Shown());
      const float height = std::max(kThumbWidth * 2.f, span * static_cast< float >(m.Shown()) / m.Rows());
      const float top = m.lines.Centre(0) - 0.5f * m.lines.pitch;
      const float travel = span - height;
      const float y = top + 0.5f * height + travel * static_cast< float >(m.first) / (m.Rows() - m.Shown());
      if (m.thumb == nullptr) {
        m.thumb = new Box(
            BoxSpec{.style = Style::Plain, .button = false, .width = kThumbWidth, .height = height, .scale = 0.5f});
      }
      m.thumb->SetPlacement({Anchor::Centre, 0.5f * m.spec.width - kThumbIn, y - 0.5f});
      m.thumb->SetAlpha(alpha);
      m.thumb->Draw();
    }
  }
  for (Text* column : m.columns) {
    column->SetAlpha(alpha);
    column->Draw();
  }
}

// --- ActionBar -------------------------------------------------------------------------------------

struct ActionBar::Impl {
  float width, y, scale;
  bool framed = false;
  float barHeight = 0.f;
  Box* bar = nullptr;
  std::vector< PageAction > actions;
  std::vector< Text* > texts;
  int pressed = -1;
  ~Impl() {
    for (Text*& text : texts) {
      Free(text);
    }
    Free(bar);
  }
};

ActionBar::ActionBar(float width, float y, float scale, bool framed, float barHeight)
    : mImpl(std::make_unique< Impl >()) {
  mImpl->width = width;
  mImpl->y = y;
  mImpl->scale = scale;
  mImpl->framed = framed;
  mImpl->barHeight = barHeight;
}
ActionBar::~ActionBar() = default;

void ActionBar::SetActions(const std::vector< PageAction >& actions) {
  Impl& m = *mImpl;
  if (actions.size() != m.texts.size()) {
    for (Text*& text : m.texts) {
      Free(text);
    }
    m.texts.assign(actions.size(), nullptr);
  }
  m.actions = actions;
}

int ActionBar::Handle(const PageInput& input) {
  Impl& m = *mImpl;
  for (size_t i = 0; i < m.actions.size(); ++i) {
    if (m.actions[i].enabled && input.Pressed(m.actions[i].button)) {
      return static_cast< int >(i);
    }
  }
  int done = -1;
  for (const PageInput::Click& c : input.clicks) {
    int over = -1;
    for (size_t i = 0; i < m.texts.size(); ++i) {
      if (m.texts[i] != nullptr) {
        const ScreenRect r = m.texts[i]->LastRect();
        if (Inside(r, c.x, c.y, 0.5f * (r.y1 - r.y0))) {
          over = static_cast< int >(i);
        }
      }
    }
    if (c.down) {
      m.pressed = over;
    } else {
      if (over >= 0 && over == m.pressed && m.actions[over].enabled) {
        done = over;
      }
      m.pressed = -1;
    }
  }
  return done;
}

void ActionBar::Draw(float alpha) {
  Impl& m = *mImpl;
  if (m.framed) {
    if (m.bar == nullptr) {
      m.bar = new Box(BoxSpec{.style = Style::Bar, .button = false, .width = m.width, .height = m.barHeight});
    }
    m.bar->SetPlacement({Anchor::Centre, 0.f, m.y});
    m.bar->SetAlpha(alpha);
    m.bar->Draw();
  }
  const float n = static_cast< float >(m.actions.size());
  for (size_t i = 0; i < m.actions.size(); ++i) {
    const PageAction& a = m.actions[i];
    // Even spread over all but the outer few percent of the width.
    const float x = m.width * 0.96f * ((static_cast< float >(i) + 0.5f) / n - 0.5f);
    const std::string text = std::string(a.enabled ? kLit : kOff) + Glyph(a.button) + " " + a.label;
    if (m.texts[i] == nullptr) {
      m.texts[i] = new Text(TextSpec{.text = text, .scale = m.scale, .placement = {Anchor::Centre, x, m.y}});
    }
    m.texts[i]->SetPlacement({Anchor::Centre, x, m.y});
    m.texts[i]->SetText(text);
    m.texts[i]->SetAlpha(alpha);
    m.texts[i]->Draw();
  }
}

// --- PageFrame -------------------------------------------------------------------------------------

namespace {
// Measured from the map screen (CAutoMapper's FRME_MapScreen) at 16:9 and 21:9, in screen heights:
// the frame keeps the same margins and stretches across, the label keeps its size.
constexpr float kFrameMarginX = 0.125f;
constexpr float kFrameMarginY = 0.08f;
constexpr float kLabelScale = 1.f;   // the map's own size
constexpr float kLabelInset = 0.022f; // its left edge in from the frame's
constexpr float kLabelDown = 0.044f;  // its middle below the frame's top
constexpr float kTitleScale = 0.78f;
constexpr float kTitleInset = 0.045f; // the title's left edge in from the label's
constexpr float kBarInsetX = 0.05f;
constexpr float kBarHeight = 0.12f;
constexpr float kBarUp = 0.027f; // its bottom above the frame's
constexpr float kActionScale = 0.7f;
constexpr float kContentInsetX = 0.06f;
constexpr float kContentBelowTop = 0.11f; // below the title's tab
constexpr float kContentAboveBar = 0.02f;
constexpr float kTabScale = 0.6f;
constexpr float kTabGap = 0.06f;   // the first tab's left edge right of the label's, at least
// The first tab's left edge, as a share of the frame's width in from its left: past the notch where
// the label's tab steps up to the frame's top edge (about 0.3 of the width at 16:9 and 16:10), so
// the tabs sit inside the frame; and their middle that far below the label's, under the top edge.
constexpr float kTabStart = 0.33f;
constexpr float kTabRightInset = 0.05f; // the last tab's right edge in from the frame's, at least
constexpr float kTabDown = 0.014f;
constexpr float kTabSpacing = 0.05f; // between one tab's text and the next
} // namespace

struct PageFrame::Impl {
  float width = 0.f, height = 0.f; // the frame's
  Box* frame = nullptr;
  Box* label = nullptr;
  Text* title = nullptr;
  ActionBar* actions = nullptr;
  // The page tabs along the top, right of the label: one text each (SetTabs).
  std::vector< std::string > tabNames;
  int currentTab = -1;
  std::vector< Text* > tabs;
  float tabScale = kTabScale;
  int tabPressed = -1;
  ~Impl() {
    Free(frame);
    Free(label);
    Free(title);
    Free(actions);
    for (Text*& tab : tabs) {
      Free(tab);
    }
  }
  // The tab under (x, y), padded by half a line, else -1.
  int TabAt(float x, float y) const {
    for (size_t i = 0; i < tabs.size(); ++i) {
      const ScreenRect r = tabs[i]->LastRect();
      if (r.valid && Inside(r, x, y, 0.5f * (r.y1 - r.y0))) {
        return static_cast< int >(i);
      }
    }
    return -1;
  }
  // Lays the tabs out left to right from past the label's notch, each where the last one's text
  // ended (their widths are measured, so a tab not yet drawn is drawn unseen first).
  void DrawTabs(float alpha) {
    const ScreenRect labelRect = label->LastRect();
    bool measured = labelRect.valid;
    for (Text* tab : tabs) {
      measured = measured && tab->LastRect().valid;
    }
    const float aspect = ScreenAspect();
    const float start = measured ? std::max(labelRect.x1 * aspect + kTabGap, kFrameMarginX + kTabStart * width) : 0.f;
    float x = start;
    float textWidth = 0.f; // the names' own widths, without the spacing between them
    for (size_t i = 0; i < tabs.size(); ++i) {
      const bool current = static_cast< int >(i) == currentTab;
      // The current page in the title's colour, the others dim; arrows at the ends say left and
      // right turn through them.
      std::string text = current ? std::string() : std::string(kDim);
      if (i == 0) {
        text = std::string("&push;") + kDim + "<  &pop;" + text;
      }
      text += tabNames[i];
      if (i + 1 == tabs.size()) {
        text += std::string("&push;") + kDim + "  >&pop;";
      }
      tabs[i]->SetText(text);
      tabs[i]->SetPlacement({Anchor::Left, x, Top() + kLabelDown + kTabDown});
      tabs[i]->SetAlpha(measured ? alpha : 0.f);
      tabs[i]->Draw();
      const ScreenRect r = tabs[i]->LastRect();
      if (measured && r.valid) {
        x += (r.x1 - r.x0) * aspect + kTabSpacing;
        textWidth += (r.x1 - r.x0) * aspect;
      }
    }
    // More names than the frame's top has room for: made again smaller, to fit (drawn unseen once
    // while they are measured again).
    const float room = kFrameMarginX + width - kTabRightInset - start;
    const float spacing = kTabSpacing * static_cast< float >(tabs.size() - 1);
    if (measured && textWidth > 0.f && textWidth + spacing > room) {
      tabScale *= std::max(0.3f, (room - spacing) / textWidth) * 0.98f;
      MakeTabs();
    }
  }
  void MakeTabs() {
    for (Text*& tab : tabs) {
      Free(tab);
      tab = new Text(TextSpec{.scale = tabScale});
    }
  }
  float Top() const { return -0.5f * height; }
  float Bottom() const { return 0.5f * height; }
  float BarY() const { return Bottom() - kBarUp - 0.5f * kBarHeight; }
};

PageFrame::PageFrame(const std::string& title) : mImpl(std::make_unique< Impl >()) {
  Impl& m = *mImpl;
  m.width = ScreenAspect() - 2.f * kFrameMarginX;
  m.height = 1.f - 2.f * kFrameMarginY;
  m.frame = new Box(BoxSpec{.style = Style::Frame, .button = false, .width = m.width, .height = m.height});
  // The label is one textured quad (no corners to stretch): its own text is only there to shape it
  // and is not drawn; the title is a text of its own over it.
  const float left = kFrameMarginX + kLabelInset;
  m.label = new Box(BoxSpec{.style = Style::Label,
                            .text = "&main-color=#00000000;Title",
                            .button = false,
                            .scale = kLabelScale,
                            .placement = {Anchor::Left, left, m.Top() + kLabelDown}});
  m.title = new Text(TextSpec{
      .text = title, .scale = kTitleScale, .placement = {Anchor::Left, left + kTitleInset, m.Top() + kLabelDown}});
  m.actions = new ActionBar(m.width - 2.f * kBarInsetX, m.BarY(), kActionScale, true, kBarHeight);
}
PageFrame::~PageFrame() = default;

void PageFrame::SetTitle(const std::string& title) { mImpl->title->SetText(title); }

void PageFrame::SetTabs(const std::vector< std::string >& names, int current) {
  Impl& m = *mImpl;
  if (names != m.tabNames) {
    for (Text*& tab : m.tabs) {
      Free(tab);
    }
    m.tabs.assign(names.size(), nullptr);
    m.tabScale = kTabScale;
    m.MakeTabs();
    m.tabNames = names;
  }
  m.currentTab = current;
}

int PageFrame::TabClicked(const PageInput& input) {
  Impl& m = *mImpl;
  int clicked = -1;
  for (const PageInput::Click& c : input.clicks) {
    const int tab = m.TabAt(c.x, c.y);
    if (c.down) {
      m.tabPressed = tab;
    } else {
      if (tab >= 0 && tab == m.tabPressed) {
        clicked = tab;
      }
      m.tabPressed = -1;
    }
  }
  return clicked;
}

void PageFrame::SetActions(const std::vector< PageAction >& actions) { mImpl->actions->SetActions(actions); }

int PageFrame::Handle(const PageInput& input) { return mImpl->actions->Handle(input); }

void PageFrame::Draw(float alpha) {
  Impl& m = *mImpl;
  for (Box* box : {m.frame, m.label}) {
    box->SetAlpha(alpha);
    box->Draw();
  }
  m.title->SetAlpha(alpha);
  m.title->Draw();
  m.DrawTabs(alpha);
  m.actions->Draw(alpha);
}

PageArea PageFrame::Content() const {
  const Impl& m = *mImpl;
  PageArea area;
  area.left = -0.5f * m.width + kContentInsetX;
  area.right = 0.5f * m.width - kContentInsetX;
  area.top = m.Top() + kContentBelowTop;
  area.bottom = m.BarY() - 0.5f * kBarHeight - kContentAboveBar;
  return area;
}

// --- QuestionInput ---------------------------------------------------------------------------------

int QuestionInput::Handle(Question& question, const PageInput& input) {
  if (input.back || input.Pressed(Button::Start)) {
    mPressed = -1;
    return kCancel;
  }
  if (input.horizontal != 0) {
    question.SetSelected(input.horizontal < 0 ? 0 : 1);
  }
  const auto over = [&](float x, float y) {
    for (int choice = 0; choice < 2; ++choice) {
      const ScreenRect r = question.ChoiceRect(choice);
      if (Inside(r, x, y, 0.5f * (r.y1 - r.y0))) {
        return choice;
      }
    }
    return -1;
  };
  if (input.pointerMoved) {
    const int choice = over(input.pointerX, input.pointerY);
    if (choice >= 0) {
      question.SetSelected(choice);
    }
  }
  for (const PageInput::Click& c : input.clicks) {
    const int choice = over(c.x, c.y);
    if (c.down) {
      mPressed = choice;
    } else {
      const bool same = choice >= 0 && choice == mPressed;
      mPressed = -1;
      if (same) {
        question.SetSelected(choice);
        return choice;
      }
    }
  }
  if (input.Pressed(Button::A)) {
    return question.Selected();
  }
  return -1;
}

} // namespace PortGameUi
