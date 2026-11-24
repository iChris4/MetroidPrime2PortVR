#pragma once

// Pages: the port's own full-screen menus, drawn by the game's GUI (port_game_ui.h) over the game
// the way the pause screen is. While a page is open the game stands still, blurs and fades to dark,
// and reads a neutral pad (as with the F1 overlay); the page reads the player's pad, the mouse and
// the console's presses instead. A page is a Page subclass made of the kit's boxes and text and the
// pieces below: a List of rows to pick from, an ActionBar of the buttons' actions, and
// QuestionInput to answer a Question.
//
//   class MyPage : public PortGameUi::Page {
//     void Update(const PortGameUi::PageInput& in) override {
//       mList.Handle(in);
//       if (mActions.Handle(in) == 0) { ...the first action, on mList.Selected()... }
//       if (in.back) Close();
//     }
//     void Render(float alpha) override { mList.Draw(alpha); mActions.Draw(alpha); }
//   };
//
// Everything here runs on the game thread (Page::Draw, from main.cpp after the game's GUI); the
// feeds (NotePagePad, ScriptPagePress) may be called from the input threads.

#include "port_game_ui.h"

#include <memory>
#include <string>
#include <vector>

struct PADStatus;

namespace PortGameUi {

// The game's buttons a page acts on: the player's bindings say which keys and pad buttons they are.
enum class Button { A, B, X, Y, Start, Count };

// The game's text tag for a button's image (&image=...;), which the prompt system draws as the
// bound button's glyph; empty before the game's tweaks are loaded.
std::string Glyph(Button button);

// A page's input for one frame.
struct PageInput {
  int vertical = 0;   // a step: -1 up, +1 down (a held direction steps again after a moment)
  int horizontal = 0; // -1 left, +1 right
  bool pressed[static_cast< int >(Button::Count)] = {};
  bool Pressed(Button button) const { return pressed[static_cast< int >(button)]; }
  // B, or the mouse's right button: back out of what is up.
  bool back = false;
  // The pointer, in fractions of the screen from its top left; moved since the last frame.
  float pointerX = -1.f, pointerY = -1.f;
  bool pointerMoved = false;
  // The mouse wheel's steps this frame: + away from the player (up), - towards.
  int wheel = 0;
  // The left button's presses and releases this frame, in order, where they happened.
  struct Click {
    bool down;
    float x, y;
  };
  std::vector< Click > clicks;
};

// Player 1's pad as the game would read it, before the game's gate takes it away (PortPumpEvents).
void NotePagePad(const PADStatus& pad);
// A scripted press for the open page (the console): up, down, left, right, a, b, x, y or start.
bool ScriptPagePress(const std::string& name);
// A page is open: the game reads a neutral pad, and direct aim and the beam and visor picks wait.
bool PageOpen();

class Page {
public:
  Page();
  virtual ~Page();
  Page(const Page&) = delete;
  Page& operator=(const Page&) = delete;

  // Opening freezes and fades the game and frees the cursor; closing lets the game run on while the
  // dark fades back out. One page at a time: opening one closes another.
  void Open();
  void Close();
  bool IsOpen() const;
  // Each frame, after the game's GUI: reads the input, runs Update, draws the page.
  void Draw();

protected:
  virtual void OnOpen() {}
  // Free the page's elements here (they are made again when it next opens).
  virtual void OnClose() {}
  virtual void Update(const PageInput& input) = 0;
  // `alpha` is the page's fade, 0 to 1: pass it to each element.
  virtual void Render(float alpha) = 0;
  // Lets the frozen game run `ticks` ticks (a save state is written in the game's tick).
  void StepGame(unsigned ticks);

private:
  struct Impl;
  std::unique_ptr< Impl > mImpl;
};

// A list of rows to pick one from: columns of game text at an edge of the list (a number, a name,
// a time), the selected row lit on a bar and the others dim. Up and down move the selection
// (wrapping); the pointer selects the row it is over. A list longer than `visibleRows` shows that
// many at a time, scrolled to keep the selection in view (and by the mouse wheel), with a dim mark
// above or below while there are more rows that way.
struct ListColumn {
  Anchor edge = Anchor::Left; // Left: the column's left edge sits `inset` in from the list's; Right: its right edge
  float inset = 0.f;          // screen heights
};
struct ListSpec {
  int rows = 1;
  std::vector< ListColumn > columns;
  float width = 1.f;   // screen heights, centred
  float centreY = 0.f; // the rows' middle, + down from the screen's centre, screen heights
  float scale = 0.6f;
  int lineSpacing = 150; // percent of the font's own, so the bar holds one line
  int visibleRows = 0;   // 0: every row
};

class List {
public:
  explicit List(const ListSpec& spec);
  ~List();
  List(const List&) = delete;
  List& operator=(const List&) = delete;

  // A cell's text (game tags work: its own colour overrides the lit or dim one).
  void SetCell(int row, int column, const std::string& text);
  int Selected() const;
  void SetSelected(int row);
  // Moves the selection by the input; the row a click picked (pressed and released on it), else -1.
  int Handle(const PageInput& input);
  void Draw(float alpha);

private:
  struct Impl;
  std::unique_ptr< Impl > mImpl;
};

// A row of actions along the bottom of a page, each its button's glyph and a name, dim when it
// can't be done. A press of its button, or a click on it, does it.
struct PageAction {
  Button button = Button::A;
  std::string label;
  bool enabled = true;
};

class ActionBar {
public:
  // Spread over `width` (screen heights, centred) with their middle `y` below the screen's centre;
  // `framed` draws them in the map screen's instruction bar (Style::Bar), `barHeight` tall.
  ActionBar(float width, float y, float scale = 0.55f, bool framed = false, float barHeight = 0.09f);
  ~ActionBar();
  ActionBar(const ActionBar&) = delete;
  ActionBar& operator=(const ActionBar&) = delete;

  void SetActions(const std::vector< PageAction >& actions);
  // The index of the action pressed or clicked this frame, else -1 (a disabled one does nothing).
  int Handle(const PageInput& input);
  void Draw(float alpha);

private:
  struct Impl;
  std::unique_ptr< Impl > mImpl;
};

// The default page layout: the map and pause screens' frame over the whole screen (the same margins
// at any aspect, as the map's), its title in the label in the top-left tab ("Light Map") and its
// actions in the instruction bar along the bottom. A page puts its own elements in Content().
//
//   PageFrame frame("Save states");             // in OnOpen (the screen's size is read then)
//   frame.SetActions({{Button::A, "Load"}, {Button::B, "Back"}});
//   const int action = frame.Handle(input);     // in Update
//   frame.Draw(alpha);                          // in Render, before the page's own elements
struct PageArea {
  // Screen heights from the screen's centre, + right and + down (Anchor::Centre placements).
  float left = 0.f, right = 0.f, top = 0.f, bottom = 0.f;
  float Width() const { return right - left; }
  float Height() const { return bottom - top; }
  float CentreY() const { return 0.5f * (top + bottom); }
};

class PageFrame {
public:
  explicit PageFrame(const std::string& title);
  ~PageFrame();
  PageFrame(const PageFrame&) = delete;
  PageFrame& operator=(const PageFrame&) = delete;

  void SetTitle(const std::string& title);
  // A page with pages of its own (as the pause screen's Map, Inventory, ...): their names along the
  // top, right of the title's label, `current` lit and the others dim, with arrows at the ends for
  // left and right. The page turns on left and right and on TabClicked itself.
  void SetTabs(const std::vector< std::string >& names, int current);
  // The tab clicked (pressed and released on it) this frame, else -1.
  int TabClicked(const PageInput& input);
  void SetActions(const std::vector< PageAction >& actions);
  // The actions' (ActionBar::Handle): the index pressed or clicked this frame, else -1.
  int Handle(const PageInput& input);
  void Draw(float alpha);
  // Inside the frame: below the title's tab and above the instruction bar.
  PageArea Content() const;

private:
  struct Impl;
  std::unique_ptr< Impl > mImpl;
};

// A Question's input: left and right or the pointer highlight a choice; A or a click on one answers.
class QuestionInput {
public:
  static constexpr int kCancel = 2;
  // 0 or 1 when answered, kCancel for back (B, a right click, Start), else -1.
  int Handle(Question& question, const PageInput& input);

private:
  int mPressed = -1;
};

} // namespace PortGameUi
