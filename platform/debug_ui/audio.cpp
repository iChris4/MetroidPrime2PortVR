// F1 overlay: Audio backends and the voices list, the Prime 1 port's debug_ui/audio.cpp for this
// game. State and helpers shared with the other parts are in debug_ui_internal.h.

#include "debug_ui_internal.h"

#include "port_debug.h"

#include <imgui.h>
#include <musyx/port_voices.h>

#include <algorithm>
#include <vector>

extern "C" void AIPortSetOutputEnabled(int enabled);
extern "C" int AIPortOutputEnabled(void);
extern "C" void salSetMuted(int muted);

namespace PortDebug {

namespace {
bool sMusyxEnabled = true;
} // namespace

// The two audio paths' switches (declared in port_debug.h). Neither is saved: a relaunch has
// sound.
bool AiAudioEnabled() {
  return AIPortOutputEnabled() != 0;
}

void SetAiAudioEnabled(bool enabled) {
  AIPortSetOutputEnabled(enabled ? 1 : 0);
}

bool MusyxAudioEnabled() {
  return sMusyxEnabled;
}

void SetMusyxAudioEnabled(bool enabled) {
  sMusyxEnabled = enabled;
  salSetMuted(enabled ? 0 : 1);
}

void DrawAudio() {
  CheckboxFn("Streamed audio (music/movies)", AiAudioEnabled, SetAiAudioEnabled);
  CheckboxFn("MusyX audio (effects/streams)", MusyxAudioEnabled, SetMusyxAudioEnabled);
}

void DrawVoices() {
  struct Agg {
    PortMusyxVoice voice;
    int instances;
  };

  // Held between frames so "Freeze list" can keep the last collection on screen.
  // Ordered by id, not loudness: sorting by rms made rows swap places every
  // frame as the levels moved, which made the checkboxes hard to hit and hid
  // which sample a row belonged to.
  static std::vector< Agg > aggs;
  static bool freeze = false;

  ImGui::Checkbox("Freeze list", &freeze);
  ImGui::SameLine();
  ImGui::TextUnformatted("(hold the current list so its rows stay put)");
  ImGui::SetItemTooltip(
      "Freezing keeps the rows shown right now so you can read them. Mute still works\n"
      "on any sample, playing or not. Untick to follow live voices again. Not saved.");

  if (!freeze) {
    aggs.clear();
    PortMusyxVoice voices[64];
    const int count = MusyxPortCopyVoices(voices, 64);
    for (int i = 0; i < count; ++i) {
      bool found = false;
      for (Agg& agg : aggs) {
        if (agg.voice.smpId == voices[i].smpId) {
          ++agg.instances;
          if (voices[i].rms > agg.voice.rms) {
            agg.voice = voices[i];
          }
          found = true;
          break;
        }
      }
      if (!found) {
        aggs.push_back(Agg{voices[i], 1});
      }
    }
    std::sort(aggs.begin(), aggs.end(),
              [](const Agg& a, const Agg& b) { return a.voice.smpId < b.voice.smpId; });
  }

  ImGui::TextUnformatted("Active samples, lowest id first. Mute one to isolate it.");
  if (aggs.empty()) {
    ImGui::TextUnformatted(freeze ? "No samples in the frozen list." : "No active MusyX voices.");
  }
  for (const Agg& agg : aggs) {
    ImGui::PushID(static_cast< int >(agg.voice.smpId));
    bool muted = MusyxPortIsSampleMuted(agg.voice.smpId) != 0;
    if (ImGui::Checkbox("##mute", &muted)) {
      MusyxPortSetSampleMuted(agg.voice.smpId, muted ? 1 : 0);
    }
    ImGui::SameLine();
    ImGui::Text("smp %u  %s  len %u  pitch %u  rms %d  vol %u/%u  x%d", agg.voice.smpId,
                agg.voice.looped ? "loop" : "one-shot", agg.voice.length, agg.voice.pitch,
                agg.voice.rms, agg.voice.volL, agg.voice.volR, agg.instances);
    ImGui::PopID();
  }
  // Deliberately not behind the empty-list case above: with nothing playing
  // there is still a mute set to clear. Also turns both audio paths back on, so
  // nothing stays silent. None of these mutes is saved: a relaunch has sound.
  if (ImGui::Button("Unmute all")) {
    MusyxPortClearSampleMutes();
    SetAiAudioEnabled(true);
    SetMusyxAudioEnabled(true);
  }
}

} // namespace PortDebug
