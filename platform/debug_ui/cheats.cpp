// F1 overlay: The Cheats page, the Prime 1 port's debug_ui/cheats.cpp for this game: items,
// health, teleport and the room's layers. State and helpers shared with the other parts are in
// debug_ui_internal.h.

#include "port_mobile.h"
#include "debug_ui_internal.h"

#include "port_cheats.h"
#include "port_overlay.h"
#include "port_collision_view.h"
#include "port_savestate.h"
#include "port_warp.h"

#include <aurora/gfx.h>

#include "Kyoto/Text/CStringTable.hpp"
#include "MetroidPrime/CHealthInfo.hpp"
#include "MetroidPrime/CMemoryCard.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <iterator>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace PortDebug {

namespace {

using PortCheats::GrantItem;
using SItemCheck = PortCheats::Item;

// A slider takes the value under the finger the moment it is pressed, before
// UpdateTouchScroll can tell a scroll from a drag, so scrolling the page past
// these sliders filled Samus's ammo and tanks. The value is held back until
// release and dropped when the drag became a scroll.
template <typename T>
bool CheatSlider(const char* label, T& value, T min, T max, const char* format = nullptr) {
  static_assert(std::is_same_v<T, int> || std::is_same_v<T, float>);
  static ImGuiID sEditing = 0;
  static T sPending = 0;
  const ImGuiID id = ImGui::GetID(label);
  T shown = sEditing == id ? sPending : value;
  ImGui::SliderScalar(label, std::is_same_v<T, float> ? ImGuiDataType_Float : ImGuiDataType_S32, &shown,
                      &min, &max, format);
  if (ImGui::IsItemActive()) {
    sEditing = id;
    sPending = shown;
    return false;
  }
  if (sEditing != id) {
    return false;
  }
  sEditing = 0;
  if (TouchScrolling() || sPending == value) {
    return false;
  }
  value = sPending;
  return true;
}

bool CheatSlider(const char* label, int& value, int max) { return CheatSlider(label, value, 0, max); }

// The toggles as a grid, as many columns as the widest name leaves room for.
void DrawItemChecks(CPlayerState& ps, const char* tableId, const SItemCheck* items, int count) {
  float nameWidth = 0.f;
  for (int i = 0; i < count; ++i) {
    nameWidth = std::max(nameWidth, ImGui::CalcTextSize(items[i].name).x);
  }
  nameWidth += ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x +
               ImGui::GetStyle().ItemSpacing.x * 2.f;
  const int columns = std::clamp(int(ImGui::GetContentRegionAvail().x / nameWidth), 1, 4);
  if (ImGui::BeginTable(tableId, columns)) {
    for (int i = 0; i < count; ++i) {
      ImGui::TableNextColumn();
      bool owned = ps.HasPowerUp(items[i].type);
      if (ImGui::Checkbox(items[i].name, &owned)) {
        GrantItem(ps, items[i].type, owned ? 1 : 0, owned ? 1 : 0);
      }
    }
    ImGui::EndTable();
  }
}

} // namespace

// PrimedGun Echoes: the Full health and Grant everything buttons, also run from the VR menu's DEBUG
// tab (platform/vr/vr_host_echoes.cpp). Game thread only.
void CheatFullHealth() { PortCheats::FullHealth(); }

void CheatGrantEverything() { PortCheats::GrantEverything(); }

// The switches and readouts for working on what mods draw: the Collision view (port_collision_view.h)
// and the frame statistics. MP1's PBR / room-geometry counters have no counterpart in this port.
void DrawRendering() {
  static const char* const kCollisionModes[] = {"off", "on top of the world", "only (hide the world)"};
  int collision = int(PortCollisionView::GetMode());
  if (ImGui::Combo("Collision", &collision, kCollisionModes, 3)) {
    PortCollisionView::SetMode(PortCollisionView::Mode(collision));
  }
  ImGui::SetItemTooltip("Recommended: off. Not saved.\n"
                        "Draws what Samus collides with: walls grey, floors blue, ceilings red,\n"
                        "lava orange, phazon cyan, grates yellow, solid objects as orange boxes.");

  if (ImGui::CollapsingHeader("Frame statistics")) {
    if (const AuroraStats* stats = aurora_get_stats()) {
      ImGui::Text("%.0f fps, %u draws (%u merged), %u passes", aurora_get_fps(), stats->drawCallCount,
                  stats->mergedDrawCallCount, stats->renderPassCount);
      ImGui::Text("vertices %.1f MiB, indices %.1f, arrays %.1f, uniforms %.1f, texture uploads %.1f",
                  stats->lastVertSize / 1048576.f, stats->lastIndexSize / 1048576.f,
                  stats->lastStorageSize / 1048576.f, stats->lastUniformSize / 1048576.f,
                  stats->lastTextureUploadSize / 1048576.f);
      if (const uint32_t resident = aurora_get_resident_geometry_mib()) {
        ImGui::Text("kept on the GPU %.1f of %u MiB", aurora_get_resident_geometry_used() / 1048576.f, resident);
      }
      ImGui::Text("pipelines %u made, %u waiting", stats->createdPipelines, stats->queuedPipelines);
    }
    AuroraTextureStats textures{};
    aurora_get_texture_stats(&textures);
    ImGui::Text("textures %u, %.0f MiB; render targets %u, %.0f MiB; lighting %u, %.0f MiB", textures.count[0],
                textures.bytes[0] / 1048576.f, textures.count[1], textures.bytes[1] / 1048576.f, textures.count[2],
                textures.bytes[2] / 1048576.f);
  }
}

namespace {
void CheatCheckbox(PortCheats::Cheat id, bool inlineHelp = false) { EntryCheckbox(PortCheats::Get(id), inlineHelp); }
} // namespace

void DrawCheatsSection() {
  using PortCheats::Cheat;
  CheatCheckbox(Cheat::GodMode, true);
  CheatCheckbox(Cheat::NoDarkAetherDamage, true);
  CheatCheckbox(Cheat::UnlockBeamDoors, true);
  CheatCheckbox(Cheat::StackBeams);
  ImGui::SameLine();
  ImGui::BeginDisabled(!PortOverlay::StackBeamsSet());
  CheatCheckbox(Cheat::StackBeamsSkipDark);
  ImGui::EndDisabled();
  CheatCheckbox(Cheat::MoonJump);
  ImGui::BeginDisabled(PortOverlay::OriginalExperience());
  float boostSpeed = PortOverlay::BoostSpeedScaleSet();
  float boostDuration = PortOverlay::BoostDurationScaleSet();
  bool boostChanged = CheatSlider("Boost Ball speed", boostSpeed, 1.f, PortOverlay::kBoostScaleMax, "%.1fx");
  ImGui::SetItemTooltip("Multiplies the Boost Ball's push. 1x is the game's. Not saved.");
  boostChanged |=
      CheatSlider("Boost Ball duration", boostDuration, 1.f, PortOverlay::kBoostScaleMax, "%.1fx");
  ImGui::SetItemTooltip("Multiplies how long the ball stays boosted and how slowly it loses speed meanwhile. "
                        "The boost still ends on leaving the ground or hitting a wall. 1x is the game's. "
                        "Not saved.");
  if (boostChanged) {
    PortOverlay::SetBoostScales(boostSpeed, boostDuration);
  }
  ImGui::EndDisabled();
  CheatCheckbox(Cheat::BoostAnywhere);
  ImGui::SameLine();
  CheatCheckbox(Cheat::BoostSteer);
  CheatCheckbox(Cheat::PhazonShot);

  CPlayerState* ps = PortCheats::PlayerInGame();
  if (ps == nullptr) {
    ImGui::TextDisabled("Waiting for gameplay...");
    return;
  }
  CHealthInfo* health = ps->HealthInfo();
  ImGui::Text("Health: %.0f / %.0f", health->GetHP(), ps->CalculateHealth());
  if (ImGui::Button("Full health")) {
    PortCheats::FullHealth();
  }
  ImGui::SameLine();
  if (ImGui::Button("Grant everything")) {
    PortCheats::GrantEverything();
  }

  ImGui::SeparatorText("Abilities");
  DrawItemChecks(*ps, "##abilitychecks", PortCheats::Abilities().data(), int(PortCheats::Abilities().size()));
  if (ImGui::CollapsingHeader("Keys")) {
    DrawItemChecks(*ps, "##keychecks", PortCheats::Keys().data(), int(PortCheats::Keys().size()));
  }

  const auto counter = [ps](const char* label, CPlayerState::EItemType type) {
    const int max = CPlayerState::PortPowerUpMaxValue(type);
    int amount = std::min(ps->GetItemAmount(type, false), max);
    // A fixed width, so the label sits next to its slider instead of at the page's right edge.
    ImGui::SetNextItemWidth(220.f);
    if (CheatSlider(label, amount, max)) {
      // The capacity only rises as far as the amount needs (the HUD draws capacity, so the
      // maximum would show a full expansion whatever the value), and the amount is set
      // outright: a slide down must take ammo away even though the capacity has room.
      GrantItem(*ps, type, std::max(amount, ps->GetItemCapacity(type)), amount);
    }
    ImGui::SameLine();
    ImGui::PushID(label);
    ImGui::CheckboxFlags("Infinite", &sInfiniteAmmo, PortOverlay::InfiniteAmmoKind(type));
    ImGui::PopID();
    ItemHelp("Firing does not use this ammo up, even at zero, and the HUD shows infinity. The real count "
             "is kept and pickups still add to it. Not saved.");
  };
  counter("Missiles", CPlayerState::kIT_Missile);
  counter("Power Bombs", CPlayerState::kIT_Powerbomb);
  counter("Dark Ammo", CPlayerState::kIT_DarkAmmo);
  counter("Light Ammo", CPlayerState::kIT_LightAmmo);
  // Ammo costs and expansion size (#27). Each has a Reset button: a slider is hard to land on
  // an exact value with a finger.
  {
    int comboBeam = PortOverlay::ComboBeamCost();
    ImGui::SetNextItemWidth(220.f);
    if (ImGui::SliderInt("Combo beam ammo cost", &comboBeam, 0, PortOverlay::kComboBeamCostMax,
                         "%d", ImGuiSliderFlags_AlwaysClamp)) {
      PortOverlay::SetComboBeamCost(comboBeam);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##combobeamcost")) {
      PortOverlay::SetComboBeamCost(PortOverlay::kComboBeamCostMax);
    }
    ItemHelp("Dark or Light ammo a charge combo uses (the Annihilator Beam uses it from both). "
             "Reset: 30, the original cost. Saved.");
    int comboMissile = PortOverlay::ComboMissileCost();
    ImGui::SetNextItemWidth(220.f);
    if (ImGui::SliderInt("Combo missile cost", &comboMissile, 0, PortOverlay::kComboMissileCostMax,
                         "%d", ImGuiSliderFlags_AlwaysClamp)) {
      PortOverlay::SetComboMissileCost(comboMissile);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##combomissilecost")) {
      PortOverlay::SetComboMissileCost(PortOverlay::kComboMissileCostMax);
    }
    ItemHelp("Missiles a charge combo uses. At 0 a combo needs no missile. Reset: 5, the original "
             "cost. Saved.");
    int perExpansion = PortOverlay::BeamAmmoPerExpansion();
    ImGui::SetNextItemWidth(220.f);
    if (ImGui::SliderInt("Beam ammo per expansion", &perExpansion,
                         PortOverlay::kBeamAmmoPerExpansionMin, PortOverlay::kBeamAmmoPerExpansionMax,
                         "%d", ImGuiSliderFlags_AlwaysClamp)) {
      PortOverlay::SetBeamAmmoPerExpansion((perExpansion + 5) / 10 * 10);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##beamammoperexpansion")) {
      PortOverlay::SetBeamAmmoPerExpansion(PortOverlay::kBeamAmmoPerExpansionDefault);
    }
    ItemHelp("Dark and Light ammo each Ammo Expansion adds, and the matching ammo it fills; the "
             "maximum is 5 expansions' worth (250 at 50, 500 at 100). Reset: 50, the original. "
             "Lowering it only lowers the cap: ammo above it is trimmed the next time you pick "
             "some up. Saves keep working: the extra above 250 is stored beside the card "
             "(beam_ammo.sidecar), so a save without it loads at 250. Saved.");
  }
  // Energy tanks are the one counter that also moves the health bar.
  {
    const int max = CPlayerState::GetPowerUpMaxValue(CPlayerState::kIT_EnergyTanks);
    int tanks = std::min(ps->GetItemAmount(CPlayerState::kIT_EnergyTanks, false), max);
    ImGui::SetNextItemWidth(220.f);
    if (CheatSlider("Energy Tanks", tanks, max)) {
      GrantItem(*ps, CPlayerState::kIT_EnergyTanks, tanks, tanks); // tanks have no ammo: count is capacity
      health->SetHP(ps->CalculateHealth());
    }
  }
  ItemHelp("The items go into the save in memory: the next save on the memory card writes them, "
           "and nothing here is kept across a restart on its own.");
  ImGui::SeparatorText("Teleport");
  // The warps' own buttons (port_cheats.h), as the special menu's Warp page lists them; Reload
  // sits with the room lists below.
  using PortCheats::Warp;
  EntryButton(PortCheats::Get(Warp::PreviousArea));
  ImGui::SameLine();
  EntryButton(PortCheats::Get(Warp::NextArea));
  ImGui::SameLine();
  EntryButton(PortCheats::Get(Warp::PreviousWorld));
  ImGui::SameLine();
  EntryButton(PortCheats::Get(Warp::NextWorld));
  // The room's twin in the other dimension, where Samus stands (PortWarp::Dimension). The game says
  // whether there was one a frame later; the line stays until the next press.
  static int sDimensionResult = 0;
  if (EntryButton(PortCheats::Get(Warp::Dimension))) {
    sDimensionResult = 0;
  }
  if (const int result = PortWarp::TakeDimensionResult(); result != 0) {
    sDimensionResult = result;
  }
  if (sDimensionResult < 0) {
    ImGui::SameLine();
    ImGui::TextDisabled("No twin room in the other dimension");
  }
  // The Prime 1 port's Teleport block (debug_ui/cheats.cpp): a filtered list of the current
  // world's rooms and the save's world table, both by name. The requests are the same PortWarp
  // ones the console's `warp`/`world` verbs and MP2_TELEPORT use, so nothing here walks around
  // what the game already does.
  if (sRooms.empty()) {
    ImGui::TextDisabled("(no world yet)");
  } else {
    const int areaCount = static_cast< int >(sRooms.size());
    const int current = sAreaId;
    // Between two updates the published ids are the same frame's, so this only guards a world
    // that is arriving: an area index outside the table is not looked up.
    const bool currentValid = current >= 0 && current < areaCount;
    const auto roomLabel = [](int index) { return sRooms[index].name; };
    const auto roomExists = [](int index) { return sRooms[index].real; };
    const auto goToRoom = [](int index) {
      char text[16];
      std::snprintf(text, sizeof(text), "%d", index);
      PortWarp::Area(text);
    };

    // One list per dimension, split by each area's map (SRoomEntry::dimension); a room whose map
    // has not loaded yet, or that the map does not cover, is listed with Light Aether's. Cutscene
    // stages (SRoomEntry::EDimension::kCutscene) are left out: warping there shows a white screen.
    const auto isDark = [](int index) {
      return sRooms[index].dimension == SRoomEntry::EDimension::kDark;
    };
    static char sRoomFilter[64] = {};
    const float goWidth = ImGui::CalcTextSize("Reload").x + ImGui::GetStyle().FramePadding.x * 2.f;
    const float tagWidth = ImGui::GetCursorPosX() +
                           std::max(ImGui::CalcTextSize("Light").x, ImGui::CalcTextSize("Dark").x) +
                           ImGui::GetStyle().ItemSpacing.x;
    const auto roomCombo = [&](const char* tag, const char* id, bool dark) {
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(tag);
      ImGui::SameLine(tagWidth);
      ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - goWidth -
                              ImGui::GetStyle().ItemSpacing.x);
      const bool here = currentValid && isDark(current) == dark;
      if (ImGui::BeginCombo(id, here ? roomLabel(current).c_str() : "(pick a room)",
                            ImGuiComboFlags_HeightLarge)) {
#if !defined(PORT_MOBILE) // it would raise the on-screen keyboard every time
        if (ImGui::IsWindowAppearing()) {
          ImGui::SetKeyboardFocusHere();
        }
#endif
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##roomfilter", "Filter rooms", sRoomFilter, sizeof(sRoomFilter));
        for (int i = 0; i < areaCount; ++i) {
          if (!roomExists(i) || isDark(i) != dark ||
              sRooms[i].dimension == SRoomEntry::EDimension::kCutscene) {
            continue;
          }
          const std::string label = roomLabel(i);
          if (sRoomFilter[0] != '\0' && SDL_strcasestr(label.c_str(), sRoomFilter) == nullptr &&
              SDL_strcasestr(sRooms[i].label.c_str(), sRoomFilter) == nullptr) {
            continue;
          }
          ImGui::PushID(i);
          if (ImGui::Selectable(label.c_str(), i == current)) {
            goToRoom(i);
          }
          ImGui::SetItemTooltip("%s", sRooms[i].label.c_str());
          ImGui::PopID();
        }
        ImGui::EndCombo();
      }
      ImGui::SetItemTooltip(dark ? "Pick a Dark Aether room to go there. A room that is not loaded "
                                   "streams in first."
                                 : "Pick a Light Aether room to go there. A room that is not loaded "
                                   "streams in first.");
    };
    roomCombo("Light", "##room", false);
    ImGui::SameLine();
    ImGui::BeginDisabled(!currentValid);
    if (ImGui::Button("Reload")) {
      // A world warp to this room would put Samus at the room's spawn point (the elevator, for
      // most); the save-state route rebuilds the room and puts her back where she stands.
      PortSaveState::RequestRoomReload();
    }
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Rebuild the room from the disc, as leaving and coming back does.");
    roomCombo("Dark", "##darkroom", true);

    // Rebuilt when the card's table changes size, so loading another save does not leave the
    // combo showing the previous one's worlds.
    static size_t sWorldListCount = 0;
    static std::vector< std::pair< uint32_t, std::string > > sWorldList;
    const int worldCount =
        gpMemoryCard != nullptr ? static_cast< int >(gpMemoryCard->GetMemoryWorlds().size()) : 0;
    if (worldCount > 0 && static_cast< size_t >(worldCount) != sWorldListCount) {
      sWorldListCount = static_cast< size_t >(worldCount);
      sWorldList.clear();
      const rstl::vector< CMemoryCard::MemoryWorld >& worlds = gpMemoryCard->GetMemoryWorlds();
      for (int i = 0; i < worlds.size(); ++i) {
        const uint32_t id = static_cast< uint32_t >(worlds[i].first);
        std::string name = PlainWorldName(worlds[i].second.GetFrontEndName());
        // The six multiplayer arenas (Metroid6.pak, same ids in every release) and the front end
        // carry no name in the card's table (Front End: G2ME, G2MP, R3ME ids); MLVL names from the disc (tools/world_areas.py --list).
        static const struct {
          uint32_t id;
          const char* name;
        } kUnnamedWorlds[] = {
            {0xa50a80cc, "Multiplayer: Sidehopper Station"}, {0xae171602, "Multiplayer: Spires"},
            {0xe3b0c703, "Multiplayer: Crossfire Chaos"},    {0x233e42be, "Multiplayer: Pipeline"},
            {0x406add7f, "Multiplayer: Spider Complex"},     {0x7e19ed26, "Multiplayer: Shooting Gallery"},
            {0x69802220, "Front End"},                       {0x7b6eaa68, "Front End"},
            {0xdaea74a3, "Front End"},
        };
        for (const auto& known : kUnnamedWorlds) {
          if (name.empty() && known.id == id) {
            name = known.name;
          }
        }
        if (name.empty()) {
          char buf[32];
          std::snprintf(buf, sizeof(buf), "MLVL %08X", static_cast< unsigned >(id));
          name = buf;
        }
        sWorldList.emplace_back(id, name);
      }
    }
    if (sWorldList.empty()) {
      ImGui::TextDisabled("(the memory card's world table is not loaded yet)");
    } else {
      const char* currentName = "(another world)";
      for (const std::pair< uint32_t, std::string >& entry : sWorldList) {
        if (entry.first == sWorldId) {
          currentName = entry.second.c_str();
        }
      }
      ImGui::SetNextItemWidth(-FLT_MIN);
      if (ImGui::BeginCombo("##world", currentName)) {
        for (const std::pair< uint32_t, std::string >& entry : sWorldList) {
          ImGui::PushID(static_cast< int >(entry.first));
          if (ImGui::Selectable(entry.second.c_str(), entry.first == sWorldId)) {
            char text[16];
            std::snprintf(text, sizeof(text), "%08x", entry.first);
            PortWarp::World(text, "");
          }
          ImGui::PopID();
        }
        ImGui::EndCombo();
      }
      ImGui::SetItemTooltip("Pick a world to reload into its first room.");
    }
  }

  ImGui::SeparatorText("Room layers");
  ImGui::TextDisabled("The room's script layers, as the world's layer state has them.");
  ItemHelp("The game switches them for good as the story moves on (a boss fight turns its "
           "first-pass layer off when it starts), and the next save keeps them, so leaving a fight "
           "by a warp can leave its room without the enemy. Turning a layer on loads it now; "
           "turning one off takes effect when the room loads again (Reload).");
  if (sLayers.empty() || sLayersArea != sAreaId) {
    ImGui::TextDisabled("(no layer state for this room)");
  } else {
    for (int i = 0; i < static_cast< int >(sLayers.size()); ++i) {
      bool active = sLayers[i].active;
      char label[96];
      std::snprintf(label, sizeof(label), "%d: %s##layer%d", i, sLayers[i].name.c_str(), i);
      if (ImGui::Checkbox(label, &active)) {
        if (active) {
          PortWarp::Layer(i);
        } else {
          PortWarp::LayerOff(i);
        }
      }
    }
  }
}

} // namespace PortDebug
