// The Android touch overlay's virtual gamepad, minus the Android parts.
//
// This logic used to live inside `#if defined(__ANDROID__)` in
// platform/debug_ui.cpp, which meant it was never compiled anywhere except an
// Android build and had no test at all: a wrong button mapping or a broken axis
// conversion could not fail on a desktop build, only on a phone, where nobody
// was running the tests. It is all platform-independent, so it lives here and
// tests/touch_pad.cpp drives it on the host.
//
// In MP2 the Android caller is platform/port_touch.cpp.
// What is NOT here is the state-sampling limitation of SDL's virtual joystick
// (a tap that begins and ends between two updates is never seen by the game).
// That is a property of SDL, not of this code, and it is documented where the
// Android code lives.

#ifndef PORT_TOUCH_PAD_H
#define PORT_TOUCH_PAD_H

#include <SDL3/SDL.h>

namespace PortTouchPad {

// An attached virtual pad, and the id needed to detach it.
struct Pad {
  SDL_JoystickID id = 0;
  SDL_Joystick* handle = nullptr;

  bool ok() const { return handle != nullptr; }
};

// Fills `desc` with a full-size gamepad that reports as a common Xbox pad, so
// the pad type - and with it the port's prompt icons - matches what the touch
// overlay draws.
void Describe(SDL_VirtualJoystickDesc& desc);

// Normalised -1..1 deflection to an SDL joystick axis value.
//
// The overlay sends a float but the virtual joystick API takes a Sint16.
// Passing the float straight through truncated every partial deflection to 0,
// so the sticks and triggers read as centred and the player could neither move
// nor aim. SDL's axes run -32768..32767, and a trigger's rest value is the
// minimum rather than the centre, so the range is scaled asymmetrically.
Sint16 AxisValue(float deflection);

// Attaches a virtual pad, opens it, and leaves the mapping to SDL.
//
// ON THE MAPPING. This used to install a hand-written 512-byte mapping string
// naming every control the overlay sends. It worked - and writing
// tests/touch_pad.cpp is what showed that it was also *incomplete*: it named
// b0-b4 and b9-b14, while SDL's enum has 26 buttons, so Guide, both stick
// clicks and the paddles were unmapped and a write to any of them went nowhere.
//
// The string was never doing the job it claimed. A virtual device gets a GUID
// with the virtual bus and a 'v' signature, so the vendor and product ids set by
// Describe never selected a built-in table by GUID match either; SDL routes an
// unmatched virtual device to the virtual driver's own generated mapping, and
// that is exactly this enum order (SDL_virtualjoystick.c:803, :933). So the ids
// and the string were both decoration. Removing the string was measured rather
// than assumed: with it disabled, every button index still reached its own
// logical control, which is what tests/touch_pad.cpp checks.
//
// Letting SDL supply the mapping is strictly better than the string. A
// hand-written table is one more thing that can disagree with SDL and it can
// only fall behind - add a button to the enum and the string silently drops it.
// The generated mapping is derived from the enum, which is SDL's public ABI. The
// test is what pins the behaviour, so a change in SDL now fails a build instead
// of quietly misplacing a button on a phone.
//
// One consequence worth knowing: the vendor and product ids in Describe are
// cosmetic. They shape what the pad is *called* and any prompt logic that keys
// off them, but they do not select a mapping. If a mapping is ever needed, add
// it in Attach with SDL_AddGamepadMapping and extend the test to match.
//
// If the open fails after the attach succeeded, the pad is detached again
// before returning. Otherwise the id would be dropped while the caller's handle
// stayed null, and every later call would attach another device: SDL keeps
// virtual devices in a linked list with no small fixed limit, so that
// accumulates rather than hitting a ceiling. Reaching it needs an allocation
// failure inside SDL, which makes this a robustness fix rather than an
// ordinary-play bug, but it is four lines and the alternative is a slow leak on
// the one path that is already reporting an error.
Pad Attach();

// Detaches and closes, leaving `pad` cleared. Safe on a pad that never opened,
// and safe to call twice.
void Detach(Pad& pad);

} // namespace PortTouchPad

#endif // PORT_TOUCH_PAD_H
