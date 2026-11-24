#ifndef METROID_PRIME_PORT_PORTPHAZONSHOTSFX_H
#define METROID_PRIME_PORT_PORTPHAZONSHOTSFX_H

// Port-only: the sounds of the Phazon shot for the "Phazon shot anywhere" cheat.
//
// The charge loop, fire and hit sounds are in the SamusPhazonGun audio group, which only
// Metroid1.pak holds and only the final Dark Samus fight's area loads, so anywhere else the cheat's
// shot was silent. `Ensure` holds the group while the cheat is on: through the resource pool where
// the mounted paks have it (shared with the area's own copy), else read straight from Metroid1.pak.
// `Release` drops it (cheat off, or the gun going away with its world).
namespace PortPhazonShotSfx {
void Ensure();
void Release();
} // namespace PortPhazonShotSfx

#endif
