// SPDX-License-Identifier: MIT
// Derived from PrimedGun's Quest launcher (features/primedgun/ui/PrimedGunRefreshable.kt,
// Copyright 2026 PrimedGun Project).
package org.primedgun.echoes.launcher

/**
 * A launcher tab whose widgets mirror settings. The launcher calls [refresh] after
 * something rewrote many settings at once (Reset All, a PrimedGun settings import, the
 * game's own rewrite of port_settings.ini) or when the game starts or stops, so the
 * visible tab re-reads them and its locked state instead of showing stale values.
 */
interface PrimedGunRefreshable {
    fun refresh()
}
