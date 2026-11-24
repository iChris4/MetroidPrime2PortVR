// The play options, apart from any menu (include/port_play_options.h).
#include "port_play_options.h"

#include "port_overlay_internal.h"
#include "port_photo.h"
#include "port_third_person.h"

#include "MetroidPrime/Player/CGameOptions.hpp"
#include "MetroidPrime/Player/CGameState.hpp"

#include <iterator>

namespace PortPlayOptions {
namespace {
// In Option's order.
constexpr Switch kSwitches[] = {
    {"Hide helmet", "Hides the helmet's frame around the view",
     "Draws the view without the helmet's frame round it.", PortOverlay::HideHelmet, PortOverlay::SetHideHelmet},
    {"Hide visor effects", "Hides the visor's own effects",
     "The scan visor's window and edges are what a visor effect draws here.", PortOverlay::HideVisorEffects,
     PortOverlay::SetHideVisorEffects},
    {"Third-person camera", "Play from behind Samus's shoulder",
     "Plays from behind Samus's shoulder (single player; not the morph ball or cutscenes),\n"
     "showing her body and hiding the arm cannon and helmet. Distance, shoulder and height:\n"
     "console `thirdperson`, or third_person_* in the settings file.",
     PortThirdPerson::Enabled,
     [](bool on) {
       PortThirdPerson::SetEnabled(on);
       PortOverlaySettings::MarkDirty();
     }},
    {"On-screen in-game time", "Shows the game's play time on screen",
     "Shows the game's own play time at the top of the screen, as a speedrun timer.", PortOverlay::IgtTimer,
     PortOverlay::SetIgtTimer},
};
static_assert(std::size(kSwitches) == static_cast< size_t >(Option::Count));

bool HintsOn() { return gpGameState != nullptr && gpGameState->GameOptions().GetIsHintSystemEnabled(); }

bool InGame() {
  double seconds = 0.0;
  return PortOverlay::GameTime(seconds);
}

constexpr Action kActions[] = {
    {"Show current hint", "Shows the hint the game would show next", "Show", nullptr,
     [] { return PortOverlay::RequestShowHint(), true; }, nullptr, true, HintsOn,
     "Hints are off in the game's options"},
    {"Photo mode", "A free camera for pictures, the game paused", "Enter", nullptr,
     [] { return PortPhoto::RequestEnter(), true; }, nullptr, true},
    {"Return to title", "Quit to the title screen", "Quit", nullptr,
     [] { return PortOverlay::RequestReturnToTitle(), true; },
     "Return to the title?\n&push;&main-color=#8A8A8AFF;Progress since your last save is lost&pop;", true, InGame},
    {"Exit game", "Close the game", "Exit", nullptr, [] { return PortOverlay::RequestQuit(), true; },
     "Exit the game?\n&push;&main-color=#8A8A8AFF;Progress since your last save is lost&pop;", true},
};
} // namespace

const Switch& Get(Option option) { return kSwitches[static_cast< int >(option)]; }

std::span< const Switch > Switches() { return kSwitches; }

std::span< const Action > Actions() { return kActions; }
} // namespace PortPlayOptions
