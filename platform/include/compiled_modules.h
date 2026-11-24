// The port's registry of REL modules that are compiled into the game library
// rather than loaded from the disc. See compiled_modules.cpp for why it exists.

#pragma once

#include <cstddef>

namespace port::modules {

// How many modules are compiled in, and the one-time init of all of them, in
// retail module order. Called from platform/main.cpp just before InvokeCMain.
size_t Count();
void InitAll();

// Reverse-order teardown, so a module goes down before whatever it initialised.
void ShutdownAll();

// One module's init and shutdown, by the path retail's module manager holds
// ("RelProd/Tweaks.rel"). They stand in for the prolog and epilog the cube calls
// inside the linked image. Prolog runs the init only if it has not run; Epilog runs
// the shutdown only if it has, so neither doubles up with InitAll/ShutdownAll. A
// module not compiled in has no host code at all, and both abort with its name.
void Prolog(const char* path);
void Epilog(const char* path);

} // namespace port::modules

// C entry points, used by src/MetroidPrime/CRelFile.cpp's link and unlink.
extern "C" void port_modules_prolog(const char* path);
extern "C" void port_modules_epilog(const char* path);
