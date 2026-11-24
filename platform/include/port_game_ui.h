#pragma once

// PortGameUi: the port's own boxes and text, drawn by the game's GUI system so they look like the
// game's own.
//
// A Box is one of the game's boxes (its frame line, its fill, its button and its text), sized to
// its text or to a size you give, and placed anywhere on screen. A Text is the game's text alone. A
// Question is the game's Yes/No box (the quit screen's): a title over two choices, one highlighted.
// Both draw with the game's font, colours and lighting and with everything the port does to those
// frames (the bound button's glyph, the drawn button ring, a texture pack's art), and never pause
// the game.
//
//   // "Skip cutscene?" in a scan-box outline at the bottom of the screen.
//   PortGameUi::Box skip({.style = PortGameUi::Style::Scan,
//                         .fill = PortGameUi::Fill::None,
//                         .text = "Skip cutscene?",
//                         .placement = {PortGameUi::Anchor::Bottom, 0.f, 0.012f}});
//   // The port's version in the top left corner.
//   PortGameUi::Text version({.text = "Metroid Prime 2 port 0.12.0",
//                             .placement = {PortGameUi::Anchor::TopLeft, 0.03f, 0.03f}});
//   // A Yes/No question in the middle of the screen.
//   PortGameUi::Question resume({.title = "Resume where you left off?"});
//   ...
//   skip.SetAlpha(fade);   // 0 hides it
//   skip.Draw();           // where the game draws its GUI (see Draw)
//
// Sizes and offsets are fractions of the screen's height, so the same numbers fit every resolution
// and aspect. Each box and text loads the frame it is made from into a resource pool of its own, so
// nothing it changes reaches the game's copy; it draws nothing until that frame, its models and its
// font are in (a frame or two after it is made).
//
// Threads and lifetime: make, change and draw them on the game thread. Don't let one be destroyed
// while the program exits (a static unique_ptr's destructor): its models are freed through the
// render thread, which is gone by then, and the exit hangs. Keep a long-lived one behind a pointer
// that is never deleted; destroy them freely while the game runs.

#include <memory>
#include <string>

namespace PortGameUi {

// Which of the game's boxes a Box is made from.
enum class Style {
  Message, // the paused message box ("Map data acquired"): a dark fill, its button on the bottom edge
  Hint,    // the HUD's hint memo: longer and thinner, its button on the bottom edge
  Scan,    // the scan visor's message box: a bracket line round a light, see-through fill
  Plain,   // the quit screen's box (Question's): a thin line round a dark fill, no button or notch
  Frame,   // the map and pause screens' frame: a thick line with heavy corners and a thin one inside
  Bar,     // the map and pause screens' instruction bar: a double line round a black fill
  Label,   // the map and pause screens' title label ("Light Map"): a thin line round a black fill
};

// What fills the box inside its line.
enum class Fill {
  Game, // the style's own fill
  None, // the line alone: the scene shows through
};

// A point of the screen, and the same point of the box or text, that are put together.
enum class Anchor { TopLeft, Top, TopRight, Left, Centre, Right, BottomLeft, Bottom, BottomRight };

// Where a box or text goes: its `anchor` point at the screen's, then (x, y) in fractions of the
// screen's height. Along an anchored edge the offset is a margin in from that edge; along a centred
// axis it is + right / + down. {Anchor::Bottom, 0, 0.02} sits the box's bottom (the button's, when
// it has one) 2% of the screen's height above the bottom; {Anchor::TopLeft, 0.05, 0.05} puts its
// top left corner 5% in from the top left.
struct Placement {
  Anchor anchor = Anchor::Centre;
  float x = 0.f;
  float y = 0.f;
};

struct BoxSpec {
  Style style = Style::Message;
  Fill fill = Fill::Game;
  // UTF-8; the game's text tags work (colours, a button's image).
  std::string text;
  // The style's button (the bound button's glyph, as the prompt system draws it); false hides it.
  bool button = true;
  // The box's size in fractions of the screen's height; 0 fits it to the text, with the style's own
  // padding. A box keeps at least the size its corners need.
  float width = 0.f;
  float height = 0.f;
  // How big it draws against the game's own size, line, text and button alike (0.75 is three
  // quarters); placement and sizes given above stay in screen heights.
  float scale = 1.f;
  Placement placement;
};

struct TextSpec {
  // UTF-8; the game's text tags work. One line, however long, unless a \n breaks it.
  std::string text;
  // Its size against the game's own text (as BoxSpec::scale).
  float scale = 1.f;
  Placement placement;
};

struct QuestionSpec {
  // UTF-8, its lines broken by \n; the game's text tags work.
  std::string title;
  // Left and right.
  std::string choices[2] = {"Yes", "No"};
  // The highlighted one, 0 (left) or 1.
  int selected = 0;
  float scale = 1.f;
  Placement placement;
};

// Where it was drawn, in fractions of the screen's width (x) and height (y) from its top left (for
// a tap or click on it); `valid` once it has drawn.
struct ScreenRect {
  bool valid = false;
  float x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f;
};

class Box {
public:
  explicit Box(const BoxSpec& spec);
  ~Box();
  Box(const Box&) = delete;
  Box& operator=(const Box&) = delete;

  // A new text: a box sized to its text sizes itself again.
  void SetText(const std::string& text);
  // How opaque it draws, 0 to 1; 0 draws nothing. The owner fades it.
  void SetAlpha(float alpha);
  void SetPlacement(const Placement& placement);
  // Draws it where the game draws its GUI: CInGameGuiManager::Draw in game. Nothing until it is
  // loaded, or at alpha 0.
  void Draw();
  // True once it has drawn its first frame (its size is set).
  bool Ready() const;
  // Its frame couldn't be loaded: it never draws (the owner falls back).
  bool Failed() const;
  // Where it last drew.
  ScreenRect LastRect() const;

  struct Impl;

private:
  std::unique_ptr< Impl > mImpl;
};

class Text {
public:
  explicit Text(const TextSpec& spec);
  ~Text();
  Text(const Text&) = delete;
  Text& operator=(const Text&) = delete;

  void SetText(const std::string& text);
  void SetAlpha(float alpha);
  void SetPlacement(const Placement& placement);
  void Draw();
  bool Ready() const;
  ScreenRect LastRect() const;

private:
  std::unique_ptr< Box::Impl > mImpl;
};

// The owner reads the input and says which choice is highlighted (SetSelected); ChoiceRect gives
// where each choice was drawn, for a pointer to hover and click.
class Question {
public:
  explicit Question(const QuestionSpec& spec);
  ~Question();
  Question(const Question&) = delete;
  Question& operator=(const Question&) = delete;

  // A new title: the box sizes itself again.
  void SetTitle(const std::string& title);
  void SetSelected(int choice);
  int Selected() const;
  void SetAlpha(float alpha);
  void SetPlacement(const Placement& placement);
  void Draw();
  bool Ready() const;
  // Its frame couldn't be loaded or isn't the quit screen's: it never draws (the owner falls back).
  bool Failed() const;
  ScreenRect LastRect() const;
  // Where choice 0 or 1's text was last drawn.
  ScreenRect ChoiceRect(int choice) const;

private:
  std::unique_ptr< Box::Impl > mImpl;
};

// For trying the kit out and finding what a new style is made of (the console's `gameui`).
namespace Dev {
// A test box or text, shown until hidden or replaced; Draw draws it (and finishes dumps).
void ShowBox(const BoxSpec& spec);
void ShowText(const TextSpec& spec);
void ShowQuestion(const QuestionSpec& spec);
// Highlights choice 0 or 1 of the test question.
void Select(int choice);
void Hide();
void Draw();
// Logs a frame's widgets once it is loaded: each one's name, type, position, visibility and scale,
// a model's vertex extent and the construction kit's zones in it, a text pane's size.
void DumpFrame(const std::string& frameName);
} // namespace Dev

} // namespace PortGameUi
