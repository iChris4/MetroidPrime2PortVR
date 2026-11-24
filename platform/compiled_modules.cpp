// The port's registry of REL modules that are compiled into the game library
// rather than loaded from the disc.
//
// The other modules are read out of the ISO and relocated by platform/rel.cpp,
// which calls each module's prolog and epilog by guest address inside the loaded
// image. The ones listed below are not: we have our own sources for them in src/,
// and those are compiled straight into mp_game. That is why they need this file.
//
// On the cube each of them is a separate module with its own RELMain and
// RELExit. On the host, CMake builds each module as its own symbol namespace
// (tools/scope_module.py): RELMain/RELExit become mp_rel_<Module>_prolog/_epilog and the
// rest of the module's symbols are local. This table, generated from those names, is how
// the port entry finds and runs them, before handing control to the game.

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define MP_MODULE_DECLS
#include "compiled_modules.inc"
#undef MP_MODULE_DECLS

namespace port::modules {

namespace {

struct CompiledModule {
  const char* name;
  // The module's file on the disc, without the directory and the ".rel": the name
  // `configure.py`'s `Rel(...)` block gives it, and the one retail's module manager
  // holds in its record (`RelProd/Tweaks.rel` and so on). Prolog/Epilog match on it.
  const char* file;
  void (*init)();
  void (*shutdown)();
  // True when the module is only the shared entry-point stub (tools/modules_port.txt): it loads,
  // so the area works, but it has no host body and none of its actors will appear.
  bool stub;
};

// Generated at build time (tools/scope_module.py registry): one row per compiled module that
// defines RELMain, in configure.py's module order, with its RELExit if it has one. The names are
// the mp_rel_<Module>_prolog/_epilog that the same tool gives the module's entry points when it
// scopes the module's symbols, so the registry cannot drift from what the modules define.
// `name` is also the module's file on the disc (configure.py's Rel("<Module>") name).
#include "compiled_modules.inc"

// Whether each module's init has run and its shutdown has not, indexed like
// kCompiledModules. InitAll runs every init at port entry, and retail's module
// manager then asks for a module's prolog when it links the image and its epilog
// when it unlinks it (src/MetroidPrime/CRelFile.cpp). The flag is what keeps
// both callers honest: a prolog of a module InitAll already started is a no-op, and
// an epilog runs the shutdown once and lets a later prolog run the init again.
bool gInitialised[sizeof(kCompiledModules) / sizeof(kCompiledModules[0])];

// `path` is what retail's record holds: an optional directory, the file, ".rel".
// Returns the table index, or -1 for a module that is not compiled in.
int Find(const char* path) {
  const char* file = std::strrchr(path, '/');
  file = file != nullptr ? file + 1 : path;
  size_t length = std::strlen(file);
  if (length > 4 && std::strcmp(file + length - 4, ".rel") == 0) {
    length -= 4;
  }
  for (size_t i = 0; i < sizeof(kCompiledModules) / sizeof(kCompiledModules[0]); ++i) {
    const char* candidate = kCompiledModules[i].file;
    if (std::strlen(candidate) == length && std::strncmp(candidate, file, length) == 0) {
      return static_cast< int >(i);
    }
  }
  return -1;
}

// Every module retail can ask for that this table lacks is PowerPC code only: the port
// has no host body to run, and relocating the disc image on the host would give it
// PowerPC instructions it cannot execute. So it is a declared stop, not a no-op that
// would leave the module's loaders unregistered with nothing reporting it.
int FindOrAbort(const char* path, const char* what) {
  const int index = Find(path);
  if (index < 0) {
    std::printf("metroid_prime2_port: module %s: %s is not compiled into the port - "
                "no host code to run\n", what, path);
    std::fflush(nullptr);
    std::abort();
  }
  return index;
}

} // namespace

size_t Count() { return sizeof(kCompiledModules) / sizeof(kCompiledModules[0]); }

void InitAll() {
  for (size_t i = 0; i < Count(); ++i) {
    std::printf("metroid_prime2_port: init compiled module %s\n", kCompiledModules[i].name);
    kCompiledModules[i].init();
    gInitialised[i] = true;
  }
}

void ShutdownAll() {
  // Reverse order, so a module is torn down before whatever it initialised.
  for (size_t i = Count(); i-- > 0;) {
    // A module need not define an exit point - CScriptPlayerActorMain.cpp has a
    // RELMain and no RELExit - so the table can hold a null shutdown.
    if (gInitialised[i] && kCompiledModules[i].shutdown != nullptr) {
      kCompiledModules[i].shutdown();
    }
    gInitialised[i] = false;
  }
}

void Prolog(const char* path) {
  const int i = FindOrAbort(path, "prolog");
  if (!gInitialised[i]) {
    kCompiledModules[i].init();
    gInitialised[i] = true;
  }
}

void Epilog(const char* path) {
  const int i = FindOrAbort(path, "epilog");
  if (gInitialised[i] && kCompiledModules[i].shutdown != nullptr) {
    kCompiledModules[i].shutdown();
  }
  gInitialised[i] = false;
}

} // namespace port::modules

extern "C" void port_modules_prolog(const char* path) { port::modules::Prolog(path); }

extern "C" void port_modules_epilog(const char* path) { port::modules::Epilog(path); }
