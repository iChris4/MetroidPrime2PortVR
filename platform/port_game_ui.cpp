// PortGameUi (include/port_game_ui.h): the port's own boxes and text, drawn as the game's.
//
// Each Box or Text loads its style's frame into a CSimplePool of its own, so the frame, its widgets
// and its models are its own copies: its text, colours, visibility, camera and resized geometry
// never reach the copy the game draws. Once the frame's assets, models and font are in, it shows
// only its style's parts (StyleInfo), measures its text, resizes the box's models with the
// construction kit (port_game_ui_math.h), centres the text in the box and places it by moving the
// frame's camera, then draws it as the game draws a frame (CGuiFrame::Draw: its camera, lights and
// widgets). The drawn button ring (port_prompt_ring.cpp) sees the resized models as any others: the
// resize is done before the first draw, so the ring patches the shape it is drawn with.
#include "port_game_ui.h"

#include "port_game_ui_math.h"
#include "port_prompt_ring_math.h"

#include "GuiSys/CGuiCamera.hpp"
#include "GuiSys/CGuiFrame.hpp"
#include "GuiSys/CGuiModel.hpp"
#include "GuiSys/CGuiTextPane.hpp"
#include "GuiSys/CGuiWidgetDrawParms.hpp"
#include "Kyoto/Basics/CBasics.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/Graphics/CCubeMaterial.hpp"
#include "Kyoto/Graphics/CCubeModel.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Streams/CInputStream.hpp"
#include "Kyoto/Text/CGuiTextSupport.hpp"
#include "MetroidPrime/CGameGlobalObjects.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Tweaks/CTweakAutoMapper.hpp"
#include "MetroidPrime/Tweaks/CTweakGuiColors.hpp"
#include "port_overlay.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

namespace PortGameUi {
namespace {

using PortGameUiMath::Point;
using PortGameUiMath::Zones;

// What a style is made of, by widget name in its frame.
struct StyleInfo {
  const char* frame;
  const char* text;      // the text pane
  const char* line;      // the frame line (resized)
  const char* fill;      // the fill inside it (resized; hidden for Fill::None)
  const char* button[2]; // the button's models (hidden for button = false); null: none
  // Space round the text inside the box, in frame units: across, and above and below.
  float padX;
  float padZ;
  // The fill isn't centred on the line (the map's black bar also runs under its right-hand pane):
  // the line sizes the box, and the fill is fitted inside it.
  bool fitFill = false;
};

const StyleInfo& Info(Style style) {
  static const StyleInfo kMessage{"FRME_MsgScreen", "textpane_message", "model_center", "model_bg",
                                  {"model_bottom", "model_abutton"}, 0.9f, 0.55f};
  static const StyleInfo kHint{"FRME_HudMessage", "textpane_message", "model_messageframe",
                               "model_bground", {"model_abutton", "model_zbuttonring"}, 0.7f, 0.4f};
  static const StyleInfo kScan{"FRME_ScanHudFlat", "textpane_message", "model_message_bracket",
                               "model_bgrond", {"model_abutton", nullptr}, 0.9f, 0.5f};
  // Question's frame without its choices (kQuestion below).
  static const StyleInfo kPlain{"FRME_QuitScreen1", "textpane_title", "model_messageframe", "model_bground",
                                {nullptr, nullptr}, 0.22f, 0.12f};
  // The map screen's (CAutoMapper) frame and instruction bar; the pause screen (FRME_LogBook) has
  // the same pieces. The frame has no fill of its own: its line is what it is sized by.
  static const StyleInfo kFrame{"FRME_MapScreen", "textpane_areaname", "model_frame", "model_frame",
                                {nullptr, nullptr}, 1.5f, 1.2f};
  static const StyleInfo kBar{"FRME_MapScreen", "textpane_teleporter", "model_legend_center", "model_black_center",
                              {nullptr, nullptr}, 1.2f, 0.8f, true};
  static const StyleInfo kLabel{"FRME_MapScreen", "textpane_label", "model_lable", "model_lable",
                                {nullptr, nullptr}, 0.8f, 0.5f};
  switch (style) {
  case Style::Plain:
    return kPlain;
  case Style::Frame:
    return kFrame;
  case Style::Bar:
    return kBar;
  case Style::Label:
    return kLabel;
  case Style::Hint:
    return kHint;
  case Style::Scan:
    return kScan;
  default:
    return kMessage;
  }
}

// A Question's frame: the quit screen's box (CQuitGameScreen), its title and its two choices in a
// table, side by side. Its padding is in its own (smaller) frame units.
const StyleInfo kQuestion{"FRME_QuitScreen1", "textpane_title", "model_messageframe", "model_bground",
                          {nullptr, nullptr}, 0.22f, 0.12f};
const char* const kQuestionTable = "tablegroup_choice";
const char* const kQuestionChoices[2] = {"textpane_choice0", "textpane_choice1"};
// Space between the title and the choices, in the same units.
constexpr float kQuestionGap = 0.16f;

// The colours the game gives the style's parts (CMessageScreen, CSamusHud's memo, CHudDecoInterfaceScan).
void ColourParts(Style style, CGuiFrame& frame) {
  const auto set = [&frame](const char* name, const CColor& colour) {
    if (CGuiWidget* w = frame.FindWidget(name)) {
      w->SetColor(colour);
    }
  };
  switch (style) {
  case Style::Message:
    for (const char* name : {"basewidget_top", "basewidget_centerdeco", "model_bottom"}) {
      set(name, gpTweakGuiColors->GetHUDMemoTextForegroundColor());
    }
    set("basewidget_bottom", CColor::White());
    break;
  case Style::Hint:
    set("basewidget_message", CColor::White());
    for (const char* name : {"model_messageframe", "model_zbuttonring"}) {
      set(name, gpTweakGuiColors->GetHUDDecorativeColor());
    }
    break;
  case Style::Scan:
    set("model_message_bracket", gpTweakGuiColors->GetScanWindowFrameActiveColor());
    break;
  case Style::Plain:
    break; // the quit screen's own colours
  case Style::Frame:
  case Style::Bar:
  case Style::Label:
    // The map screen's (CAutoMapper's frame glue).
    set("model_frame", gpTweakAutoMapper->GetFrameColor());
    set("model_legend_center", gpTweakAutoMapper->GetFrameColor());
    set("model_black_center", gpTweakAutoMapper->GetBlackColor());
    set("model_lable", gpTweakAutoMapper->GetFrameColor());
    break;
  }
  set("model_abutton", CColor::White());
}

float ReadFloat(const uint* words, uint i) {
  union {
    uint u;
    float f;
  } v;
  v.u = CBasics::SwapBytes(words[i]);
  return v.f;
}

void WriteFloat(uint* words, uint i, float f) {
  union {
    uint u;
    float f;
  } v;
  v.f = f;
  words[i] = CBasics::SwapBytes(v.u);
}

// A model's vertices, in its own (x, z) plane.
std::vector< Point > PointsOf(const CCubeModel* model) {
  std::vector< Point > points;
  if (model == nullptr || model->GetPositions() == nullptr) {
    return points;
  }
  const uint* pos = static_cast< const uint* >(model->GetPositions());
  const uint n = model->GetModelInstance().GetVertexSize() / (3 * sizeof(uint));
  points.reserve(n);
  for (uint i = 0; i < n; ++i) {
    points.push_back(Point{ReadFloat(pos, i * 3), ReadFloat(pos, i * 3 + 2)});
  }
  return points;
}

// The indices of the vertices a model draws: those its display lists index (a model may hold
// others no triangle uses: the scan box's bracket keeps some far above its top line). All of them
// when its lists can't be read (a format the parser leaves alone).
std::vector< uint > DrawnIndicesOf(const CCubeModel* model) {
  const uint count = model->GetModelInstance().GetVertexSize() / (3 * sizeof(uint));
  std::set< uint > used;
  for (int list = 0; list < 2; ++list) {
    CCubeSurface surface = list == 0 ? model->GetNormalSurfaces() : model->GetAlphaSurfaces();
    for (; surface.IsValid(); surface = surface.GetNextSurface()) {
      const uint8_t* dl = static_cast< const uint8_t* >(surface.GetDisplayList());
      if (dl == nullptr) {
        continue;
      }
      for (const PortPromptRingMath::Command& c : PortPromptRingMath::ParseDisplayList(
               dl, surface.GetDisplayListSize(), model->GetMaterial(surface).GetVertexDesc())) {
        for (const uint32_t v : c.vertices) {
          const uint i = (uint(dl[v]) << 8) | dl[v + 1];
          if (i < count) {
            used.insert(i);
          }
        }
      }
    }
  }
  if (used.empty()) {
    std::vector< uint > all(count);
    for (uint i = 0; i < count; ++i) {
      all[i] = i;
    }
    return all;
  }
  return std::vector< uint >(used.begin(), used.end());
}

// The vertices a model draws, in its own (x, z) plane.
std::vector< Point > DrawnPointsOf(const CCubeModel* model) {
  const std::vector< Point > all = PointsOf(model);
  std::vector< Point > drawn;
  for (const uint i : DrawnIndicesOf(model)) {
    drawn.push_back(all[i]);
  }
  return drawn;
}

const CCubeModel* ModelOf(CGuiWidget* widget) {
  if (widget == nullptr || widget->GetWidgetTypeID() != 'MODL') {
    return nullptr;
  }
  const CCubeModel* model = static_cast< CGuiModel* >(widget)->PortCubeModel();
  return model != nullptr && model->GetPositions() != nullptr ? model : nullptr;
}

// How much a transform scales its x (axis 0) or z (axis 2): some frames' widgets are scaled, and a
// model's vertices are in its own units, a text pane's extents in the pane's.
float AxisScale(const CTransform4f& xf, int axis) {
  const CVector3f unit(axis == 0 ? 1.f : 0.f, 0.f, axis == 2 ? 1.f : 0.f);
  const float s = (xf * unit - xf * CVector3f::Zero()).Magnitude();
  return s > 1e-6f ? s : 1.f;
}

// An extent in world space (x across, z up, y the depth).
struct Extent {
  float x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f, y = 0.f;
  bool valid = false;
  void Add(const CVector3f& p) {
    x0 = std::min(x0, p.GetX());
    x1 = std::max(x1, p.GetX());
    z0 = std::min(z0, p.GetZ());
    z1 = std::max(z1, p.GetZ());
    y = p.GetY();
    valid = true;
  }
};

// A model's vertices through its widget's transform.
void AddWorldExtent(CGuiWidget* widget, Extent& extent) {
  const CCubeModel* model = ModelOf(widget);
  if (model == nullptr) {
    return;
  }
  // Every coordinate, depth too: a model may be built away from its widget along the view, and its
  // depth sets where an offset on screen lands.
  const CTransform4f& xf = widget->GetWorldTransform();
  const uint* pos = static_cast< const uint* >(model->GetPositions());
  for (const uint i : DrawnIndicesOf(model)) {
    extent.Add(xf * CVector3f(ReadFloat(pos, i * 3), ReadFloat(pos, i * 3 + 1), ReadFloat(pos, i * 3 + 2)));
  }
}

// Every widget of a frame, by its name table.
std::vector< CGuiWidget* > WidgetsOf(CGuiFrame& frame) {
  std::vector< CGuiWidget* > out;
  CGuiWidgetIdDB& db = frame.WidgetIdDB();
  for (int id = 0; id < db.PortCount(); ++id) {
    if (CGuiWidget* w = frame.FindWidget(db.PortName(id))) {
      out.push_back(w);
    }
  }
  return out;
}

// Loads a frame by name into `pool`; null when the disc has none.
CGuiFrame* LoadFrame(const char* name, CSimplePool& pool) {
  const SObjectTag* tag = gpResourceFactory->GetResourceIdByName(name);
  if (tag == nullptr) {
    std::fprintf(stderr, "[gameui] no %s on the disc\n", name);
    return nullptr;
  }
  CInputStream* in = gpResourceFactory->GetResLoader().LoadNewResourceSync(*tag, nullptr);
  if (in == nullptr) {
    return nullptr;
  }
  CGuiFrame* frame = new CGuiFrame(*in, &pool);
  delete in;
  return frame;
}

// True once a frame's assets and all its models are in.
bool FrameLoaded(CGuiFrame& frame) {
  if (!frame.GetIsFinishedLoading()) {
    return false;
  }
  for (CGuiWidget* w : WidgetsOf(frame)) {
    if (w->GetWidgetTypeID() == 'MODL' && ModelOf(w) == nullptr) {
      return false;
    }
  }
  return true;
}

// True once nothing loads into the frame's pool any more: its assets, its models' textures, and
// every text pane's font and the images its text names (prompt button glyphs), which load lazily.
bool PoolSettled(CGuiFrame& frame) {
  if (!FrameLoaded(frame)) {
    return false;
  }
  for (CGuiWidget* w : WidgetsOf(frame)) {
    if (w->GetWidgetTypeID() == 'TXPN' &&
        !static_cast< CGuiTextPane* >(w)->TextSupport().GetIsTextSupportFinishedLoading()) {
      return false;
    }
  }
  return true;
}

// A frame whose element was freed before its loads were in: its fonts and textures load through
// the element's own pool, so the pool must outlive the loads (freed with them, the finished load
// writes into it: a crash in CRasterFont's constructor). Cancelling a load does not save it:
// CResFactory finds a pending load by tag alone, so with two pools loading one font a cancel can
// drop the other pool's load and leave this one's running. Held here until PoolSettled, then
// freed (ReapHeld, from every element's draw).
struct HeldFrame {
  CGuiFrame* frame;
  std::unique_ptr< CSimplePool > pool;
};
std::vector< HeldFrame >& Held() {
  // Never freed at exit, as the elements are not (port_game_ui.h).
  static auto* held = new std::vector< HeldFrame >();
  return *held;
}

void ReapHeld() {
  std::vector< HeldFrame >& held = Held();
  for (size_t i = 0; i < held.size();) {
    if (PoolSettled(*held[i].frame)) {
      // The frame's widgets hold tokens of the pool: the frame goes first.
      delete held[i].frame;
      held[i] = std::move(held.back());
      held.pop_back();
    } else {
      ++i;
    }
  }
}

} // namespace

struct Box::Impl {
  BoxSpec spec;
  bool textOnly = false;
  // A Question: the title is `spec.text`, and two choices sit under it (kQuestion).
  bool question = false;
  std::string choices[2];
  int selected = 0;
  Extent choiceInk[2];
  ScreenRect choiceRect[2];
  float alpha = 0.f;
  bool failed = false;
  bool shaped = false;
  bool dressed = false;

  std::unique_ptr< CSimplePool > pool;
  CGuiFrame* frame = nullptr;
  CGuiTextPane* text = nullptr;
  CGuiWidget* camera = nullptr;
  CVector3f cameraHome = CVector3f::Zero();
  CVector3f textHome = CVector3f::Zero();

  // What is drawn, in world space, the button included: placed by its edges.
  Extent box;
  ScreenRect rect;
  std::chrono::steady_clock::time_point last{};
  int layoutWaits = 0; // draws spent waiting for the text's layout (Draw)

  Impl(const BoxSpec& s, bool onlyText) : spec(s), textOnly(onlyText) {}

  ~Impl() { Unload(); }

  const StyleInfo& Style() const { return question ? kQuestion : Info(spec.style); }

  void Unload() {
    if (frame != nullptr && !PoolSettled(*frame)) {
      // Still loading into the pool: both kept until it is done (Held).
      Held().push_back({frame, std::move(pool)});
    } else {
      // The frame's widgets hold tokens of the pool: the frame goes first.
      delete frame;
    }
    frame = nullptr;
    pool.reset();
    text = nullptr;
    camera = nullptr;
    shaped = false;
    dressed = false;
    layoutWaits = 0;
    rect = ScreenRect{};
    choiceRect[0] = choiceRect[1] = ScreenRect{};
  }

  void Load() {
    if (frame != nullptr || failed || gpResourceFactory == nullptr) {
      return;
    }
    pool = std::make_unique< CSimplePool >(*gpResourceFactory);
    frame = LoadFrame(Style().frame, *pool);
    if (frame == nullptr) {
      failed = true;
      pool.reset();
      return;
    }
    text = static_cast< CGuiTextPane* >(frame->FindWidget(Style().text));
    camera = frame->GetFrameCamera();
    if (text == nullptr || camera == nullptr) {
      std::fprintf(stderr, "[gameui] %s has no %s or camera\n", Style().frame, Style().text);
      failed = true;
      return;
    }
    cameraHome = camera->GetLocalPosition();
    textHome = text->GetLocalPosition();
    Scale();
  }

  // The scale: the frame's own camera sees more (a wider field of view, a larger orthographic
  // view) about its centre, so everything draws smaller, and ViewHalf, which reads the same
  // parameters, places it where it was asked to go. The camera is this element's own.
  void Scale() {
    const float scale = spec.scale > 0.f ? spec.scale : 1.f;
    if (scale == 1.f) {
      return;
    }
    CGuiCamera* cam = static_cast< CGuiCamera* >(camera);
    CGuiCamera::UCameraParms parms = cam->GetParms();
    if (cam->IsPerspective()) {
      const float halfTan = std::tan(0.5f * parms.mPerspective.mFov * 3.14159265f / 180.f) / scale;
      parms.mPerspective.mFov = 2.f * std::atan(halfTan) * 180.f / 3.14159265f;
    } else {
      auto widen = [scale](float& a, float& b) {
        const float mid = 0.5f * (a + b);
        a = mid + (a - mid) / scale;
        b = mid + (b - mid) / scale;
      };
      widen(parms.mOrthographic.mLeft, parms.mOrthographic.mRight);
      widen(parms.mOrthographic.mTop, parms.mOrthographic.mBottom);
    }
    cam->SetParms(parms);
  }

  bool Loaded() const {
    if (frame == nullptr || text == nullptr || !FrameLoaded(*frame) ||
        !text->TextSupport().GetIsTextSupportFinishedLoading()) {
      return false;
    }
    for (int i = 0; question && i < 2; ++i) {
      CGuiTextPane* pane = ChoicePane(i);
      if (pane != nullptr && !pane->TextSupport().GetIsTextSupportFinishedLoading()) {
        return false;
      }
    }
    return true;
  }

  // The parts it shows: its text, and for a box the line, the fill and the button.
  std::vector< CGuiWidget* > Parts() const {
    std::vector< CGuiWidget* > parts{text};
    if (textOnly) {
      return parts;
    }
    const StyleInfo& info = Style();
    if (question) {
      // The table, and with it the choices it holds.
      parts.push_back(frame->FindWidget(kQuestionTable));
    }
    parts.push_back(frame->FindWidget(info.line));
    if (spec.fill == Fill::Game && std::strcmp(info.fill, info.line) != 0) {
      parts.push_back(frame->FindWidget(info.fill));
    }
    if (spec.button) {
      for (const char* name : info.button) {
        if (name != nullptr) {
          parts.push_back(frame->FindWidget(name));
        }
      }
    }
    return parts;
  }

  // Only its parts show (the rest of the frame - a scan gauge, a history - is hidden), drawn over
  // the world without its depth, in the game's colours, with the text set.
  void Dress() {
    std::set< const CGuiObject* > parts, onPath;
    for (CGuiWidget* part : Parts()) {
      if (part == nullptr) {
        continue;
      }
      parts.insert(part);
      for (const CGuiObject* up = part; up != nullptr; up = up->GetParent()) {
        onPath.insert(up);
      }
    }
    for (CGuiWidget* w : WidgetsOf(*frame)) {
      // A part, what holds it, and what it holds (a button's glyph) show; the rest is hidden.
      bool shown = onPath.count(w) != 0;
      for (const CGuiObject* up = w->GetParent(); up != nullptr && !shown; up = up->GetParent()) {
        shown = parts.count(up) != 0;
      }
      w->SetIsVisible(shown);
      w->SetDepthTest(false);
    }
    CGuiTextSupport& support = text->TextSupport();
    support.SetTypeWriteEffectOptions(false, 0.f, 1.f);
    support.SetControlTXTRMap(&gpGameState->GameOptions().GetControlTXTRMap());
    if (question) {
      // The quit screen's own colours; its lines as given.
      support.SetWordWrap(false);
      for (int i = 0; i < 2; ++i) {
        if (CGuiTextPane* pane = ChoicePane(i)) {
          CGuiTextSupport& choice = pane->TextSupport();
          choice.SetTypeWriteEffectOptions(false, 0.f, 1.f);
          choice.SetWordWrap(false);
          choice.SetControlTXTRMap(&gpGameState->GameOptions().GetControlTXTRMap());
          choice.SetText(rstl::string_l(choices[i].c_str()));
        }
      }
      ColourChoices();
    } else {
      ColourParts(spec.style, *frame);
      support.SetFontColor(gpTweakGuiColors->GetHUDMemoTextForegroundColor());
      support.SetOutlineColor(gpTweakGuiColors->GetHUDMemoTextOutlineColor());
    }
    // Text alone isn't held to the message box's width: it stays on one line (a \n breaks it).
    if (textOnly) {
      support.SetWordWrap(false);
    }
    support.SetText(rstl::string_l(spec.text.c_str()));
    dressed = true;
  }

  CGuiTextPane* ChoicePane(int i) const {
    return frame != nullptr ? static_cast< CGuiTextPane* >(frame->FindWidget(kQuestionChoices[i])) : nullptr;
  }

  // The chosen one light, the other dark, as CQuitGameScreen::SetColors.
  void ColourChoices() {
    const CColor chosen(uchar(200), uchar(200), uchar(200), uchar(255));
    const CColor other(uchar(50), uchar(50), uchar(50), uchar(255));
    for (int i = 0; i < 2; ++i) {
      if (CGuiTextPane* pane = ChoicePane(i)) {
        pane->SetColor(i == selected ? chosen : other);
      }
    }
  }

  // The camera's view at the box's depth, in frame units: half its width and half its height, as
  // CGuiCamera::Draw sets it (the authored aspect widened to the window's, PortOverlay::AspectWiden:
  // wider fits the height, narrower the width).
  void ViewHalf(float& halfW, float& halfH) const {
    const CGuiCamera* cam = static_cast< const CGuiCamera* >(camera);
    const CGuiCamera::UCameraParms parms = cam->GetParms();
    const float widen = PortOverlay::AspectWiden();
    const float fitW = widen < 1.f ? 1.f : widen;
    const float fitH = widen < 1.f ? 1.f / widen : 1.f;
    if (cam->IsPerspective()) {
      const float depth = std::fabs(box.y - camera->GetWorldTransform().GetTranslation().GetY());
      halfH = depth * std::tan(0.5f * parms.mPerspective.mFov * 3.14159265f / 180.f) * fitH;
      halfW = depth * std::tan(0.5f * parms.mPerspective.mFov * 3.14159265f / 180.f) *
              parms.mPerspective.mAspect * fitW;
      return;
    }
    halfW = 0.5f * std::fabs(parms.mOrthographic.mRight - parms.mOrthographic.mLeft) * fitW;
    halfH = 0.5f * std::fabs(parms.mOrthographic.mTop - parms.mOrthographic.mBottom) * fitH;
  }

  // The window's aspect (width over height).
  static float ScreenAspect() {
    const CViewport& vp = CGraphics::GetViewport();
    return vp.mHeight > 0 ? static_cast< float >(vp.mWidth) / static_cast< float >(vp.mHeight) : 16.f / 9.f;
  }

  // The text's ink (where its letters are) in world space, from its bounds in the pane's text extents
  // (from the pane's top left, y down) about the pane's centre (its position moved by its scale centre).
  Extent TextInk() { return TextInk(text); }
  static Extent TextInk(CGuiTextPane* text) {
    CGuiTextSupport& support = text->TextSupport();
    const rstl::pair< CVector2i, CVector2i >& bounds = support.GetBounds();
    const float extentX = static_cast< float >(std::max(1, support.GetTextBoundingWidth()));
    const float extentY = static_cast< float >(std::max(1, support.GetTextBoundingHeight()));
    const float sx = text->GetWidth() * AxisScale(text->GetWorldTransform(), 0) / extentX;
    const float sz = text->GetHeight() * AxisScale(text->GetWorldTransform(), 2) / extentY;
    const CVector3f centre = text->GetWorldTransform() * text->GetScaleCenter();
    Extent ink;
    ink.Add(centre + CVector3f((bounds.first.GetX() - 0.5f * extentX) * sx, 0.f,
                               -(bounds.second.GetY() - 0.5f * extentY) * sz));
    ink.Add(centre + CVector3f((bounds.second.GetX() - 0.5f * extentX) * sx, 0.f,
                               -(bounds.first.GetY() - 0.5f * extentY) * sz));
    return ink;
  }

  // Sizes the box to its text (or the spec's size) and centres the text in it; for a text alone, its
  // ink is what is placed. Once: the models are its own, and the drawn ring patches them on the
  // first draw.
  void Shape() {
    if (question) {
      ShapeQuestion();
      return;
    }
    const Extent ink = TextInk();
    if (textOnly) {
      box = ink;
      shaped = true;
      return;
    }
    const StyleInfo& info = Style();
    CGuiWidget* fillWidget = frame->FindWidget(info.fill);
    Extent fill;
    AddWorldExtent(info.fitFill ? frame->FindWidget(info.line) : fillWidget, fill);
    box = fill;
    // Frame units per screen height, across and up.
    float halfW = 0.f, halfH = 0.f;
    ViewHalf(halfW, halfH);
    const float unitsPerHeightX = 2.f * halfW / ScreenAspect();
    const float unitsPerHeightZ = 2.f * halfH;

    const float textW = ink.x1 - ink.x0, textH = ink.z1 - ink.z0;
    const float wantHalf = spec.width > 0.f ? 0.5f * spec.width * unitsPerHeightX : 0.5f * textW + info.padX;
    const float wantHeight = spec.height > 0.f ? spec.height * unitsPerHeightZ : textH + 2.f * info.padZ;
    float growX = wantHalf - 0.5f * (fill.x1 - fill.x0);
    float growZ = wantHeight - (fill.z1 - fill.z0);

    // The button's half width keeps its notch; the line and fill move by the same change, limited by
    // the one that can give least, each in its own units (some frames scale their widgets: Grow).
    float notch = 0.f;
    if (info.button[0] != nullptr) {
      Extent e;
      AddWorldExtent(frame->FindWidget(info.button[0]), e);
      if (e.valid) {
        notch = 0.5f * (e.x1 - e.x0) * 1.1f;
      }
    }
    const Extent newFill = Grow(growX, growZ, notch);

    // The text in the middle of the fill.
    text->SetLocalPosition(textHome);
    const Extent at = TextInk();
    text->MoveInWorld(CVector3f(0.5f * (newFill.x0 + newFill.x1) - 0.5f * (at.x0 + at.x1), 0.f,
                                0.5f * (newFill.z0 + newFill.z1) - 0.5f * (at.z0 + at.z1)));

    // What is drawn: the line, the fill (shown or not, it is inside the line) and the button.
    Extent drawn;
    for (CGuiWidget* part : Parts()) {
      if (part != text) {
        AddWorldExtent(part, drawn);
      }
    }
    AddWorldExtent(fillWidget, drawn);
    box = drawn.valid ? drawn : newFill;
    shaped = true;
    std::fprintf(stderr, "[gameui] %s box: text %.2f x %.2f, box %.2f x %.2f frame units (asked %.2f, %.2f)\n",
                 info.frame, textW, textH, box.x1 - box.x0, box.z1 - box.z0, growX, growZ);
  }

  // Resizes the box's line and fill by (growX, growZ) world units, each model in its own units, as
  // far as their corners let them; returns the fill's new extent.
  Extent Grow(float growX, float growZ, float notch) {
    const StyleInfo& info = Style();
    struct Part {
      const CCubeModel* model;
      Zones zn;
      float sx, sz;
    };
    std::vector< Part > parts;
    // The line, then the fill: not when it is the line's own model (reshaped once) or is fitted to
    // the line afterwards (FitFill).
    const bool fillToo = std::strcmp(info.line, info.fill) != 0 && !info.fitFill;
    const char* const names[2] = {info.line, info.fill};
    for (int part = 0; part < (fillToo ? 2 : 1); ++part) {
      const char* name = names[part];
      CGuiWidget* w = frame->FindWidget(name);
      const CCubeModel* model = ModelOf(w);
      if (model == nullptr) {
        continue;
      }
      const float sx = AxisScale(w->GetWorldTransform(), 0);
      const float sz = AxisScale(w->GetWorldTransform(), 2);
      const Zones zn = PortGameUiMath::ZonesOf(DrawnPointsOf(model), notch / sx);
      if (zn.valid) {
        growX = std::max(growX, PortGameUiMath::MinGrowX(zn) * sx);
        growZ = std::max(growZ, PortGameUiMath::MinGrowZ(zn) * sz);
        parts.push_back(Part{model, zn, sx, sz});
      }
    }
    for (const auto& [model, zn, sx, sz] : parts) {
      uint* pos = const_cast< uint* >(static_cast< const uint* >(model->GetPositions()));
      const uint n = model->GetModelInstance().GetVertexSize() / (3 * sizeof(uint));
      for (uint i = 0; i < n; ++i) {
        const Point q = PortGameUiMath::Reshape({ReadFloat(pos, i * 3), ReadFloat(pos, i * 3 + 2)}, zn,
                                                growX / sx, growZ / sz);
        WriteFloat(pos, i * 3, q.x);
        WriteFloat(pos, i * 3 + 2, q.z);
      }
      model->PortUpdateResidentData();
    }
    if (info.fitFill) {
      return FitFill();
    }
    Extent fill;
    AddWorldExtent(frame->FindWidget(info.fill), fill);
    return fill;
  }

  // A fill that isn't centred on its line: resized to the line, less a little, and moved to its
  // middle; the line's extent is what the box is.
  Extent FitFill() {
    const StyleInfo& info = Style();
    Extent line, fill;
    AddWorldExtent(frame->FindWidget(info.line), line);
    CGuiWidget* w = frame->FindWidget(info.fill);
    const CCubeModel* model = ModelOf(w);
    AddWorldExtent(w, fill);
    if (!line.valid || !fill.valid || model == nullptr) {
      return line;
    }
    constexpr float kInset = 0.25f; // world units inside the line on each side
    const float sx = AxisScale(w->GetWorldTransform(), 0);
    const float sz = AxisScale(w->GetWorldTransform(), 2);
    // The fill is off its own origin (the map's runs on under its right-hand pane), so the corner
    // zones Reshape finds from |x| do not hold: each half moves out by what that side lacks, about the
    // model's own middle, which keeps a chamfered corner's shape.
    const std::vector< Point > drawn = DrawnPointsOf(model);
    if (!drawn.empty() && sx > 0.f && sz > 0.f) {
      float x0 = drawn[0].x, x1 = x0, z0 = drawn[0].z, z1 = z0;
      for (const Point& p : drawn) {
        x0 = std::min(x0, p.x);
        x1 = std::max(x1, p.x);
        z0 = std::min(z0, p.z);
        z1 = std::max(z1, p.z);
      }
      const float midX = 0.5f * (x0 + x1), midZ = 0.5f * (z0 + z1);
      const float growX = (0.5f * ((line.x1 - line.x0) - (fill.x1 - fill.x0)) - kInset) / sx;
      const float growZ = (0.5f * ((line.z1 - line.z0) - (fill.z1 - fill.z0)) - kInset) / sz;
      uint* pos = const_cast< uint* >(static_cast< const uint* >(model->GetPositions()));
      const uint n = model->GetModelInstance().GetVertexSize() / (3 * sizeof(uint));
      for (uint i = 0; i < n; ++i) {
        const float x = ReadFloat(pos, i * 3), z = ReadFloat(pos, i * 3 + 2);
        WriteFloat(pos, i * 3, x + (x >= midX ? growX : -growX));
        WriteFloat(pos, i * 3 + 2, z + (z >= midZ ? growZ : -growZ));
      }
      model->PortUpdateResidentData();
    }
    Extent fitted;
    AddWorldExtent(w, fitted);
    if (fitted.valid) {
      w->MoveInWorld(CVector3f(0.5f * (line.x0 + line.x1) - 0.5f * (fitted.x0 + fitted.x1), 0.f,
                               0.5f * (line.z0 + line.z1) - 0.5f * (fitted.z0 + fitted.z1)));
    }
    return line;
  }

  // A Question: the box sized to its title over its row of choices, the title at the top, the
  // choices under it, both centred across.
  void ShapeQuestion() {
    const StyleInfo& info = Style();
    CGuiWidget* table = frame->FindWidget(kQuestionTable);
    CGuiTextPane* panes[2] = {ChoicePane(0), ChoicePane(1)};
    if (table == nullptr || panes[0] == nullptr || panes[1] == nullptr) {
      std::fprintf(stderr, "[gameui] %s has no choices\n", info.frame);
      failed = true;
      return;
    }
    const Extent title = TextInk();
    Extent row;
    for (CGuiTextPane* pane : panes) {
      const Extent e = TextInk(pane);
      row.Add(CVector3f(e.x0, e.y, e.z0));
      row.Add(CVector3f(e.x1, e.y, e.z1));
    }
    Extent fill;
    AddWorldExtent(frame->FindWidget(info.fill), fill);
    const float wantW = std::max(title.x1 - title.x0, row.x1 - row.x0) + 2.f * info.padX;
    const float wantH = (title.z1 - title.z0) + kQuestionGap + (row.z1 - row.z0) + 2.f * info.padZ;
    const Extent grown = Grow(0.5f * (wantW - (fill.x1 - fill.x0)), wantH - (fill.z1 - fill.z0), 0.f);
    const float midX = 0.5f * (grown.x0 + grown.x1);
    // The block of title and choices centred up and down in the fill (it may have stopped bigger).
    const float blockH = (title.z1 - title.z0) + kQuestionGap + (row.z1 - row.z0);
    const float top = 0.5f * (grown.z0 + grown.z1) + 0.5f * blockH;
    text->MoveInWorld(CVector3f(midX - 0.5f * (title.x0 + title.x1), 0.f, top - title.z1));
    const float rowTop = top - (title.z1 - title.z0) - kQuestionGap;
    table->MoveInWorld(CVector3f(midX - 0.5f * (row.x0 + row.x1), 0.f, rowTop - row.z1));
    for (int i = 0; i < 2; ++i) {
      choiceInk[i] = TextInk(panes[i]);
    }
    Extent drawn;
    AddWorldExtent(frame->FindWidget(info.line), drawn);
    AddWorldExtent(frame->FindWidget(info.fill), drawn);
    box = drawn.valid ? drawn : grown;
    shaped = true;
    std::fprintf(stderr, "[gameui] %s question: title %.2f x %.2f, choices %.2f x %.2f, box %.2f x %.2f\n",
                 info.frame, title.x1 - title.x0, title.z1 - title.z0, row.x1 - row.x0, row.z1 - row.z0,
                 box.x1 - box.x0, box.z1 - box.z0);
  }

  // Moves the frame's camera so the box sits at its placement, and notes where it lands.
  void Place() {
    float viewW = 0.f, viewH = 0.f;
    ViewHalf(viewW, viewH);
    if (!(viewW > 0.f) || !(viewH > 0.f)) {
      return;
    }
    const float aspect = ScreenAspect();
    // In screen heights, + right and + down from the screen's centre, with the camera at home: the
    // screen is `aspect` heights across, and the view's width and height fill it.
    camera->SetLocalPosition(cameraHome);
    const CVector3f eye = camera->GetWorldTransform().GetTranslation();
    const float scaleX = 0.5f * aspect / viewW, scaleZ = 0.5f / viewH;
    const float bx0 = (box.x0 - eye.GetX()) * scaleX, bx1 = (box.x1 - eye.GetX()) * scaleX;
    const float by0 = -(box.z1 - eye.GetZ()) * scaleZ, by1 = -(box.z0 - eye.GetZ()) * scaleZ;
    const float halfW = 0.5f * aspect;
    // Where the anchor wants the box's own anchor point: an anchored edge's offset is a margin in
    // from that edge of the screen, a centred axis's is + right / + down.
    const Placement& pl = spec.placement;
    float wantX = pl.x, haveX = 0.5f * (bx0 + bx1);
    float wantY = pl.y, haveY = 0.5f * (by0 + by1);
    switch (pl.anchor) {
    case Anchor::TopLeft:
    case Anchor::Left:
    case Anchor::BottomLeft:
      wantX = -halfW + pl.x;
      haveX = bx0;
      break;
    case Anchor::TopRight:
    case Anchor::Right:
    case Anchor::BottomRight:
      wantX = halfW - pl.x;
      haveX = bx1;
      break;
    default:
      break;
    }
    switch (pl.anchor) {
    case Anchor::TopLeft:
    case Anchor::Top:
    case Anchor::TopRight:
      wantY = -0.5f + pl.y;
      haveY = by0;
      break;
    case Anchor::BottomLeft:
    case Anchor::Bottom:
    case Anchor::BottomRight:
      wantY = 0.5f - pl.y;
      haveY = by1;
      break;
    default:
      break;
    }
    const float dx = wantX - haveX, dy = wantY - haveY;
    // Moving the camera one way moves the box the other way on screen.
    camera->SetLocalPosition(cameraHome + CVector3f(-dx / scaleX, 0.f, dy / scaleZ));
    const auto toScreen = [&](const Extent& e) {
      ScreenRect r;
      r.valid = e.valid;
      r.x0 = ((e.x0 - eye.GetX()) * scaleX + dx + halfW) / aspect;
      r.x1 = ((e.x1 - eye.GetX()) * scaleX + dx + halfW) / aspect;
      r.y0 = -(e.z1 - eye.GetZ()) * scaleZ + dy + 0.5f;
      r.y1 = -(e.z0 - eye.GetZ()) * scaleZ + dy + 0.5f;
      return r;
    };
    rect = toScreen(box);
    if (question) {
      choiceRect[0] = toScreen(choiceInk[0]);
      choiceRect[1] = toScreen(choiceInk[1]);
    }
  }

  void SetText(const std::string& value) {
    if (value == spec.text) {
      return;
    }
    spec.text = value;
    // The models were resized in place for the old text: a box sized to its text starts from a fresh
    // frame; one of a set size, and a text alone, only take the new words.
    if (question || (!textOnly && (spec.width <= 0.f || spec.height <= 0.f))) {
      Unload();
    } else {
      dressed = false;
      shaped = false;
      if (text != nullptr) {
        text->SetLocalPosition(textHome);
      }
    }
  }

  void Draw() {
    if (failed) {
      return;
    }
    Load();
    if (frame == nullptr || !Loaded()) {
      return;
    }
    if (!dressed) {
      Dress();
    }
    // Updated each draw (the text's layout, the widgets' animations), at the real time between draws.
    const auto now = std::chrono::steady_clock::now();
    const float dt = last.time_since_epoch().count() == 0
                         ? 1.f / 60.f
                         : std::clamp(std::chrono::duration< float >(now - last).count(), 0.f, 0.1f);
    last = now;
    frame->Update(dt);
    if (!shaped) {
      // A text with an image in it (a button's glyph) has no layout until the image is in: shaped
      // then, it would be measured as empty and placed off. Wait for it (a second at most).
      const rstl::pair< CVector2i, CVector2i >& bounds = text->TextSupport().GetBounds();
      const bool laidOut = spec.text.empty() || bounds.second.GetX() > bounds.first.GetX();
      if (!laidOut && ++layoutWaits < 60) {
        return;
      }
      Shape();
    }
    Place();
    if (alpha > 0.f) {
      frame->Draw(CGuiWidgetDrawParms(alpha, CVector3f::Zero()));
    }
  }
};

Box::Box(const BoxSpec& spec) : mImpl(std::make_unique< Impl >(spec, false)) {}
Box::~Box() = default;
void Box::SetText(const std::string& text) { mImpl->SetText(text); }
void Box::SetAlpha(float alpha) { mImpl->alpha = std::clamp(alpha, 0.f, 1.f); }
void Box::SetPlacement(const Placement& placement) { mImpl->spec.placement = placement; }
void Box::Draw() {
  ReapHeld();
  mImpl->Draw();
}
bool Box::Ready() const { return mImpl->shaped; }
bool Box::Failed() const { return mImpl->failed; }
ScreenRect Box::LastRect() const { return mImpl->rect; }

namespace {
// A text alone is drawn from the message box's frame, every part but its text hidden.
BoxSpec TextBoxSpec(const TextSpec& spec) {
  BoxSpec box;
  box.style = Style::Message;
  box.fill = Fill::None;
  box.button = false;
  box.text = spec.text;
  box.scale = spec.scale;
  box.placement = spec.placement;
  return box;
}
} // namespace

Text::Text(const TextSpec& spec) : mImpl(std::make_unique< Box::Impl >(TextBoxSpec(spec), true)) {}
Text::~Text() = default;
void Text::SetText(const std::string& text) { mImpl->SetText(text); }
void Text::SetAlpha(float alpha) { mImpl->alpha = std::clamp(alpha, 0.f, 1.f); }
void Text::SetPlacement(const Placement& placement) { mImpl->spec.placement = placement; }
void Text::Draw() {
  ReapHeld();
  mImpl->Draw();
}
bool Text::Ready() const { return mImpl->shaped; }
ScreenRect Text::LastRect() const { return mImpl->rect; }

Question::Question(const QuestionSpec& spec) {
  BoxSpec box;
  box.text = spec.title;
  box.scale = spec.scale;
  box.placement = spec.placement;
  mImpl = std::make_unique< Box::Impl >(box, false);
  mImpl->question = true;
  mImpl->choices[0] = spec.choices[0];
  mImpl->choices[1] = spec.choices[1];
  mImpl->selected = std::clamp(spec.selected, 0, 1);
}
Question::~Question() = default;
void Question::SetTitle(const std::string& title) { mImpl->SetText(title); }
void Question::SetSelected(int choice) {
  choice = std::clamp(choice, 0, 1);
  if (choice != mImpl->selected) {
    mImpl->selected = choice;
    if (mImpl->dressed) {
      mImpl->ColourChoices();
    }
  }
}
int Question::Selected() const { return mImpl->selected; }
void Question::SetAlpha(float alpha) { mImpl->alpha = std::clamp(alpha, 0.f, 1.f); }
void Question::SetPlacement(const Placement& placement) { mImpl->spec.placement = placement; }
void Question::Draw() {
  ReapHeld();
  mImpl->Draw();
}
bool Question::Ready() const { return mImpl->shaped; }
bool Question::Failed() const { return mImpl->failed; }
ScreenRect Question::LastRect() const { return mImpl->rect; }
ScreenRect Question::ChoiceRect(int choice) const { return mImpl->choiceRect[std::clamp(choice, 0, 1)]; }

namespace Dev {
namespace {
// Not unique_ptrs: one still shown when the program exits is left to the OS (see the header).
Box* sBox = nullptr;
Text* sText = nullptr;
Question* sQuestion = nullptr;

// A frame waiting to be dumped, in a pool of its own; never freed (see the header).
struct PendingDump {
  std::string name;
  CGuiFrame* frame;
};
std::vector< PendingDump > sDumps;

void Dump(CGuiFrame& frame, const std::string& name) {
  std::fprintf(stderr, "[gameui] dump %s\n", name.c_str());
  CGuiWidgetIdDB& db = frame.WidgetIdDB();
  for (int id = 0; id < db.PortCount(); ++id) {
    CGuiWidget* w = frame.FindWidget(db.PortName(id));
    if (w == nullptr) {
      continue;
    }
    const uint t = w->GetWidgetTypeID();
    const CVector3f p = w->GetWorldTransform().GetTranslation();
    std::fprintf(stderr, "[gameui]   %-30s %c%c%c%c at (%.2f %.2f %.2f)%s scale %.2f x %.2f\n",
                 db.PortName(id).data(), char(t >> 24), char(t >> 16), char(t >> 8), char(t), p.GetX(),
                 p.GetY(), p.GetZ(), w->GetIsVisible() ? "" : " hidden", AxisScale(w->GetWorldTransform(), 0),
                 AxisScale(w->GetWorldTransform(), 2));
    if (t == 'TXPN') {
      const CGuiTextPane* pane = static_cast< const CGuiTextPane* >(w);
      std::fprintf(stderr, "[gameui]     text pane %.2f x %.2f\n", pane->GetWidth(), pane->GetHeight());
    }
    const CCubeModel* model = ModelOf(w);
    if (model == nullptr) {
      continue;
    }
    const std::vector< Point > points = DrawnPointsOf(model);
    const Zones zn = PortGameUiMath::ZonesOf(points, 0.f);
    std::fprintf(stderr,
                 "[gameui]     %zu vertices drawn: |x| to %.2f (corners from %.2f), z %.2f..%.2f (top band from %.2f, "
                 "bottom band to %.2f)%s\n",
                 points.size(), zn.outer, zn.cornerFrom, zn.bottom, zn.top, zn.topFrom, zn.bottomTo,
                 zn.valid ? "" : ", no zones (one cluster)");
  }
}
} // namespace

void ShowBox(const BoxSpec& spec) {
  Hide();
  sBox = new Box(spec);
  sBox->SetAlpha(1.f);
}

void ShowText(const TextSpec& spec) {
  Hide();
  sText = new Text(spec);
  sText->SetAlpha(1.f);
}

void ShowQuestion(const QuestionSpec& spec) {
  Hide();
  sQuestion = new Question(spec);
  sQuestion->SetAlpha(1.f);
}

void Select(int choice) {
  if (sQuestion != nullptr) {
    sQuestion->SetSelected(choice);
  }
}

void Hide() {
  delete sBox;
  sBox = nullptr;
  delete sText;
  sText = nullptr;
  delete sQuestion;
  sQuestion = nullptr;
}

void DumpFrame(const std::string& frameName) {
  if (gpResourceFactory == nullptr) {
    return;
  }
  CSimplePool* pool = new CSimplePool(*gpResourceFactory);
  if (CGuiFrame* frame = LoadFrame(frameName.c_str(), *pool)) {
    sDumps.push_back(PendingDump{frameName, frame});
  }
}

void Draw() {
  for (auto it = sDumps.begin(); it != sDumps.end();) {
    if (!FrameLoaded(*it->frame)) {
      ++it;
      continue;
    }
    Dump(*it->frame, it->name);
    it = sDumps.erase(it);
  }
  if (sBox != nullptr) {
    sBox->Draw();
  }
  if (sText != nullptr) {
    sText->Draw();
  }
  if (sQuestion != nullptr) {
    sQuestion->Draw();
  }
}
} // namespace Dev

} // namespace PortGameUi
