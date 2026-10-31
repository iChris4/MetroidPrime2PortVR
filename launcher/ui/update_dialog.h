// SPDX-License-Identifier: MIT
//
// Setup > Check for updates: the update dialog (update_dialog.cpp).
#pragma once

namespace PrimedGunLauncher {
class TabContext;
// Opens the update dialog (or brings the open one forward); it checks GitHub at once.
void CheckForUpdates(TabContext& ctx);
} // namespace PrimedGunLauncher
