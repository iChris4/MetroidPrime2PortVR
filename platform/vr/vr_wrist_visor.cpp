// SPDX-License-Identifier: GPL-3.0-or-later
#include "vr/vr_wrist_visor.h"
#include "vr/vr_view.h"
#include "vr/vr_pad.h"
#include "vr/vr_settings.h"
#include "GuiSys/CGuiModel.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Graphics/CModel.hpp"
#include "Kyoto/Graphics/CModelFlags.hpp"
#include "Kyoto/Math/CQuaternion.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/CCameraManager.hpp"
#include "MetroidPrime/Cameras/CGameCamera.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include <algorithm>

namespace PortVr {
// PrimedGun Echoes: the wrist's selection runs in vr_pad.cpp; its icons are the HUD visor menu's
// models, which the Metroid Prime port reaches through CGuiModel additions this port does not
// have yet (docs/PRIMEDGUN.md, Phase 4), so nothing is drawn on the wrist. The setting is off by
// default.
void VrWristCaptureModels(CGuiModel* const*, CGuiModel* const*) noexcept {}
void VrWristForgetModel(const CGuiModel*) noexcept {}
void VrWristVisorDraw(const CStateManager&) noexcept {}
} // namespace PortVr
