// SPDX-License-Identifier: GPL-3.0-or-later

import Foundation

/// What makes this app Echoes rather than Metroid Prime. The two PrimedGun apps share every other
/// Swift file of visionos/App; only this one differs between the repositories.
enum GameInfo {
    static let title = "PrimedGun Echoes"
    static let gameName = "Metroid Prime 2: Echoes"
    /// The disc the game plays, as the player is told.
    static let discDescription =
        "Your own Metroid Prime 2: Echoes GameCube disc image: USA (G2ME01). PAL (G2MP01) and Metroid Prime Trilogy (R3ME01, R3MP01) are experimental."
    /// The disc image extensions the port opens (launcher/core/disc_probe.cpp IsSupportedDiscExtension).
    static let discExtensions: Set<String> = ["iso", "gcm", "rvz", "wia", "gcz", "ciso", "wbfs"]
    /// Shown under the Play button.
    static let firstSteps =
        "Once in the game, click the right stick to set your height and the left stick to open or close the VR menu. Press the Digital Crown to leave the game."
    /// The About tab's credits.
    static let credits =
        "PrimedGun Echoes: iChris4. PrimedGun's VR experience and controls: Nobbie and iChris4. Echoes Unbound, the native port: Odrannnn and its contributors, from the PrimeDecomp Echoes decompilation. The Apple Vision Pro layer comes from Wiicompiled VR."
}
